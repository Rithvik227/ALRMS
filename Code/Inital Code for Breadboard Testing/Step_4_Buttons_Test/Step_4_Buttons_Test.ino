// Gemini 3.1 Pro created tester code for individual components from the code I (with partial assistance from Gemini) wrote for the entire system.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 

// LCD I2C pins
const int SDA_pin = A4; 
const int SCL_pin = A5;

// Buttons pins
const int start_stop_button_pin = 10;
const int home_button_pin = 11;
const int manual_button_pin = 12;
const int auto_button_pin = 13;

hd44780_I2Cexp lcd; 

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
unsigned long homeMessageTime = 0;
bool showHomeMessage = false;

void setup() {
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) {
    while(1); 
  }

  // Initialize the button pins as INPUT_PULLUP
  pinMode(start_stop_button_pin, INPUT_PULLUP);
  pinMode(home_button_pin, INPUT_PULLUP);
  pinMode(manual_button_pin, INPUT_PULLUP);
  pinMode(auto_button_pin, INPUT_PULLUP);

  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("Testing Buttons.");
  delay(1500); 
  lcd.clear(); 
}

void loop() {
  // Read current states
  int currentStartStopButtonState = digitalRead(start_stop_button_pin);
  int currentHomeButtonState = digitalRead(home_button_pin);
  int currentManualButtonState = digitalRead(manual_button_pin);
  int currentAutoButtonState = digitalRead(auto_button_pin);

  // Debounce logic from your main code
  if (millis() - lastButtonPressTime > debounceDelay) {
    
    // Start/Stop Logic
    if (currentStartStopButtonState == LOW && lastStartStopState == HIGH) {
      isRunning = !isRunning; 
      lastButtonPressTime = millis();
    }
    
    // Home Button Logic
    if (currentHomeButtonState == LOW && lastHomeButtonState == HIGH) {
      showHomeMessage = true;
      homeMessageTime = millis();
      isRunning = false; // Homing stops normal running
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

  // --- UI Update ---
  
  // Row 0: Display Mode
  lcd.setCursor(0, 0);
  if (isAuto) {
    lcd.print("Mode: AUTO      ");
  } else if (isManual) {
    lcd.print("Mode: MANUAL    ");
  }

  // Row 1: Display Status (or Home action override)
  lcd.setCursor(0, 1);
  if (showHomeMessage) {
    lcd.print("Action: GO HOME!");
    // Clear the home message after 1.5 seconds
    if (millis() - homeMessageTime > 1500) {
      showHomeMessage = false;
    }
  } else {
    if (isRunning == false) {
      lcd.print("Status: PAUSED  ");
    } else {
      lcd.print("Status: RUNNING ");
    }
  }

  delay(50); // Tiny delay for screen stability
}