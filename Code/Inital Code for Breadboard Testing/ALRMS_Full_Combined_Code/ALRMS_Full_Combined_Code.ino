// This combined full system code for ALRMS has been written with some assistance from Gemini 3.1 Pro.

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
const int limit_switch_home_pin = 8;
const int limit_switch_end_pin = 9;

// Buttons pins
const int start_stop_button_pin = 10;
const int home_button_pin = 11;
const int manual_button_pin = 12;
const int auto_button_pin = 13;

// Stepper Pins
const int EN = 5;
const int DIR = 6;
const int STEP = 7;

// Joystick Pins
const int joystick_select_pin = A0;
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

const int ANALOG_X_CORRECTION = 128; 
const int ANALOG_Y_CORRECTION = 128; 

const int debounceDelay = 200;

bool isRunning = false;
bool isManual = false;
bool isAuto = true;
bool isDark = false;
bool isAsleep = false;

int lastStartStopState = HIGH;
int lastHomeButtonState = HIGH;
int lastManualButtonState = HIGH;
int lastAutoButtonState = HIGH;
int lcdPage = 0;

unsigned long darkStartTime = 0;
unsigned long manual_motion_time = 0;
unsigned long lastLcdUpdate = 0;
unsigned long lastPageChange = 0;
unsigned long lastButtonPressTime = 0;
 

void setup() {
  randomSeed(generate_true_noise_seed());
  
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  
  if(status) {
    while(1); 
  }

  pinMode(limit_switch_home_pin, INPUT_PULLUP);
  pinMode(limit_switch_end_pin, INPUT_PULLUP);
  
  pinMode(start_stop_button_pin, INPUT_PULLUP);
  pinMode(home_button_pin, INPUT_PULLUP);
  pinMode(manual_button_pin, INPUT_PULLUP);
  pinMode(auto_button_pin, INPUT_PULLUP);

  pinMode(photoresistor_pin, INPUT_PULLUP);

  pinMode(EN, OUTPUT);
  digitalWrite(EN, LOW);

  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(400.0);
  
  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("System Boot...");
  delay(1500); 
  
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Test: Joystick");
}

void loop() {
  // checks if lights in the room are dim to turn off after 30s of dark
  int photoresistor_value = analogRead(photoresistor_pin);

  if (photoresistor_value < 250) {
    if (isDark == false) {
      darkStartTime = millis();
      isDark = true;
    } else if (millis() - darkStartTime >= 30000 && isAsleep == false) {
      disable_all_activity();
    } 
  } else {
    isDark = false; 
  }

  // Checks start/stop button
  int currentStartStopButtonState = digitalRead(start_stop_button_pin);
  
  if (currentStartStopButtonState == LOW && lastStartStopState == HIGH) {
    if (isAsleep) {
      wake_up();
    } else {
      isRunning = !isRunning; 
      if (isRunning == false) {
        stepper.stop(); // Calculates the deceleration curve instantly
      }
    }
  }
  lastStartStopState = currentStartStopButtonState;

  // this stops the loop if the system is asleep so that nothing happens
  if (isAsleep) {
    return; 
  }

  // Reads the rest of the buttons are checking for sleep state so that the program can now start running
  // Checks start/stop button
  int currentHomeButtonState = digitalRead(home_button_pin);
  int currentManualButtonState = digitalRead(manual_button_pin);
  int currentAutoButtonState = digitalRead(auto_button_pin);

  // AI Citation: Non-blocking button debounce logic by Gemini 3.1 Pro.
  // Uses a single millis() check to prevent switch bouncing without stuttering the motor.
  if (millis() - lastButtonPressTime > debounceDelay) {
    // Start/Stop Logic
    if (currentStartStopButtonState == LOW && lastStartStopState == HIGH) {
      if (isAsleep) {
        wake_up();
      } else {
        isRunning = !isRunning; 
        if (isRunning == false) {
          stepper.stop(); // Calculates the deceleration curve instantly
        }
      }
      lastButtonPressTime = millis(); // Reset the bounce timer
    }
    
    // Home Button Logic
    if (currentHomeButtonState == LOW && lastHomeButtonState == HIGH) {
      goHome();
      lastButtonPressTime = millis();
    }

    // Manual Button Logic
    if (currentManualButtonState == LOW && lastManualButtonState == HIGH) {
      isManual = true;
      isAuto = false;
      isRunning = false;
      lastButtonPressTime = millis();
    }
    
    // Auto Button Logic
    if (currentAutoButtonState == LOW && lastAutoButtonState == HIGH) {
      isAuto = true;
      isManual = false;
      isRunning = false;
      lastButtonPressTime = millis();
    }
  }

  // Update last states
  lastStartStopState = currentStartStopButtonState;
  lastHomeButtonState = currentHomeButtonState;
  lastManualButtonState = currentManualButtonState;
  lastAutoButtonState = currentAutoButtonState;

  // Handles all motion

  // If automatic mode is active, runs the function that handles automatic motion
  if (isAuto) {
    // auto_motion() is called continuously so it can finish smooth stops.
    // auto_motion() checks 'isRunning' within itself to know whether to pick new targets, so checking that is not required in the if statement above
    auto_motion(); 
  } else if (isManual && isRunning) {
    // Manual mode only reads the joystick and moves if the system is running
    int joystick_xValue = analogRead(joystick_horiz_pin);
    int joystick_yValue = analogRead(joystick_vert_pin);
    manual_motion(joystick_xValue, joystick_yValue);
  }

  // Updates the LCD twice a second or every 500ms
  if (millis() - lastLcdUpdate >= 500) {
    // update_dashboard updates the lcd
    update_dashboard();

    // updates the lastLCD update to right now for the next loop
    lastLcdUpdate = millis();
  }

  // checks limit switches to ensure system safety and hard stop if necessary
  check_limit_switches();
}

