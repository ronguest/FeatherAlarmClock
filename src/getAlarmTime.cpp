#include <Arduino.h>
#include <WiFi101.h>
#include "ArduinoHttpClient.h"

// Connect to the server and read the alarm time
// 0000 means no alarm
void getAlarmTime(String url) {
  WiFiClient client;
  const int httpPort = 80;
  String server;
  String filePart;      // Holds the part of the URL after the server name
  String response;

  server = url.substring(0, url.indexOf('/'));
  filePart = url.substring(url.indexOf('/'));
  Serial.print("Server: " + server);
  Serial.println(", Filepart: " + filePart);

  HttpClient http = HttpClient(client, server, httpPort);
  http.get(filePart);

  // read the status code and body of the response
  Serial.print("statusCode: "); Serial.println(http.responseStatusCode());
  response = http.responseBody();
  //Serial.print("response length: "); Serial.println(response.length());
  Serial.print("response: "); Serial.println(response);

  Serial.print("toInt of response: "); Serial.println(response.toInt());
  String hour = response.substring(0,2);
  String minute = response.substring(2,4);

  Serial.print("Alarm hour: "); Serial.println(atoi(hour.c_str()));
  Serial.print("Alarm minute: "); Serial.println(atoi(minute.c_str()));
  alarmHour = atoi(hour.c_str());
  alarmMinute = atoi(minute.c_str());
}
