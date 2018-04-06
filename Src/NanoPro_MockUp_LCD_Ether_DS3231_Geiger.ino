
//! @author  Identity Withheld <metssigadus@xyz.ee> (l)
//! @date    2018-04-06 1523048882
//! @brief   Networked Geiger Counter sketch
//! @note    The tube interfaced. Ether payload dummied for a while.

/* 
 DIY Geiger counter
 with an Hitachi HD44780 LCD driver and network capability
 
 Geiger signal fed to  Digital pin 2 (INT0 capable)
 
 NB! My device rogrammed to Nano Pro 16MHz 5V; programmer = USB Tiny
 currently ~560 bytes free RAM left thnx to PROGRAM and F() macro */

// ========================== include the libraries:
#include <LiquidCrystal.h>
#include <EtherCard.h>
// DS3231 RTC is connected via I2C using Wire lib
#include <Wire.h>
#include <RTClib.h>

// Using this library: https://github.com/PaulStoffregen/Time
#include <Time.h>
#include <TimeLib.h>

// ========================== defines
#define DEBUG true


//  --------- Network related vars:

// We bother the web site repeatedly
#define REQUEST_RATE 59999 // milliseconds

const char timeServer[] PROGMEM = "pool.ntp.org";
const int utcOffset = 0; // Because we like so
const unsigned int remoteNtpPort = 123;

// So far our HTTP target is a static one
const char website[] PROGMEM = {"xyz.ee"}; //7
const char url[] = {"pong.htm"}; //9 

// User Agent definition:
const char headerline[] PROGMEM = {"User-Agent: Arduino/1.0 (Nano Pro Geiger / 0.06)"}; //49

// ethernet interface mac address
const byte mymac[] = {0x74,0x69,0x69,0x2D,0x30,0x34 };

byte Ethernet::buffer[750]; // RAM is a scare resource on Arduinos!

boolean weHaveTheEther = false;
boolean weHaveTheNetwork = false;
int secsWithoutTheNetwork = 0;


// GMC related
// Conversion factor - CPM to uSV/h
#define CONV_FACTOR 0.0057 // SBM20
int geigerPin = 2;

volatile long clicks = 0;
volatile long clicksPerPeriodOne = 0;
volatile long clicksPerPeriodTwo = 0;
volatile long totalClicks = 0;
volatile int averageCpmForTask1 = 0;
volatile int averageCpmForTask2 = 0;


float microSieverts = 0.0;

// Measurement variables
const int taskOneInterval = 10; // LCD information every 10 sec
const int taskTwoInterval = 60; // Server contacted each minute or 10
const int taskThreeInterval = 50; // trying to repair the network
const int taskFourInterval = 7200; // NTP time update
volatile int taskOneTimes = 0;
volatile int taskOneCycles = 6; // MUST BE DIVISIBLE
volatile int taskTwoTimes = 0;

volatile long totalRunTime = 0; // secs
static long timer; //milliseconds
long cpm = 0;
long timeOfLastAction = 0;


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
  rtc_init();
  ether_init();
  
  if (weHaveTheNetwork) { dhcp_init(); }
  if (weHaveTheNetwork) { dns_lookup(); }

  rtc_sync();
  while (timeStatus() == timeNotSet) {
    Serial.print("Trying to set the time");
    delay(1000); 
  }
  setSyncInterval(taskFourInterval);

  publishInformation();

  // --------- Geiger setup
  pinMode(geigerPin, INPUT);
  digitalWrite(geigerPin,HIGH);
  attachInterrupt(0,irqService,FALLING);

} // END of SETUP


// ========================== main()

void loop() {
  // DS3231Time + 946684800 = UnixTime
  // https://github.com/PaulStoffregen/Time
  long currentTime = now();
    if (currentTime != timeOfLastAction) {
    
    if ((currentTime % taskOneInterval) == 0) {
       Serial.print(currentTime);
       Serial.print(" -------- ");
       DateTime now = rtc.now();
       Serial.print(now.unixtime());
       Serial.println(" -------- measurementTask");
       measurementTask();
    }

    if ((currentTime % taskTwoInterval) == 0) {
       Serial.print(currentTime);
       Serial.println(" -------- !!!!! reportingTask");
       reportingTask();
    }
  timeOfLastAction = currentTime;
  }
} // end of MAIN



