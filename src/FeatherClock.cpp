//
//   Copyright (C) 2017 Ronald Guest <http://about.me/ronguest>
//
//  Alarm time of 0000 is a disabled alarm
//  Alarm time of 9999 means to continuously sound the alarm
//
#include <Arduino.h>
#include "Clock.h"

void setup() {
  Serial.begin(115200);                           // Start the serial console
  delay(100);
  Serial.println(F("Clock starting!"));              // Start the clock message.

  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Set up debouncer library for the alarm off button
  pinMode(buttonPIN, INPUT);
  debouncer.attach(buttonPIN);
  debouncer.interval(20);

  // Set up the display.
  clockDisplay.begin(DISPLAY_ADDRESS);
  clockDisplay.print(0, DEC);
  clockDisplay.writeDisplay();

  // Connect to WiFi access point.
  Serial.print(F("Connecting to ")); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  clockDisplay.print(1, DEC);
  clockDisplay.writeDisplay();
  // Get current time using NTP
  Udp.begin(localPort);
  ntpTime = getNtpTime();
  if (ntpTime != 0) {
    setTime(ntpTime);
  } else {
    Serial.println(F("Failed to set the initial time"));
  }

  clockDisplay.print(2, DEC);
  clockDisplay.writeDisplay();
  // Initialize the Music Maker wing library
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1) {
       delay(10);  // we're done! do nothing...
     }
  }
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working

  clockDisplay.print(3, DEC);
  clockDisplay.writeDisplay();
  // Initialize the SD card
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1) {
      delay(10);  // we're done! do nothing...
    }
  }
  Serial.println("SD OK!");
  musicPlayer.setVolume(1,1);       // Would be nice to have a volume setting option

  clockDisplay.print(4, DEC);
  clockDisplay.writeDisplay();
  // Get the URL to the alarm time
  if (readFile()) {
    Serial.print(F("alarmURL is: ")); Serial.println(alarmURL);
  } else {
    Serial.println(F("Unable to read alarmURLFile"));
    // Might as well just loop for now, monkey with display to show error
    clockDisplay.print(9999, DEC);
    clockDisplay.writeDisplay();
    while (1) {delay(10);}
  }

  clockDisplay.print(5, DEC);
  clockDisplay.writeDisplay();
  #if defined(__AVR_ATmega32U4__)
    // Timer interrupts are not suggested, better to use DREQ interrupt!
    // but we don't have them on the 32u4 feather...
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
  #elif defined(ESP32)
    // no IRQ! doesn't work yet :/
  #else
    // If DREQ is on an interrupt pin we can do background
    // audio playing
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  #endif

  clockDisplay.print(6, DEC);
  clockDisplay.writeDisplay();
}

void loop() {
  // Get the time in local timezone taking DST into account
  local = usCT.toLocal(now(), &tcr);
  hours = hour(local);
  minutes = minute(local);
  seconds = second(local);
  dayOfWeek = weekday(local);
  displayValue = (hours * 100) + minutes;

  // At bootup and at the top of every hour check the alarm time from the server for changes
  if ((previousHour != hours) || (startUp)) {
    startUp = false;
    previousHour = hours;
    getAlarmTime(alarmURL);     // We ignore any errors because we will just use the last fetched time
    Serial.print(F("Alarm time from URL is: "));
    Serial.print(alarmHour); Serial.println(alarmMinute);
    // Try an NTP time sync once an hour, no big deal if it fails occassionally
    ntpTime = getNtpTime();
    if (ntpTime != 0) {
      setTime(ntpTime);
    } else {
      Serial.println(F("NTP sync failed"));
    }
  }

  // Configure for 24 vs 12 hour display value
  if (!TIME_24_HOUR) {
    if (hours > 12) {
      displayValue -= 1200;
    }
    else if (hours == 0) {
      displayValue += 1200;
    }
  }

  // Print the time on the display
  clockDisplay.print(displayValue, DEC);

  // Add zero padding when in 24 hour mode and it's midnight.
  // In this case the print function above won't have leading 0's
  // which can look confusing.  Go in and explicitly add these zeros.
  if (TIME_24_HOUR && hours == 0) {
    // Pad hour 0.
    clockDisplay.writeDigitNum(1, 0);
    // Also pad when the 10's minute is 0 and should be padded.
    if (minutes < 10) {
      clockDisplay.writeDigitNum(2, 0);
    }
  }

  currentMillis = millis();
  // Toggle the colon by flipping its value every colonToggleDelay ms
  if ((currentMillis - previousMillis) > colonToggleDelay) {
    previousMillis = currentMillis;
    blinkColon = !blinkColon;
  }
  // We have to re-draw the colon on every iteration else it is erased
  clockDisplay.drawColon(blinkColon);

  // Now push out to the display the new values that were set above.
  clockDisplay.writeDisplay();

  if (alarmPlaying) {
    // Update the state of the button (debouncer is not interrupt drive so this is polling)
    debouncer.update();
    if (((currentMillis - alarmStart) > alarmDuration) || (debouncer.read() == LOW)) {
      // Alarm has been on long enough or user pushed the button so stop the audio
      if ((currentMillis - alarmStart) > alarmDuration) {
        Serial.println("Stop alarm playing due to duration");
      } else {
        Serial.println("Stop alarm playing due to button press");
      }
      alarmPlaying = false;
      musicPlayer.stopPlaying();
    } else if (musicPlayer.stopped()) {
      // We finished playing the song but the user has not yet hit the button/woken up
      // Play an alert?
      Serial.println("Play the alert sound since user is not up yet and time has not expired");
      musicPlayer.startPlayingFile(alarmAlert);
    }
  }

 // If we've hit the HH:MM time for the alarm so turn it on, unless it is sounding
 // ****************** Replace the digitalWrite code the coded needed to sound a play list
  if (alarmTime() && !alarmPlaying) {
    // Only want to print this message the first time we start playing
    Serial.println("Playing alarm");
    alarmStart = currentMillis;
    // Start playing the alarm for a fixed amount of time
    musicPlayer.startPlayingFile(alarmSong);
    alarmPlaying = true;
  }
}

