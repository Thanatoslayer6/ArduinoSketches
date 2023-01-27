// CONNECTIONS:
// DS1302 CLK/SCLK --> 5
// DS1302 DAT/IO --> 4
// DS1302 RST/CE --> 2
// DS1302 VCC --> 3.3v - 5v
// DS1302 GND --> GND

#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include <Servo.h>
#define countof(a) (sizeof(a) / sizeof(a[0]))

// PIN 8 = RST
// PIN 7 = DAT
// PIN 6 = CLK
ThreeWire myWire(7,6,8); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

// Create a servo object with the name of 'dispenser'
Servo dispenser;

void initializeTime() {
    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    //Serial.println();
    
    if (!Rtc.IsDateTimeValid()) {
        // Common Causes:
        // 1) first time you ran and the device wasn't running yet
        // 2) the battery on the device is low or even missing
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected()) {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning()) {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    } else if (now > compiled) {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }  
}

void printDateTime(const RtcDateTime& dt) {
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}


void setup () {
    Serial.begin(9600);
    // Initialize Time library for DS1302
    initializeTime();
    // Initialize LEDs
    pinMode(12, OUTPUT);
    pinMode(13, OUTPUT);
    // Initialize servo
    dispenser.attach(9);
    dispenser.write(0);  
}

// Declare some global variable to always reset
// boolean turned = false;
//int pos = 0;

void loop () {
    digitalWrite(13, LOW);
    digitalWrite(12, HIGH);
    //digitalWrite(13, HIGH);
    RtcDateTime now = Rtc.GetDateTime();

    printDateTime(now);
    Serial.println();
    //Serial.println(now.Minute());
    
    //digitalWrite(12, HIGH); // turn the LED on (HIGH is the voltage level)
    //delay(1000); // wait for a second
    //digitalWrite(12, LOW); // turn the LED off by making the voltage LOW
    //delay(1000); // wait for a second
    
    // Check time, like every 5 minutes move the servo or something
    if ((now.Minute() % 5 == 0)) {
      digitalWrite(12, LOW);
      digitalWrite(13, HIGH);
      dispenser.write(180);
      // Stop for 1 minute
      delay(60000);
      // Reset
      dispenser.write(0);
    }
    
    

    if (!now.IsValid()) {
        // Common Causes:
        // 1) the battery on the device is low or even missing and the power line was disconnected
        Serial.println("RTC lost confidence in the DateTime!");
    }

    delay(5000); // five seconds
}
