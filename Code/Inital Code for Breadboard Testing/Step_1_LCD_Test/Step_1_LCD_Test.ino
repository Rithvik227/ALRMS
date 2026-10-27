// Gemini 3.1 Pro created tester code for individual components from the code I (with partial assistance from Gemini) wrote for the entire system.

#include <Wire.h>
#include <hd44780.h>                      
#include <hd44780ioClass/hd44780_I2Cexp.h> 

// Note: On standard Arduino Unos/Nanos, Wire.h automatically uses A4 for SDA and A5 for SCL.
const int SDA_pin = A4; 
const int SCL_pin = A5;

hd44780_I2Cexp lcd; 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

void setup() {
  // Attempt to initialize the LCD
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  
  // If the LCD fails to connect, the Arduino will freeze here on purpose
  if(status) {
    while(1); 
  }

  lcd.backlight();
  lcd.setCursor(0, 0); 
  lcd.print("System Boot...");
  delay(1500); 
  
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("LCD Test: PASS  ");
}

void loop() {
  // Print a running timer on the second row so you know it's actively working
  lcd.setCursor(0, 1);
  lcd.print("Uptime: ");
  lcd.print(millis() / 1000);
  lcd.print("s   ");
  
  delay(500);
}