// Helper/Custom Functions

// Handles code for returning dolley to 'home' position
void goHome() {
  stepper.stop();
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Going home..."); 

  stepper.setSpeed(-400); 

  // 1. Move backward until the switch is clicked
  while (digitalRead(limit_switch_home_pin) == HIGH) {
    stepper.runSpeed();
  }
  
  stepper.stop();

  // AI Citation: The "Bounce-Back" CNC homing pattern by Gemini 3.1 Pro.
  // Slowly reverses off the switch until it un-clicks to ensure a highly accurate zero point.
  stepper.setSpeed(200); 
  while (digitalRead(limit_switch_home_pin) == LOW) {
    stepper.runSpeed();
  }

  stepper.stop();
  
  // 3. NOW set this safe, switch-cleared location as absolute zero
  stepper.setCurrentPosition(0);
  isRunning = false;
}



// Handles code for moving the dolley to whatever direction the joystick is pointed to
void manual_motion(int joystick_x, int joystick_y) {
  if (joystick_x < LEFT_THRESHOLD) {
    // Maps from the edge of the deadzone (400) down to full left (0) to a speed of 0 up to 180
    int speed = map(joystick_x, LEFT_THRESHOLD, 0, 0, 180); 
    stepper.setSpeed(speed); 
    stepper.runSpeed();
  } else if (joystick_x > RIGHT_THRESHOLD) {
    // Maps from the edge of the deadzone (800) up to full right (1023)to a speed of 0 up to -180 (negative reverses direction)
    int speed = map(joystick_x, RIGHT_THRESHOLD, 1023, 0, -180); 
    stepper.setSpeed(speed); 
    stepper.runSpeed();
  }

  manual_motion_time = millis();
}

// AI Citation: LSB (Least Significant Bit) Noise Harvesting Algorithm by Gemini 3.1 Pro.
// Uses bitwise operations on thermal static to generate a true random seed without needing empty pins. (FULLY WRITTEN BY GEMINI 3.1 PRO)
unsigned long generate_true_noise_seed() {
  unsigned long random_seed = 0;
  
  for (int i = 0; i < 32; i++) {
    // Read the connected photoresistor
    int noise_reading = analogRead(photoresistor_pin);
    
    // Extract ONLY the very last bit (the unpredictable static)
    int random_bit = noise_reading & 1; 
    
    // Shift it into our growing 32-bit seed
    random_seed = (random_seed << 1) | random_bit; 
    
    // Tiny 2ms delay to let the electrical noise reset
    delay(2); 
  }
  
  return random_seed;
}



