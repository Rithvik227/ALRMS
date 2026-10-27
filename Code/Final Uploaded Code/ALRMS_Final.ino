// This combined full system code for ALRMS has been written with some assistance from Gemini.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 
#include <AccelStepper.h> 

// LCD I2C pins
const int SDA_pin = A4; 
const int SCL_pin = A5;

// Photoresistor
const int photoresistor_pin = A3;

// limit switch pins
const int limit_switch_home_pin = 7;
const int limit_switch_end_pin = 8; 

// Buttons pins
const int start_stop_button_pin = 9;
const int home_button_pin = 10;
const int manual_button_pin = 11;
const int auto_button_pin = 12;

// Stepper Pins
const int EN = 4;
const int DIR = 5;
const int STEP = 6;

// Joystick Pins
const int joystick_select_pin = A0; // Joystick click button
const int joystick_horiz_pin = A1;
const int joystick_vert_pin = A2;

const int LEFT_THRESHOLD = 400;
const int RIGHT_THRESHOLD = 800;
const int UP_THRESHOLD = 400;
const int DOWN_THRESHOLD = 800;

hd44780_I2Cexp lcd; 
AccelStepper stepper(1, STEP, DIR); 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

const int debounceDelay = 200;

// Core System Variables
bool isRunning = false;
bool isManual = false;
bool isAuto = true;
bool isDark = false;
bool isAsleep = false;
bool isEmergencyStopped = false; 
bool isSettingsMode = false; 

int lastStartStopState = HIGH;
int lastHomeButtonState = HIGH;
int lastManualButtonState = HIGH;
int lastAutoButtonState = HIGH;
int lastJoystickButtonState = HIGH;
int lcdPage = 0;

// Dynamic Track & Motor Variables
long max_track_length = 36000; 
float system_max_speed = 2400.0; 
float system_max_accel = 5000.0;

unsigned long darkStartTime = 0;
unsigned long manual_motion_time = 0;
unsigned long lastLcdUpdate = 0;
unsigned long lastPageChange = 0;
unsigned long lastButtonPressTime = 0;
 

void setup() {
  randomSeed(generate_true_noise_seed());
  
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if (status != 0) {
    while (true) {
      // System failed to start LCD, freeze program
    }
  }

  pinMode(limit_switch_home_pin, INPUT_PULLUP);
  pinMode(limit_switch_end_pin, INPUT_PULLUP);
  
  pinMode(start_stop_button_pin, INPUT_PULLUP);
  pinMode(home_button_pin, INPUT_PULLUP);
  pinMode(manual_button_pin, INPUT_PULLUP);
  pinMode(auto_button_pin, INPUT_PULLUP);
  pinMode(joystick_select_pin, INPUT_PULLUP); 

  pinMode(EN, OUTPUT);
  digitalWrite(EN, LOW); 

  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("System Boot...  ");
  delay(1500); 
  
  calibrateSystem(); 
}

