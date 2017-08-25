//
//   Copyright (C) 2017 Ronald Guest <http://about.me/ronguest>
//
//  This is based on a Feather Huzzah, Adafruit Sound Board, and MAX98306 Stereo amp. Audio file is stored on the Sound Board.
//  Simply plug the board into my computer when the Feather is not powered up.
//
//  Alarm time of 0000 is a disabled alarm
//  Alarm time of 9999 means to continuously sound the alarm
//

#include <Arduino.h>
#include "Clock.h"

void setup() {
  Serial.begin(115200);                           // Start the serial console
  delay(100);
  Serial.println("Clock starting!");              // Start the clock message.

  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);
  debouncer.attach(buttonPIN);
  debouncer.interval(20);

  // Set up the display.
  clockDisplay.begin(DISPLAY_ADDRESS);

  clockDisplay.print(1, DEC);
  clockDisplay.writeDisplay();

  // Connect to WiFi access point.
  Serial.print(F("Connecting to ")); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  clockDisplay.print(2, DEC);
  clockDisplay.writeDisplay();
  // Get current time using NTP
  Udp.begin(localPort);
  ntpTime = getNtpTime();
  if (ntpTime != 0) {
    setTime(ntpTime);
  } else {
    Serial.println(F("Failed to set the initial time"));
  }

  // Start up with alarm disabled: hour and minute set to 00 means never sound the alarm
  alarmMinute = 0;
  alarmHour = 0;

  clockDisplay.print(3, DEC);
  clockDisplay.writeDisplay();
  if (readFile()) {
    Serial.print(F("alarmURL is: ")); Serial.println(alarmURL);
    //urlFile.close();
  } else {
    Serial.println(F("Unable to read alarmURLFile"));
    // Might as well just loop for now, monkey with display to show error later
    // Maybe show 66:66 as the time?
    while (1) {delay(1000);}
  }

  clockDisplay.print(4, DEC);
  clockDisplay.writeDisplay();
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1) {
       delay(10);  // we're done! do nothing...
     }
  }
  musicPlayer.setVolume(60,60);
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working

  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1) {
      delay(10);  // we're done! do nothing...
    }
  }
  Serial.println("SD OK!");
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

  // Play a file in the background, REQUIRES interrupts!
  //Serial.println(F("Playing full track 001"));
  //musicPlayer.playFullFile("track001.mp3");
  //musicPlayer.playFullFile("T01.wav");
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
    Serial.print("Alarm time from URL is: ");
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

  //********
  //  Replace digitalWrite with a way to stop a playlist
  //  Reset the board if necessary
  //  Also replace the debouncer with a check on the value of the analog input
  //  Don't need the alarmCounter any more or to check duration
  if (alarmPlaying) {
    /*alarmCounter++;
    if ((alarmCounter > alarmDuration) || (debouncer.read() == LOW)){
      // Time is up, turn off the alarm
      alarmPlaying = false;
      //digitalWrite(alarmPIN, HIGH);
    }*/
  }

 // We've hit the HH:MM time for the alarm so turn it on, unless it is sounding
 // ****************** Replace the digitalWrite code the coded needed to sound a play list
  if (alarmTime() && !alarmPlaying) {
    // Start playing the alarm for a fixed amount of time
    Serial.println("Playing alarm");
    //digitalWrite(alarmPIN, LOW);
    //alarmCounter = 0;
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
void getAlarmTime(String url) {
  WiFiClient client;
  const int httpPort = 80;
  int endServer = -1;   // locates the third slash in the URL
  int hostStart = -1;   // locates the start of the hostname, after the leading protocol info (e.g. http://)
  String server;
  String host;
  String filePart;      // Holds the part of the URL after the server name
  String payload;       // string read from the server


  // ASSUMPTION: The third "/" is the end of the server name. There must be a file path after that, e.g. foo.php
  // This is based on http{s}://server.foo/something.php format
  /*endServer = url.indexOf('/');                // First /
  endServer = url.indexOf('/', endServer+1);    // Second /
  hostStart = endServer;    // So we can skip the protocol to grab just the hostname for the request
  endServer = url.indexOf('/', endServer+1);    // Third /
  // At this point we either know the locations of the 3rd slash or endServer = -1 which means we didn't find it
  if ((endServer == -1) || (hostStart == -1)) {
    Serial.print("Failed to parse server portion of URL"); Serial.println(url);
    return;
  }
  Serial.print("hostStart = "); Serial.println(hostStart);
  Serial.print("endServer = "); Serial.println(endServer);
  server = url.substring(hostStart+1, endServer);
  host = url.substring(hostStart+1, endServer);
  filePart = url.substring(endServer+1);*/

  //url = "www.arduino.cc/latest.txt";

  server = url.substring(0, url.indexOf('/'));
  filePart = url.substring(url.indexOf('/'));

  Serial.print("Server: " + server);
  Serial.println(", Filepart: " + filePart);
  //Serial.print(F("Parsed host name of: ")); Serial.println(host);

  //char server2[] = "www.arduino.cc";
  if (client.connect(server.c_str(), 80)) {
    Serial.println(F("Connected to server"));
    client.println("GET " + filePart + " HTTP/1.1");
    client.println("Host: " + server);
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();

    char c;
    Serial.println("Wait for client data");
    while (!client.available()) {
      //char c = client.read();
      //Serial.write(c);
    }
    while(client.available()) {
      c = client.read();
      Serial.write(c);
      payload += c;
    }
    Serial.println("<end>");
    client.stop();

    alarmHour = atoi(payload.substring(0,2).c_str());
    alarmMinute = atoi(payload.substring(2,4).c_str());
  } else {
    Serial.println("Failed to connect to server");
  }
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

// Reads the string value from the specified file
// Returns true on success -- SD.open("AURL.txt"), alarmURL)
boolean readFile() {
  alarmURL = "www.farsidetechnology.com/ashley_alarm.php";
}