// Checks to see if the limit switches were pressed by the dolley and then calls the function to hard stop
void check_limit_switches() {
  if (digitalRead(limit_switch_end_pin) == LOW) {
    trigger_emergency_stop("End Sw: Pressed ");
  }
  
  if (digitalRead(limit_switch_home_pin) == LOW) {
    trigger_emergency_stop("Home Sw: Pressed");
  }
}

// AI Citation: DRY (Don't Repeat Yourself) design pattern by Gemini 3.1 Pro.
// Abstracted the emergency stop logic into a single helper function to handle multiple switches cleanly.
void trigger_emergency_stop(String lcdMessage) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(lcdMessage);
    stepper.setCurrentPosition(stepper.currentPosition());
    isRunning = false;
}

void auto_motion() {
  if (isRunning) {
    // Only pick new random target positions if the system is running (isRunning == true)

    // Ensures that the stepper motor finishes the movement it is currently doing before proceeding with the next motion
    if (stepper.distanceToGo() == 0) {
      long randomPosition = random(0, 2000); 
      stepper.moveTo(randomPosition);
    }
  }
  
  // This allows the motor to finish its smooth deceleration curve even after you press the stop button.
  stepper.run();
}

// Disables all systems 
void disable_all_activity() {
  isAsleep = true;
  isRunning = false;
  stepper.stop();
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Turning off.");
  delay(2000);
  lcd.clear();
  lcd.print("System Asleep");
  delay(3000);
  lcd.noBacklight();
  
  digitalWrite(EN, HIGH);
}

// Wakes system up from sleep
void wake_up() {
  isAsleep = false;
  digitalWrite(EN, LOW);

  lcd.backlight();
  
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("System Awake ");

  darkStartTime = millis();
}

void update_dashboard() {
  // AI Citation: Non-blocking state machine for LCD pagination by Gemini 3.1 Pro.
  // Updates the visual UI every 3 seconds without freezing the motor loop.
  if (millis() - lastPageChange >= 3000) {
    lcdPage++;
    if (lcdPage > 2) {
      lcdPage = 0; // Loop back to the first page
    }
    lastPageChange = millis();
  }

  // Page 0: Main Mode & Status
  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    if (isAuto) {
      lcd.print("Mode: AUTO      ");
    } else if (isManual) {
      lcd.print("Mode: MANUAL    ");
    }

    lcd.setCursor(0, 1);
    if (isRunning == false) {
      lcd.print("Status: PAUSED  ");
    } else {
      lcd.print("Pos: ");
      lcd.print(stepper.currentPosition());
      lcd.print("          ");
    }
  } else if (lcdPage == 1) {
    // Page 1: Environment & Sleep
    lcd.setCursor(0, 0);
    lcd.print("Light Lvl: ");
    lcd.print(analogRead(photoresistor_pin));
    lcd.print("     "); 

    lcd.setCursor(0, 1);
    if (isDark) {
      // Calculates how many seconds are left until sleep
      int secondsLeft = 30 - ((millis() - darkStartTime) / 1000);
      if (secondsLeft < 0) secondsLeft = 0; // Prevent negative numbers
      
      lcd.print("Sleep in: ");
      lcd.print(secondsLeft);
      lcd.print("s   ");
    } else {
      lcd.print("Light: Good  ");
    }
  } else if (lcdPage == 2) {
    // Page 2: Motor Targets
    lcd.setCursor(0, 0);
    if (isAuto && isRunning) {
      lcd.print("Target: ");
      lcd.print(stepper.targetPosition());
      lcd.print("      ");
    } else {
      lcd.print("Target: N/A     ");
    }

    lcd.setCursor(0, 1);
    if (isRunning) {
      lcd.print("Dist: ");
      lcd.print(stepper.distanceToGo());
      lcd.print("        ");
    } else {
      lcd.print("Motor Idle      ");
    }
  }
}