void loop() {
  // Check if system is asleep first. If it is, read all buttons to see if any can wake it up.
  if (isAsleep == true) {
    int checkStart = digitalRead(start_stop_button_pin);
    int checkHome = digitalRead(home_button_pin);
    int checkManual = digitalRead(manual_button_pin);
    int checkAuto = digitalRead(auto_button_pin);
    int checkJoystick = digitalRead(joystick_select_pin);

    if (checkStart == LOW || checkHome == LOW || checkManual == LOW || checkAuto == LOW || checkJoystick == LOW) {
      wake_up();
      delay(debounceDelay); // Give a short pause so it doesn't double-trigger actions
    }
    return; // Don't run the rest of the loop while asleep
  }

  int photoresistor_value = analogRead(photoresistor_pin);

  if (photoresistor_value < 250) {
    if (isDark == false) {
      darkStartTime = millis();
      isDark = true;
      lcdPage = 1; 
      lastPageChange = millis();
      lcd.clear(); 
    } 
    else if (millis() - darkStartTime >= 30000 && isSettingsMode == false) {
      disable_all_activity();
    } 
  } 
  else {
    isDark = false; 
  }

  int currentStartStopButtonState = digitalRead(start_stop_button_pin);
  int currentHomeButtonState = digitalRead(home_button_pin);
  int currentManualButtonState = digitalRead(manual_button_pin);
  int currentAutoButtonState = digitalRead(auto_button_pin);
  int currentJoystickButtonState = digitalRead(joystick_select_pin);

  if (millis() - lastButtonPressTime > debounceDelay) {
    
    // Joystick Settings Toggle
    if (currentJoystickButtonState == LOW && lastJoystickButtonState == HIGH) {
      isSettingsMode = !isSettingsMode;
      if (isSettingsMode == true) {
        isRunning = false; 
        stepper.stop();
      }
      lastButtonPressTime = millis();
      lcd.clear();
    }

    if (isSettingsMode == false) {
      if (currentStartStopButtonState == LOW && lastStartStopState == HIGH) {
        isEmergencyStopped = false;
        isRunning = !isRunning; 
        if (isRunning == false) { 
          stepper.stop(); 
        }
        lastButtonPressTime = millis(); 
        lcdPage = 0; 
        lastPageChange = millis(); 
        lcd.clear();
      }
      
      if (currentHomeButtonState == LOW && lastHomeButtonState == HIGH) {
        isEmergencyStopped = false; 
        calibrateSystem();
        lastButtonPressTime = millis();
        lcdPage = 0; 
        lastPageChange = millis(); 
        lcd.clear();
      }

      if (currentManualButtonState == LOW && lastManualButtonState == HIGH) {
        isManual = true; 
        isAuto = false; 
        isRunning = false;
        lastButtonPressTime = millis();
        lcdPage = 0; 
        lastPageChange = millis(); 
        lcd.clear();
      }
      
      if (currentAutoButtonState == LOW && lastAutoButtonState == HIGH) {
        isAuto = true; 
        isManual = false; 
        isRunning = false;
        lastButtonPressTime = millis();
        lcdPage = 0; 
        lastPageChange = millis(); 
        lcd.clear();
      }
    }
  }

  lastStartStopState = currentStartStopButtonState;
  lastHomeButtonState = currentHomeButtonState;
  lastManualButtonState = currentManualButtonState;
  lastAutoButtonState = currentAutoButtonState;
  lastJoystickButtonState = currentJoystickButtonState;

  if (isSettingsMode == true) {
    handle_settings();
  } 
  else {
    if (isAuto == true) {
      auto_motion(); 
    } 
    else if (isManual == true && isRunning == true) {
      int joystick_xValue = analogRead(joystick_horiz_pin);
      int joystick_yValue = analogRead(joystick_vert_pin);
      manual_motion(joystick_xValue, joystick_yValue);
    }

    if (millis() - lastLcdUpdate >= 500 && isEmergencyStopped == false) {
      update_dashboard();
      lastLcdUpdate = millis();
    }

    check_limit_switches();
  }
}

// Helper/Custom Functions

void calibrateSystem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating...  "); 
  
  // 1. Find Home (Fast)
  lcd.setCursor(0, 1);
  lcd.print("1. Finding Home "); 
  stepper.setMaxSpeed(2400.0);
  stepper.setAcceleration(1500.0);
  stepper.setSpeed(-1000); 

  while (digitalRead(limit_switch_home_pin) == HIGH) {
    if (digitalRead(start_stop_button_pin) == LOW || digitalRead(manual_button_pin) == LOW) { 
      return; 
    }
    stepper.runSpeed();
  }
  
  stepper.setCurrentPosition(stepper.currentPosition());
  delay(200);
  stepper.moveTo(stepper.currentPosition() + 1000);
  while (stepper.distanceToGo() != 0) { 
    stepper.run(); 
  }
  stepper.setCurrentPosition(0); 

  // 2. Find End (Fast)
  lcd.setCursor(0, 1);
  lcd.print("2. Finding End  "); 
  stepper.setMaxSpeed(2400.0);
  stepper.setAcceleration(1500.0);
  stepper.setSpeed(1000); 
  
  while (digitalRead(limit_switch_end_pin) == HIGH) {
    if (digitalRead(start_stop_button_pin) == LOW || digitalRead(manual_button_pin) == LOW) {
      return; 
    }
    stepper.runSpeed();
  }

  stepper.setCurrentPosition(stepper.currentPosition());
  delay(200);
  
  max_track_length = stepper.currentPosition() - 1000;
  stepper.moveTo(max_track_length);
  
  while (stepper.distanceToGo() != 0) {
    stepper.run(); 
  }

  // 3. Move to Center 
  lcd.setCursor(0, 1);
  lcd.print("3. Centering... "); 
  stepper.moveTo(max_track_length / 2);
  while (stepper.distanceToGo() != 0) { 
    stepper.run(); 
  }

  isRunning = false;

  // Force system to show the Settings Hint immediately after centering
  lcdPage = 3; 
  lastPageChange = millis();
  lcd.clear();
}