// -=============== Functions ==============-

// ---------- lcd_init()
static void lcd_init() {

if (DEBUG) {
  Serial.println(F("\nInitializing LCD now"));
  }
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // set the cursor to column 0, line 0
  lcd.setCursor(2, 0);
  // ToDo: define the atom sign!
  lcd.print(F("Greetz from"));
  lcd.setCursor(5, 1);
  lcd.print(F("Fukushima!"));
if (DEBUG) {  
  Serial.println(F("Go and check whether the LCD is working!")); // No EZ way to dor it automagically
}
  delay(2400); // timespan to read the msg
}


// --------- ether_init() enj28j60

static void ether_init() {
  if (DEBUG) {
    Serial.println(F("Initializing Ethercard..."));
  }
  lcd.clear();
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    // weHaveTheNetwork = false;
    lcd.setCursor(0, 1);
    lcd.print(F("Ether failure!"));
    if (DEBUG) {
    Serial.println(F("Ether init failed miserably! We have no Ethercard present"));

    }

  } else {
    weHaveTheNetwork = true;
    lcd.setCursor(0, 0);
    lcd.print(F("Ether init OK."));
      if (DEBUG) {
        Serial.println(F("Ether init OK."));
        Serial.print(F("MAC="));
      }
    // MAC address will be printed
    // should we invent a function? ->  char[] beautify(char[],count,separator)
    lcd.setCursor(0, 1);
    lcd.print(F("MAC="));

    for (byte i = 0; i < 6; ++i) {
        lcd.print(mymac[i], HEX);
          if (DEBUG) {  
             Serial.print(mymac[i], HEX);
          }
      if (i < 5) {
        if (DEBUG) { 
        Serial.print(':');
        }
      }
    } // for byte
    if (DEBUG) { 
     Serial.println();
    }
  }
  delay(3000); // a timespan enabling staring at the screen
}


// ------------- DHCP init

static void dhcp_init() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Requesting IP:"));
  lcd.setCursor(3, 1);
  lcd.print(F("timeout???"));
  if (DEBUG) { 
  Serial.println(F("Requesting IP via DHCP..."));
  }
  if (!ether.dhcpSetup()) {
    weHaveTheNetwork = false;
    lcd.setCursor(0, 1);
    lcd.print(F("DHCP failed. ERR"));
    ether.powerDown();
      if (DEBUG) { 
        Serial.println(F("\nWe obtained no DHCP address.\nNo network available."));
      }
  } 
  else {
    weHaveTheNetwork = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("DHCP IP OK"));
       if (DEBUG) { 
    Serial.print(F("\nWe obtained an IP address: "));
       }
    lcd.setCursor(0, 1);
    lcd.print(F("="));

    for (byte i = 0; i < 4; ++i) {
      Serial.print(ether.myip[i], DEC);
      lcd.print(ether.myip[i], DEC);
      if (i < 3) {
        Serial.print(F("."));
        lcd.print(F("."));
      }
    } // for

  } // else
      delay(3000);
}


// ------------- DNS Lookup

static void dns_lookup() {
     if (DEBUG) { 
  Serial.println(F("\nAttempting DNS..."));
     }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Attempting DNS:"));
  delay(500);
  
  if (!ether.dnsLookup(website)) {
    weHaveTheNetwork = false; // explicitly; could have had meanwhile
    lcd.setCursor(0, 0);
    lcd.print(F("DNSreq FATAL"));
    if (DEBUG) { 
      Serial.println(F("\nDNS request failed. This is FATAL."));
    }
  } else {
    weHaveTheNetwork = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Target DNS OK"));
         if (DEBUG) { 
           Serial.print(F("DNS lookup OK. Destination WWW server IP is: "));
           for (byte i = 0; i < 4; ++i) {
             Serial.print(ether.hisip[i], DEC);
             if (i < 3) {
               Serial.print('.');
             } else {
               Serial.println();
             }
         } //for
        } // DEBUG
   delay(2500);
   } // else
   


}


