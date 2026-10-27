// Gemini 3.1 Pro created tester code for individual components from the code I (with partial assistance from Gemini) wrote for the entire system.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 

// LCD I2C pins
const int SDA_pin = A4; 
const int SCL_pin = A5;

// Limit switch pins
const int limit_switch_home_pin = 8;
const int limit_switch_end_pin = 9;

hd44780_I2Cexp lcd; 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

void setup() {
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) {
    while(1); 
  }

  // Initialize the limit switch pins exactly as they are in your main code
  pinMode(limit_switch_home_pin, INPUT_PULLUP);
  pinMode(limit_switch_end_pin, INPUT_PULLUP);
  
  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("Testing Switches");
  delay(1500); 
  lcd.clear(); 
}

void loop() {
  // Read the switches. Remember: INPUT_PULLUP means LOW = Pressed, HIGH = Not Pressed
  int home_state = digitalRead(limit_switch_home_pin);
  int end_state = digitalRead(limit_switch_end_pin);

  // --- UI Update ---
  
  // Row 0: Home Switch Status
  lcd.setCursor(0, 0);
  if (home_state == LOW) {
    lcd.print("Home Sw: PRESSED");
  } else {
    lcd.print("Home Sw: Open   "); // Spaces clear leftover characters
  }

  // Row 1: End Switch Status
  lcd.setCursor(0, 1);
  if (end_state == LOW) {
    lcd.print("End Sw:  PRESSED");
  } else {
    lcd.print("End Sw:  Open   ");
  }

  // Small delay to prevent the LCD from flickering
  delay(100);
}