void handle_settings() {
  int jx = analogRead(joystick_horiz_pin);
  int jy = analogRead(joystick_vert_pin);

  if (jy < UP_THRESHOLD && system_max_speed < 8000) { 
    system_max_speed = system_max_speed + 50; 
    delay(150); 
  }
  if (jy > DOWN_THRESHOLD && system_max_speed > 500) { 
    system_max_speed = system_max_speed - 50; 
    delay(150); 
  }
  
  // SWAPPED DIRECTIONS: Left now increases max acceleration, Right decreases it
  if (jx < LEFT_THRESHOLD && system_max_accel < 15000) { 
    system_max_accel = system_max_accel + 100; 
    delay(150); 
  }
  if (jx > RIGHT_THRESHOLD && system_max_accel > 500) { 
    system_max_accel = system_max_accel - 100; 
    delay(150); 
  }

  if (millis() - lastLcdUpdate >= 200) {
    lcd.setCursor(0, 0);
    lcd.print("SET SPD: ");
    lcd.print((int)system_max_speed);
    lcd.print("        "); 
    lcd.setCursor(0, 1);
    lcd.print("SET ACC: ");
    lcd.print((int)system_max_accel);
    lcd.print("        ");
    lastLcdUpdate = millis();
  }
}

void manual_motion(int joystick_x, int joystick_y) {
  float ratio = 0.0;
  long direction = 0;

  if (joystick_y < UP_THRESHOLD) {
    ratio = (float)(UP_THRESHOLD - joystick_y) / (float)UP_THRESHOLD;
    direction = 1; 
  } 
  else if (joystick_y > DOWN_THRESHOLD) {
    ratio = (float)(joystick_y - DOWN_THRESHOLD) / (float)(1023 - DOWN_THRESHOLD);
    direction = -1; 
  }

  if (direction != 0) {
    float curve = ratio * ratio * ratio * ratio;

    float targetSpeed = curve * system_max_speed;
    if (targetSpeed < 100.0) {
      targetSpeed = 100.0; 
    }

    float targetAccel = curve * system_max_accel;
    if (targetAccel < 100.0) {
      targetAccel = 100.0; 
    }

    stepper.setMaxSpeed(targetSpeed);
    stepper.setAcceleration(targetAccel);

    stepper.moveTo(stepper.currentPosition() + (direction * 10000));
    stepper.run();
  } 
  else {
    if (stepper.distanceToGo() != 0) {
      stepper.stop(); 
      stepper.run();
    }
  }

  manual_motion_time = millis();
}

unsigned long generate_true_noise_seed() {
  unsigned long random_seed = 0;
  for (int i = 0; i < 32; i++) {
    int noise_reading = analogRead(photoresistor_pin);
    int random_bit = noise_reading & 1; 
    random_seed = (random_seed << 1) | random_bit; 
    delay(2); 
  }
  return random_seed;
}

void check_limit_switches() {
  if (digitalRead(limit_switch_end_pin) == LOW) { 
    trigger_emergency_bounce("End Sw: Pressed ", -1000); 
  }
  if (digitalRead(limit_switch_home_pin) == LOW) { 
    trigger_emergency_bounce("Home Sw: Pressed", 1000); 
  }
}