// --------- rtc_init()  -------- I2C clock

static void rtc_init() {

    if (DEBUG) { 
      Serial.print(F("Initializing the runtime clock...  "));
    }
  lcd.clear();
  lcd.setCursor(0, 0);
  
  if (! rtc.begin()) {
    if (DEBUG) { 
    Serial.println(F("No DS3231 RTC accessible!"));
    }
    lcd.setCursor(0, 1);
    lcd.print(F("No DS3231 RTC!"));
  } else {
    lcd.clear();
    lcd.print(F("DS3231 RTC OK!"));
    if (DEBUG) { 
      Serial.println(F("DS3231 RTC OK!"));
      // Serial.print(F("utime: "));
      // Serial.println(F("waiting for sync"));
      }
     time_t t = now(); // Store the current time in time 
     setTime(t);
     // rtc.adjust(t);
     
    } //else OK
    delay(2400);
}


  static void rtc_sync() {
  // NB! - no timezone defined! Do think as of UTC.
    if (weHaveTheNetwork) {
      
         /*  if (!ether.dnsLookup(timeServer)) {
    Serial.println("DNS failed");
    } else {
          lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Target DNS OK"));
         if (DEBUG) { 
           Serial.print(F("DNS lookup OK. Destination NTP server IP is: "));
           for (byte i = 0; i < 4; ++i) {
             Serial.print(ether.hisip[i], DEC);
             if (i < 3) {
               Serial.print('.');
             } else {
               Serial.println();
             }
         } //for
        } // DEBUG
} // else */
      
      setSyncProvider(getNtpTime);
      // setSyncProvider(getDstCorrectedTime);
    } else {
    delay(500);
    DateTime now = rtc.now();
    lcd.setCursor(3, 1);
    lcd.print(now.unixtime());
    if (DEBUG) { 
    Serial.println(now.unixtime());
    } 
    delay(2400);
      }
  }
// ----------------- GUI trivia

static void publishInformation() {
  if (DEBUG) {
    Serial.println(F("\nStarting a timer driven loop.\n"));
   // ....Extra Debug
    Serial.print(F("free RAM left: "));
    Serial.println(freeRam()); 
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("SETUP successful"));
  delay(1500);
  if (weHaveTheNetwork) {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(F("loop: HTTP GET"));
  } else {
  lcd.setCursor(2, 1);
  lcd.print(F("No network!!"));
  
  }
  delay(1500);
  lcd.setCursor(0, 0);
  lcd.print(F("UpSec:   wait!"));
}

// ------------------- measure and show the radiation level
static void measurementTask() {
    taskOneTimes++;
  // volatile long timeMarker = millis();
    cpm = clicks * taskOneCycles ; // measured 10 sec; prognosed for 60 sec
    microSieverts = cpm * CONV_FACTOR;
    averageCpmForTask1 = ((clicksPerPeriodOne * taskOneCycles) / taskOneTimes ) ; // totalTime
    lcd.clear();    
    lcd.setCursor(0,0);
    lcd.print("cpm=");
    lcd.print(cpm);
    lcd.setCursor(8,0);
    lcd.print("avg=");
    lcd.print(averageCpmForTask1);
    lcd.setCursor(2,1);
    lcd.print(microSieverts,5);
    lcd.setCursor(8,1);
    lcd.print(" uSv/h");
    clicks = 0;


    if (DEBUG) {
      Serial.print(taskOneInterval * taskOneTimes);
      Serial.print(" secs");
      Serial.print(" - ");
      Serial.print("clicks=");
      Serial.print(clicksPerPeriodOne);
      Serial.print(" - ");
      Serial.print("CPM = "); 
      Serial.print(cpm,DEC);
      Serial.print(" -===- ");
      Serial.print("avg=");
      Serial.print(averageCpmForTask1, DEC);
      Serial.print(" -===- ");
      Serial.print("uSv/h = ");
      Serial.println(microSieverts,5);
    }    
    if (taskOneTimes == taskOneCycles) {

    clicksPerPeriodOne = 0;
    averageCpmForTask1 = 0;
    taskOneCycles = 0;
    }


} // end of measurementTask


