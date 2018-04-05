
//! @author  Identity Withheld <metssigadus@xyz.ee> (l)
//! @date    2018-04-05 1522936740
//! @brief   Networked Geiger Counter sketch
//! @note    The tube interfaced. Ether payload dummied for a while.

/* 
 DIY Geiger counter
 with an Hitachi HD44780 LCD driver and network capability
 
 Geiger signal fed to  Digital pin 2 (INT0 capable)
 
 NB! Programmed to Nano Pro 16MHz 5V; programmer = USB Tiny
 currently ~560 bytes free RAM left thnx to PROGRAM and F() macro */

// ========================== include the libraries:
#include <LiquidCrystal.h>
#include <EtherCard.h>
// DS3231 RTC is connected via I2C using Wire lib
#include <Wire.h>
#include <RTClib.h>

// ========================== defines
byte Ethernet::buffer[750]; // RAM is a scare resource on Arduinos!

const char website[] PROGMEM = {"xyz.ee"}; //7
const char url[] = {"pong.htm"}; //9 

// We bother the web site repeatedly
#define REQUEST_RATE 59999 // milliseconds
static long timer; //milliseconds

// User Agent definition:
const char headerline[] PROGMEM = {"User-Agent: Arduino/1.0 (Nano Pro Geiger / 0.06)"}; //49

// ethernet interface mac address
const byte mymac[] = {0x74,0x69,0x69,0x2D,0x30,0x34 };

boolean weHaveNetwork = false;
// Geiger
// Conversion factor - CPM to uSV/h
#define CONV_FACTOR 0.0057

int geigerPin = 2;
volatile long clicks = 0;
long cpm = 0;
long timePreviousMeasure = 0;
volatile long totalClicks = 0;
volatile long totalTime = 0;
// volatile long average = 0; // not int to avoid conversions

float microSieverts = 0.0;


// ========================== initializing the libraries

/* initialize the library in 4-bit mode according to HW pins:
   RS, Enable, DB4 , DB5, DB6, DB7 */
LiquidCrystal lcd(9, 8, 7, 6, 5, 4); 

// RTC
RTC_DS3231 rtc;


// ========================== setup()
void setup() {
  Serial.begin(57600);
  delay (3000);
  Serial.println(F("Starting..."));

  lcd_init();
  ether_init();
  
  if (weHaveNetwork) {
     dhcp_init();
  }
    if (weHaveNetwork) { // Again!!
     dns_lookup();
     }
     
  rtc_init();
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2018, 3, 25, 16, 35, 0));
  // Uwaga - no timezone! Do think as of UTC.

    if (weHaveNetwork) {
  // --------- NTP
  // TBD - https://www.eecis.udel.edu/~mills/y2k.html
  // ToDo - set, correct or use the current time (once)
    }
  
  gui_trivia();

  // --------- Geiger setup
  pinMode(geigerPin, INPUT);
  digitalWrite(geigerPin,HIGH);
  attachInterrupt(0,irqService,FALLING);

} // END of SETUP

// ========================== main()

void loop() {
  
  // do_whatever_but_fast_enough();
  int average = 0;
  
  volatile long timeMarker = millis();
  totalTime = (timeMarker / 1000);
  if ((timeMarker - timePreviousMeasure) > 10000) {
    cpm = 6 * clicks;
    microSieverts = cpm * CONV_FACTOR;
    timePreviousMeasure = millis();
    average = (totalClicks * 60 / totalTime) ; // totalTime
    Serial.print(totalTime);
    Serial.print(" secs");
    Serial.print(" - ");
    Serial.print("count=");
    Serial.print(totalClicks);
    Serial.print(" - ");
    Serial.print("CPM = "); 
    Serial.print(cpm,DEC);
    Serial.print(" -===- ");
    Serial.print("avg=");
    Serial.print(average, DEC);
    Serial.print(" -===- ");
    Serial.print("uSv/h = ");
    Serial.println(microSieverts,5);      
    lcd.clear();    
    lcd.setCursor(0,0);
    lcd.print("cpm ");
    lcd.print(cpm);
    lcd.setCursor(8,0);
    lcd.print("avg ");
    lcd.print(average);
    lcd.setCursor(2,1);
    lcd.print(microSieverts,5);
    lcd.setCursor(8,1);
    lcd.print(" uSv/h");
    clicks = 0;
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
  lcd.setCursor(0, 1);
  // Print a message to the LCD.
  lcd.print(F("LCD is WORKING!!"));
  Serial.println(F("Do check if the LCD is working!")); // No EZ way to check manually
  delay(2000);
  lcd.clear(); 
}

// --------- ether_init() enj

static void ether_init() {
  Serial.println(F("Requesting IP..."));
  lcd.clear();
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    weHaveNetwork = false;
    lcd.setCursor(0, 0);
    lcd.print(F("Ether failure!"));
    Serial.println(F("Ether init fail!"));
    fatalerror(); // Bail off!
  } 
  else {
    weHaveNetwork = true;
    lcd.setCursor(0, 0);
    lcd.print(F("Ether init OK."));
    Serial.println(F("Ether init OK."));
    lcd.setCursor(0, 1);
    // MAC address need to be printed
    // should we invent a function? ->  char[] beautify(char[],count,separator)
    Serial.print(F("MAC="));
    lcd.print(F("MAC="));

    for (byte i = 0; i < 6; ++i) {
      Serial.print(mymac[i], HEX);
      lcd.print(mymac[i], HEX);
      if (i < 5)
        Serial.print(':');
    }
     Serial.println("");
  }
  delay(3000);
}


