#include <Arduino.h>

//
//  This is based on a Feather Huzzah, Adafruit Sound Board, and MAX98306 Stereo amp. Audio file is stored on the Sound Board.
//  Simply plug the board into my computer when the Feather is not powered up.
//
//  Alarm time of 0000 is a disabled alarm
//  Alarm time of 9999 means to continuously sound the alarm
//

#include "Clock.h"

void setup() {
  Serial.begin(115200);                           // Start the serial console
  Serial.println("Clock starting!");              // Start the clock message.

  // Configure pin for control alarm sound on the soundboard
  // It will continue to play the sound until the pin goes back high
  pinMode(alarmPIN, OUTPUT);
  digitalWrite(alarmPIN, HIGH);
  pinMode(buttonPIN, INPUT_PULLUP);
  debouncer.attach(buttonPIN);
  debouncer.interval(20);

  // Set up the display.
  clockDisplay.begin(DISPLAY_ADDRESS);

  // Tell the module not to advertise itself as an access point
  WiFi.mode(WIFI_STA);

  // Connect to WiFi access point.
  Serial.print(F("Connecting to ")); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  // Get current time using NTP
  Udp.begin(localPort);
  ntpTime = getNtpTime();
  if (ntpTime != 0) {
    setTime(ntpTime);
  } else {
    Serial.println("Failed to set the initial time");
  }

  // Start up with alarm disabled: hour and minute set to 00 means never sound the alarm
  alarmMinute = 0;
  alarmHour = 0;
}

void loop() {
  // Get the time in local timezone taking DST into account
  time_t local = usCT.toLocal(now(), &tcr);
  hours = hour(local);
  minutes = minute(local);
  seconds = second(local);
  dayOfWeek = weekday(local);
  //Serial.print("Weekday is ");Serial.println(dayOfWeek);
  int displayValue = hours*100 + minutes;
  unsigned long currentMillis = millis();

  debouncer.update();

  // At bootup and at the top of every hour read the alarm time
  if ((previousHour != hours) || (startUp)) {
    startUp = false;
    previousHour = hours;
    getAlarmTime(alarmURL);
    Serial.print("Alarm time set to: ");
    Serial.print(alarmHour);Serial.print(alarmMinute);Serial.println();
    // Try an NTP time sync
    ntpTime = getNtpTime();
    if (ntpTime != 0) {
      setTime(ntpTime);
    } else {
      Serial.println("NTP sync failed");
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

  // Blink the colon by flipping its value every loop iteration
  if ((currentMillis - previousMillis) > colonToggleDelay) {
    previousMillis = currentMillis;
    blinkColon = !blinkColon;
  }
  // We draw the colon on every iteration else it is erased
  clockDisplay.drawColon(blinkColon);

  // Now push out to the display the new values that were set above.
  clockDisplay.writeDisplay();

  // If the alarm is sounding, check how long it has been sounding and turn it off if it has been alarmDuration seconds
  // Also stops the alarm if the button is pushed (debounced)
  if (alarmPlaying) {
    alarmCounter++;
    if ((alarmCounter > alarmDuration) || (debouncer.read() == LOW)){
      // Time is up, turn off the alarm
      alarmPlaying = false;
      digitalWrite(alarmPIN, HIGH);
    }
  }

 // We've hit the HH:MM time for the alarm so turn it on, unless it is sounding
  if (alarmTime() && !alarmPlaying) {
    // Start playing the alarm for a fixed amount of time
    Serial.println("Playing alarm");
    digitalWrite(alarmPIN, LOW);
    alarmCounter = 0;
    alarmPlaying = true;
  }

  // Pause for a second for time to elapse.  This value is in milliseconds
  // so 1000 milliseconds = 1 second.
  // REPLACE WITH A TIME CHECK SO WE CAN MONITOR A PUSHBUTTON TO SILENCE THE ALARM MANUALLY
  //delay(1000);

}

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  if(WiFi.status() == WL_CONNECTED) {
    while (Udp.parsePacket() > 0) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // discard any previously received packets, I made this change not sure if needed though
    }
    Serial.print("Transmit NTP Request ");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) { // Extending wait from 1500 to 2-3k seemed to avoid the sync problem, but now it doesn't help
      int size = Udp.parsePacket();
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

// Connect to the server and read the alarm time
// 0000 means no alarm
void getAlarmTime(char* url) {
  HTTPClient http;
  String payload;

   http.begin(url);
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        //Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            payload = http.getString();
            //Serial.println(payload);
            alarmHour = atoi(payload.substring(0,2).c_str());
            alarmMinute = atoi(payload.substring(2,4).c_str());
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

boolean alarmTime() {

  // Sound continuously if debugging
  if ((alarmHour == 99) && (alarmMinute == 99)) return true;

  // Never sound if the alarm is set for 00:00 (which is midnight...)
  if ((alarmHour == 0) && (alarmMinute == 0)) return false;

  // Never sound on the Saturday & Sunday
  if ((dayOfWeek == 1) || (dayOfWeek == 7)) return false;

  // We check the seconds so that if the user hits the silence button quickly the alarm doesn't turn on again
  if ((hours == alarmHour) && (minutes == alarmMinute) && (seconds < 5)) {
    return true;
  } else {
    return false;
  }
}
