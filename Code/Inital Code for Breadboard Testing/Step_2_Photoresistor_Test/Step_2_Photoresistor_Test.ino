// Gemini 3.1 Pro created tester code for individual components from the code I (with partial assistance from Gemini) wrote for the entire system.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 

// LCD I2C pins
const int SDA_pin = A4; 
const int SCL_pin = A5;

// Photoresistor
const int photoresistor_pin = A3;

hd44780_I2Cexp lcd; 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

bool isDark = false;
unsigned long darkStartTime = 0;

void setup() {
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) {
    while(1); 
  }

  // Initialize the photoresistor pin exactly as it is in your main code
  pinMode(photoresistor_pin, INPUT_PULLUP);
  
  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("Testing Sensor..");
  delay(1500); 
  lcd.clear(); 
}

void loop() {
  // Read the photoresistor
  int photoresistor_value = analogRead(photoresistor_pin);

  // Core logic from your main code
  if (photoresistor_value < 250) {
    if (isDark == false) {
      darkStartTime = millis();
      isDark = true;
    } 
  } else {
    isDark = false; 
  }

  // --- UI Update ---
  // Row 0: Display the raw analog value
  lcd.setCursor(0, 0);
  lcd.print("Light Lvl: ");
  lcd.print(photoresistor_value);
  lcd.print("     "); // Blank spaces clear leftover digits

  // Row 1: Display the status or countdown
  lcd.setCursor(0, 1);
  if (isDark) {
    // Calculate how many seconds are left until sleep
    int secondsLeft = 30 - ((millis() - darkStartTime) / 1000);
    if (secondsLeft <= 0) {
      lcd.print("SYSTEM ASLEEP!  ");
    } else {
      lcd.print("Sleep in: ");
      lcd.print(secondsLeft);
      lcd.print("s   ");
    }
  } else {
    lcd.print("Light: Good     ");
  }

  // Small delay to prevent the LCD from flickering updating too fast
  delay(150);
}