//! @author  Identity Withheld <metssigadus@xyz.ee> (l)
//! @date    2018-03-30 1522405388
//! @brief   Networked Geiger Counter sketch
//! @note    The tube being interfaced. LCD and Ether and RTC were previously checked

/*
 DIY Geiger counter
 with an Hitachi HD44780 LCD driver

 Geiger signal fed to  Digital pin 2 (INT0 capable) */

// ========================== include the libraries:
#include <LiquidCrystal.h>
#include <EtherCard.h>

// ========================== defines

// Geiger
// Conversion factor - CPM to uSV/h
#define CONV_FACTOR 0.0057

int geigerPin = 2;
long particles_counted = 0;
long cpm = 0;
long timePreviousMeasure = 0;
long time = 0;
float microSieverts = 0.0;

volatile long count = 0; // intterrupt resilient variable

// ========================== initializing the libraries

/* initialize the library in 4-bit mode according to HW pins:
   RS, Enable, DB4 , DB5, DB6, DB7 */
LiquidCrystal lcd(9, 8, 7, 6, 5, 4);



// ========================== setup()
void setup() {
  Serial.begin(57600);
  delay (3000);
  Serial.println(F("Starting..."));

  lcd_init();

  // --------- Geiger setup
  pinMode(geigerPin, INPUT);
  digitalWrite(geigerPin,HIGH);
  attachInterrupt(0,irqService,FALLING);
} // END of SETUP

// ========================== main()

void loop() {

  // do_whatever_but_fast_enough();

  if (millis()-timePreviousMeasure > 10000){
    cpm = 6 * count;
    microSieverts = cpm * CONV_FACTOR;
    timePreviousMeasure = millis();
    Serial.print("cpm = ");
    Serial.print(cpm,DEC);
    Serial.print(" -===- ");
    Serial.print("uSv/h = ");
    Serial.println(microSieverts,5);
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("CPM =");
    lcd.setCursor(9,0);
    lcd.print(cpm);
    lcd.setCursor(2,1);
    lcd.print(microSieverts,5);
    lcd.setCursor(8,1);
    lcd.print(" uSv/h");
    count = 0;
    }


} // end of MAIN

// -=============== Functions ==============-



// ---------- lcd_init()
static void lcd_init() {
  // set up the LCD's number of columns and rows:
  Serial.println(F("\nInitializing LCD now"));
  lcd.begin(16, 2);
    // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 0);
  // Print a message to the LCD.
  lcd.print(F("LCD is WORKING!!"));
  delay(1000);
  lcd.setCursor(1, 1);
  lcd.print(freeRam());
  lcd.print(" bytes free");
  delay(2000);
  lcd.clear();
}




// -------------------irqservice()
void irqService(){
  detachInterrupt(0);
  count++;
  while(digitalRead(2)==0){
  }
  attachInterrupt(0,irqService,FALLING);
}


// --------------- loopforever() -> a substitution for  break() not available for Arduino
static void loopforever()
{
  lcd.setCursor(0, 1);
  lcd.print(F("FATAL ERR!!!"));
  while (1); // Do nothing forever
}


// =============== Foreign functions (copied from elsewhere and modded) ================



//--------------- freeRam ----------------

//! @brief    Shows how many bytes of free memory left
//! @details  Some black magic far above the beginners' level
//! @note   Src: https://learn.adafruit.com/memories-of-an-arduino/measuring-free-memory

int freeRam ()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}