// --------------- Report the values tio the network

static void reportingTask() {
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
  // safeguards: detaching the interrupt for a while and using volatile variables
  detachInterrupt(0);
  clicks++;
  clicksPerPeriodOne++;
  clicksPerPeriodTwo++;
  totalClicks++;
  while(digitalRead(2)==0){
  }
  attachInterrupt(0,irqService,FALLING);
}


// =============== Foreign functions (copied from elsewhere and possibly modded) ================

// --------- raw_http_reply() - returns what the server answered and how fast
static void raw_http_reply (byte status, word off, word len) {
  if (DEBUG) {
  Serial.print(F("<<< reply "));
  Serial.print(millis() - timer);
  Serial.println(F(" ms"));
  Serial.println(F("-----------------"));
  Serial.println((const char*) Ethernet::buffer + off);
  Serial.println(F("-----------------"));
  }
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

/* ========================================================================================
  https://github.com/PaulStoffregen/Time/blob/master/examples/TimeNTP_ENC28J60/TimeNTP_ENC28J60.ino */

/*-------- NTP code ----------*/

// SyncProvider that returns UTC time
time_t getNtpTime() {
  // Send request
  Serial.println("Transmit NTP Request");
  if (!ether.dnsLookup(timeServer)) {
    Serial.println("DNS failed");
    return 0; // return 0 if unable to get the time
  } else {
    //ether.printIp("SRV: ", ether.hisip);
    ether.ntpRequest(ether.hisip, remoteNtpPort);
  
    // Wait for reply
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      word len = ether.packetReceive();
      ether.packetLoop(len);

      unsigned long secsSince1900 = 0L;
      if (len > 0 && ether.ntpProcessAnswer(&secsSince1900, remoteNtpPort)) {
        Serial.println("Receive NTP Response");
        return secsSince1900 - 2208988800UL;
      }
    }
    
    Serial.println("No NTP Response :-(");
    return 0;
  }
}

/* Alternative SyncProvider that automatically handles Daylight Saving Time (DST) periods,
 * at least in Europe, see below.
 */
 
time_t getDstCorrectedTime (void) {
  time_t t = getNtpTime ();

  if (t > 0) {
    TimeElements tm;
    breakTime (t, tm);
    t += (utcOffset + dstOffset (tm.Day, tm.Month, tm.Year + 1970, tm.Hour)) * SECS_PER_HOUR;
  }

  return t;
}

/* This function returns the DST offset for the current UTC time.
 * This is valid for the EU, for other places see
 * http://www.webexhibits.org/daylightsaving/i.html
 * 
 * Results have been checked for 2012-2030 (but should work since
 * 1996 to 2099) against the following references:
 * - http://www.uniquevisitor.it/magazine/ora-legale-italia.php
 * - http://www.calendario-365.it/ora-legale-orario-invernale.html
 */
byte dstOffset (byte d, byte m, unsigned int y, byte h) {
  // Day in March that DST starts on, at 1 am
  byte dstOn = (31 - (5 * y / 4 + 4) % 7);

  // Day in October that DST ends  on, at 2 am
  byte dstOff = (31 - (5 * y / 4 + 1) % 7);

  if ((m > 3 && m < 10) ||
      (m == 3 && (d > dstOn || (d == dstOn && h >= 1))) ||
      (m == 10 && (d < dstOff || (d == dstOff && h <= 1))))
    return 1;
  else
    return 0;
}

