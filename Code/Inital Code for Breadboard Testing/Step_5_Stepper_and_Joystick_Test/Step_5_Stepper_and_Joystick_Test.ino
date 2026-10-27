// Gemini 3.1 Pro created tester code for individual components from the code I (with partial assistance from Gemini) wrote for the entire system.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 
#include <AccelStepper.h> 

// LCD I2C pins
const int SDA_pin = A4; 
const int SCL_pin = A5;

// Limit switch pin (Only Home needed for this test)
const int limit_switch_home_pin = 8;

// Buttons pins
const int start_stop_button_pin = 10;
const int home_button_pin = 11;
const int manual_button_pin = 12;
const int auto_button_pin = 13;

// Stepper Pins
const int EN = 5;
const int DIR = 6;
const int STEP = 7;

// Joystick Pin (Horizontal only for Dolly movement)
const int joystick_horiz_pin = A1;

const int LEFT_THRESHOLD = 400;
const int RIGHT_THRESHOLD = 800;

hd44780_I2Cexp lcd; 
AccelStepper stepper(1, STEP, DIR); 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int debounceDelay = 200;

bool isRunning = false;
bool isManual = false;
bool isAuto = true;

int lastStartStopState = HIGH;
int lastHomeButtonState = HIGH;
int lastManualButtonState = HIGH;
int lastAutoButtonState = HIGH;

unsigned long lastButtonPressTime = 0;
unsigned long lastLcdUpdate = 0;

void setup() {
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) {
    while(1); 
  }

  pinMode(limit_switch_home_pin, INPUT_PULLUP);
  
  pinMode(start_stop_button_pin, INPUT_PULLUP);
  pinMode(home_button_pin, INPUT_PULLUP);
  pinMode(manual_button_pin, INPUT_PULLUP);
  pinMode(auto_button_pin, INPUT_PULLUP);

  pinMode(EN, OUTPUT);
  digitalWrite(EN, LOW); // Force driver ENABLE to ON

  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(400.0);
  
  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("Motor Test Ready");
  delay(1500); 
  lcd.clear(); 
}

void loop() {
  // Read current button states
  int currentStartStopState = digitalRead(start_stop_button_pin);
  int currentHomeState = digitalRead(home_button_pin);
  int currentManualState = digitalRead(manual_button_pin);
  int currentAutoState = digitalRead(auto_button_pin);

  // Debounce and Button Logic
  if (millis() - lastButtonPressTime > debounceDelay) {
    // Start/Stop
    if (currentStartStopState == LOW && lastStartStopState == HIGH) {
      isRunning = !isRunning; 
      if (!isRunning) stepper.stop(); // Calculate rapid deceleration if stopped
      lastButtonPressTime = millis();
    }
    
    // Home
    if (currentHomeState == LOW && lastHomeButtonState == HIGH) {
      goHome();
      lastButtonPressTime = millis();
    }

    // Manual
    if (currentManualState == LOW && lastManualButtonState == HIGH) {
      isManual = true;
      isAuto = false;
      isRunning = false;
      stepper.stop();
      lastButtonPressTime = millis();
    }
    
    // Auto
    if (currentAutoState == LOW && lastAutoButtonState == HIGH) {
      isAuto = true;
      isManual = false;
      isRunning = false;
      stepper.stop();
      lastButtonPressTime = millis();
    }
  }

  // Update last states
  lastStartStopState = currentStartStopState;
  lastHomeButtonState = currentHomeState;
  lastManualButtonState = currentManualState;
  lastAutoButtonState = currentAutoState;

  // --- Motion Handling ---
  if (isAuto) {
    auto_motion(); 
  } else if (isManual && isRunning) {
    int joystick_xValue = analogRead(joystick_horiz_pin);
    manual_motion(joystick_xValue);
  }

  // --- UI Update (4 times a second) ---
  if (millis() - lastLcdUpdate >= 250) {
    lcd.setCursor(0, 0);
    lcd.print(isAuto ? "Mode: AUTO      " : "Mode: MANUAL    ");
    
    lcd.setCursor(0, 1);
    if (!isRunning) {
      lcd.print("Status: PAUSED  ");
    } else {
      lcd.print("Motor: RUNNING  ");
    }
    lastLcdUpdate = millis();
  }
}

// Helper/Custom Functions

void goHome() {
  stepper.stop();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Action: HOMING  "); 

  stepper.setSpeed(-400); 

  // 1. Move backward until the switch is clicked
  while (digitalRead(limit_switch_home_pin) == HIGH) {
    stepper.runSpeed();
  }
  
  stepper.stop();

  // 2. Slowly reverse off the switch ("Bounce-Back")
  stepper.setSpeed(200); 
  while (digitalRead(limit_switch_home_pin) == LOW) {
    stepper.runSpeed();
  }

  stepper.stop();
  
  // 3. Set absolute zero
  stepper.setCurrentPosition(0);
  isRunning = false;
  lcd.clear();
}

void manual_motion(int joystick_x) {
  if (joystick_x < LEFT_THRESHOLD) {
    // Maps from deadzone edge (400) to full left (0) -> speeds 0 to 180
    int speed = map(joystick_x, LEFT_THRESHOLD, 0, 0, 180); 
    stepper.setSpeed(speed); 
    stepper.runSpeed();
  } else if (joystick_x > RIGHT_THRESHOLD) {
    // Maps from deadzone edge (800) to full right (1023) -> speeds 0 to -180
    int speed = map(joystick_x, RIGHT_THRESHOLD, 1023, 0, -180); 
    stepper.setSpeed(speed); 
    stepper.runSpeed();
  }
}

void auto_motion() {
  if (isRunning) {
    // Pick a new random target if the motor has reached its destination
    if (stepper.distanceToGo() == 0) {
      long randomPosition = random(0, 2000); 
      stepper.moveTo(randomPosition);
    }
  }
  // Let the motor handle its own microstepping and acceleration curves
  stepper.run();
}