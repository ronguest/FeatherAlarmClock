
//   Copyright (C) 2017 Ronald Guest <http://about.me/ronguest>

#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <SPI.h>                              // SPI interface API
#include <Wire.h>                             // Wire support library
#include "Adafruit_LEDBackpack.h"             // Support for the LED Backpack FeatherWing
#include "Adafruit_GFX.h"                     // Adafruit's graphics library
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "Bounce2.h"
#include "alarm_setup.h"

int alarmPIN = 13;          // Take low to turn on sound
int buttonPIN = 12;         // If pressed turn off the alarm

long alarmDuration = 300000;      // Duration of alarm in milliseconds (5 minutes)
int alarmCounter;           // Measures how long alarm has been sounding
boolean alarmPlaying = false;
long previousMillis = 0;
long colonToggleDelay = 500;  // Millis to delay before flashing the colon on the display, 500 = half second

Bounce debouncer = Bounce();

#define TIME_24_HOUR    false
#define DISPLAY_ADDRESS 0x70

 //US Central Time Zone (Chicago, Houston)
//static const char ntpServerName[] = "us.pool.ntp.org";
static const char ntpServerName[] = "time.nist.gov";
const int timeZone = 0;     // Using the Timezone library now which does it's own TZ and DST correction
TimeChangeRule usCDT = {"CDT", Second, dowSunday, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, dowSunday, Nov, 2, -360};
Timezone usCT(usCDT, usCST);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t local;

////// Code and variables for NTP syncing
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
int ntpTime;
// Create an ESP8266 WiFiClient class to connect to the AIO server.
WiFiClient client;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// Create display object
Adafruit_7segment clockDisplay = Adafruit_7segment();

int hours = 0;                      // Track hours
int minutes = 0;                    // Track minutes
int seconds = 0;                    // Track seconds
int dayOfWeek = 0;                  // Sunday == 1
int previousHour = 0;
//int tzOffset = -5;                  // Time zone offset

bool blinkColon = false;            // Track the status of the colon to blink every second

int status = WL_IDLE_STATUS;        // WINC1500 chip status

int alarmMinute;
int alarmHour;
boolean startUp = true;
void getAlarmTime(char*);
boolean alarmTime();