// Connect to the server and read the alarm time
void getAlarmTime(String url) {
  WiFiClient client;
  const int httpPort = 80;
  String server;
  String filePart;      // Holds the part of the URL after the server name
  String response;

  server = url.substring(0, url.indexOf('/'));
  filePart = url.substring(url.indexOf('/'));
  //Serial.print("Server: " + server);
  //Serial.println(", Filepart: " + filePart);

  HttpClient http = HttpClient(client, server, httpPort);
  http.get(filePart);

  // read the status code and body of the response
  int responseCode = http.responseStatusCode();
  Serial.print("statusCode: "); Serial.println(responseCode);
  if (responseCode != 200) {
    Serial.println("Non-success return code");
    // Light the Red LED if fails
    digitalWrite(ledPin, HIGH);
    return;
  }
  response = http.responseBody();
  //Serial.print("response length: "); Serial.println(response.length());
  //Serial.print("response: "); Serial.println(response);

  String hour = response.substring(0,2);
  String minute = response.substring(2,4);

  Serial.print("Alarm hour: "); Serial.println(atoi(hour.c_str()));
  Serial.print("Alarm minute: "); Serial.println(atoi(minute.c_str()));
  alarmHour = atoi(hour.c_str());
  alarmMinute = atoi(minute.c_str());
  digitalWrite(ledPin, LOW);
}

boolean alarmTime() {
  // Sound continuously if debugging
  if ((alarmHour == 99) && (alarmMinute == 99)) return true;

  // Never sound if the alarm is set for 00:00 (which is midnight...)
  if ((alarmHour == 0) && (alarmMinute == 0)) return false;

  // Never sound on the Saturday & Sunday
  if ((dayOfWeek == 1) || (dayOfWeek == 7)) {
    // Remind the poor developer that the alarm is off on the weekend...
    if (minutes != previousMinute) {
      Serial.println("***** Alarm disabled, it's the weekend");
      previousMinute = minutes;
    }
    return false;
  }
  /*if (minutes != previousMinute) {
    Serial.println("******** Debugging mode: alarm WILL SOUND on the weekend");
    previousMinute = minutes;
  }*/

  // We check the seconds so that if the user hits the silence button within the first minute the alarm doesn't turn on again
  if ((hours == alarmHour) && (minutes == alarmMinute) && (seconds < 5)) {
    //Serial.println("We hit the alarm time");
    return true;
  } else {
    return false;
  }
}

// Reads the string value from the specified file
// Returns true on success -- SD.open("AURL.txt"), alarmURL)
boolean readFile() {
  //alarmURL = "www.farsidetechnology.com/ashley_alarm.php";
  File dataFile = SD.open("AURL.txt", FILE_READ);
  if (dataFile) {
    Serial.println(F("Reading AURL.txt"));
    while (dataFile.available()) {
      char c = dataFile.read();
      if (c == '\n') {
        break;
      } else {
        alarmURL += c;
      }
    }
    dataFile.close();
    return true;
  } else {
    Serial.print(F("Failed to open AURL.txt"));
    return false;
  }
}

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address
  uint32_t beginWait;
  int size;

  if(WiFi.status() == WL_CONNECTED) {
    while (Udp.parsePacket() > 0) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // discard any previously received packets, I made this change not sure if needed though
    }
    Serial.print("Transmit NTP Request ");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName); Serial.print(": "); Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    beginWait = millis();
    while (millis() - beginWait < 1500) { // Extending wait from 1500 to 2-3k seemed to avoid the sync problem, but now it doesn't help
      size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Serial.println("Receive NTP Response");
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    Serial.println("No NTP Response");
    return 0; // return 0 if unable to get the time
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  int result;

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  result = Udp.beginPacket(address, 123); //NTP requests are to port 123
  result = Udp.write(packetBuffer, NTP_PACKET_SIZE);
  result = Udp.endPacket();
}