// ------------- DHCP init

static void dhcp_init() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Requesting IP:"));
  Serial.println(F("Requesting IP via DHCP..."));
  
  if (!ether.dhcpSetup()) {
    weHaveNetwork = false;
    lcd.setCursor(0, 0);
    lcd.print(F("DHCP failed. BRR"));
    Serial.println(F("\nWe obtained no DHCP address. This is FATAL."));
    fatalerror(); // Bail off!
  } 
  else {
    weHaveNetwork = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("DHCP IP OK"));
    Serial.print(F("\nWe obtained an IP address: "));
    lcd.setCursor(0, 1);
    lcd.print(F("="));

    for (byte i = 0; i < 4; ++i) {
      Serial.print(ether.myip[i], DEC);
      lcd.print(ether.myip[i], DEC);
      if (i < 3) {
        Serial.print(F("."));
        lcd.print(F("."));
      }
    }

  }
      delay(3000);
      lcd.clear();
}


// ------------- DNS Lookup

static void dns_lookup() {
  Serial.println(F("\nAttempting DNS..."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Attempting DNS:"));
  delay(2000);
  // --------- DNS 
  if (!ether.dnsLookup(website)) {
    lcd.setCursor(0, 0);
    lcd.print(F("DNS failed. BRR"));
    Serial.println(F("\nDNS request failed. This is FATAL."));
    fatalerror(); // Bail off!
    weHaveNetwork = false;

  } 
  else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("DNS OK"));
    Serial.print(F("\nDNS lookup OK. Destination WWW server IP is: "));
    for (byte i = 0; i < 4; ++i) {
      Serial.print(ether.hisip[i], DEC);
      if (i < 3)
        Serial.print('.');
    }
    Serial.println();
    weHaveNetwork = true;
  }
  delay(3000);
}


// --------- rtc_init()  -------- I2C clock

static void rtc_init() {

  if (! rtc.begin()) {
    Serial.println(F("No DS3231 RTC!"));
    lcd.setCursor(0, 0);
    lcd.print(F("No DS3231 RTC!"));
    fatalerror();
  } 
  else {
    Serial.println(F("DS3231 RTC OK!"));
    lcd.clear();
    lcd.print(F("DS3231 RTC OK!"));
    delay (1000);
    DateTime now = rtc.now();

    Serial.print(F("utime: "));
    Serial.println(now.unixtime());
    lcd.setCursor(3, 1);
    lcd.print(now.unixtime());
    delay(3000);
    lcd.clear();

  }

}


// ----------------- GUI trivia

static void gui_trivia() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("SETUP successful"));
  delay(1000);
  lcd.clear();
  // ....LCD wipe
  lcd.setCursor(0, 1);
  lcd.print(F("loop: HTTP GET"));
  lcd.setCursor(0, 0);
  lcd.print(F("UpSec: "));
  Serial.println(F("\nStarting a timer driven loop.\n"));  

  lcd.setCursor(9, 0);
  lcd.print(F("wait!"));

  // ....Extra Debug
  Serial.print(F("free RAM left: "));
  Serial.println(freeRam()); 
}


// --------------- do_whatever_but_fast_enough()

static void do_whatever_but_fast_enough() {
/* (https://jeelabs.org/2011/06/19/ethercard-library-api/)
// likely it means - do those ARP and ICMP thingies each time we call you
// ... on low level: take received data from the ENC28J60 and put into Arduino memory buffer. */
  ether.packetLoop(ether.packetReceive());
  
  if (millis() > timer + REQUEST_RATE) {
    timer = millis();
    
    // a real hack to cover the word "wait"
    // b/c right justify with a buffer is too expensive
    lcd.setCursor(7, 0);
    lcd.print("         ");
    
    lcd.setCursor(7, 0);
    lcd.print(timer/1000); // Seconds ( *1000) passed from the inizialization

    Serial.print(F("free RAM left: "));
    Serial.print(freeRam());
    Serial.println(F(" >>> REQ"));

  // http://www.rogerclark.net/aurduino-ethercard-multiple-browser-request-example/
    // A very picky method - website&headerline must be PROGMEM and url must be not, OR face problems!
    ether.browseUrl(PSTR("/"), url, website, headerline, raw_http_reply); // magic, eh?
    delay (20); // probably a safeguard timer 
  }
  
}

// -------------------irqservice()
void irqService(){
  detachInterrupt(0);
  clicks++;
  totalClicks++;
  while(digitalRead(2)==0){
  }
  attachInterrupt(0,irqService,FALLING);
}


// --------------- loopforever() -> a substitution for  break() not available for Arduino
static void fatalerror()
{
  lcd.setCursor(0, 1);
  lcd.print(F(" ERR!!!"));
  // while (1); // Do nothing forever
}


// =============== Foreign functions (copied from elsewhere and modded) ================

// --------- raw_http_reply() - returns what the server answered and how fast
static void raw_http_reply (byte status, word off, word len) {
  Serial.print(F("<<< reply "));
  Serial.print(millis() - timer);
  Serial.println(F(" ms"));
  Serial.println(F("-----------------"));
  Serial.println((const char*) Ethernet::buffer + off);
  Serial.println(F("-----------------"));
}


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