void trigger_emergency_bounce(String lcdMessage, int bounceDistance) {
  if (isRunning == true) {  
    lcd.clear(); 
    lcd.setCursor(0, 0); 
    lcd.print(lcdMessage);
    lcd.setCursor(0, 1); 
    lcd.print("Bouncing back...");

    stepper.setMaxSpeed(system_max_speed * 0.75);
    stepper.setAcceleration(system_max_accel * 0.75);

    stepper.setCurrentPosition(stepper.currentPosition());
    delay(200); 
    
    stepper.runToNewPosition(stepper.currentPosition() + bounceDistance);

    isRunning = false; 
    isEmergencyStopped = false;  

    lcd.clear(); 
    lcd.setCursor(0, 0); 
    lcd.print("E-Stop Cleared  ");
    lcd.setCursor(0, 1); 
    lcd.print("System Paused   ");
    delay(2000); 
  }
}

void auto_motion() {
  if (isRunning == true) {
    stepper.setMaxSpeed(system_max_speed * 0.75);
    stepper.setAcceleration(system_max_accel * 0.75);

    if (stepper.distanceToGo() == 0) {
      long randomPosition = random(0, max_track_length); 
      stepper.moveTo(randomPosition);
    }
  }
  stepper.run();
}

void disable_all_activity() {
  isAsleep = true; 
  isRunning = false; 
  stepper.stop();
  lcd.clear(); 
  lcd.setCursor(0, 1); 
  lcd.print("Turning off.    ");
  delay(2000); 
  lcd.clear(); 
  lcd.print("System Asleep   ");
  delay(3000); 
  lcd.noBacklight(); 
  digitalWrite(EN, HIGH);
}

void wake_up() {
  isAsleep = false; 
  digitalWrite(EN, LOW); 
  lcd.backlight();
  lcd.clear(); 
  lcd.setCursor(0, 1); 
  lcd.print("System Awake    ");
  darkStartTime = millis();
}

void update_dashboard() {
  if (millis() - lastPageChange >= 5000) {
    if (isDark == true) { 
      lcdPage = 1; 
    } 
    else {
      lcdPage++;
      if (lcdPage > 3) { 
        lcdPage = 0; 
      }
    }
    lastPageChange = millis();
    lcd.clear(); 
  }

  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    if (isAuto == true) { 
      lcd.print("Mode: AUTO      "); 
    } 
    else if (isManual == true) { 
      lcd.print("Mode: MANUAL    "); 
    }

    lcd.setCursor(0, 1);
    if (isRunning == false) { 
      lcd.print("Status: PAUSED  "); 
    } 
    else { 
      lcd.print("Pos: "); 
      lcd.print(stepper.currentPosition()); 
      lcd.print("          "); 
    }
  } 
  else if (lcdPage == 1) {
    lcd.setCursor(0, 0); 
    lcd.print("Light Lvl: "); 
    lcd.print(analogRead(photoresistor_pin)); 
    lcd.print("     "); 

    lcd.setCursor(0, 1);
    if (isDark == true) {
      int secondsLeft = 30 - ((millis() - darkStartTime) / 1000);
      if (secondsLeft < 0) {
        secondsLeft = 0; 
      }
      lcd.print("Sleep in: "); 
      lcd.print(secondsLeft); 
      lcd.print("s   ");
    } 
    else { 
      lcd.print("Light: Good     "); 
    }
  } 
  else if (lcdPage == 2) {
    lcd.setCursor(0, 0);
    if (isAuto == true && isRunning == true) { 
      lcd.print("Target: "); 
      lcd.print(stepper.targetPosition()); 
      lcd.print("      "); 
    } 
    else { 
      lcd.print("Target: N/A     "); 
    }

    lcd.setCursor(0, 1);
    if (isAuto == true && isRunning == true) { 
      lcd.print("Dist: "); 
      lcd.print(stepper.distanceToGo()); 
      lcd.print("        "); 
    } 
    else if (isManual == true && isRunning == true) { 
      lcd.print("Dist: N/A       "); 
    } 
    else { 
      lcd.print("Motor Idle      "); 
    }
  } 
  else if (lcdPage == 3) {
    lcd.setCursor(0, 0);
    lcd.print("Press Joystick  ");
    lcd.setCursor(0, 1);
    lcd.print("For Settings    ");
  }
}