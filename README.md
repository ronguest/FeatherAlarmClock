Feather Alarm Clock
========

* This is based on a Feather Huzzah, Adafruit Sound Board, and MAX98306 Stereo amp
* The audio file is stored on the Sound Board (must be a small file as the storage is small)
* Simply plug the sound board into a computer when the Feather is not powered up to copy to it as a storage device

* An alarm time of 0000 is a disabled alarm
* An alarm time of 9999 means to continuously sound the alarm

* It requires a URL (php assumed) on a server that returns the alarm time as 4 digits in 24 hour time (e.g. 0600 for 6am)

* It requires a WiFi connection

* Both are configured in alarm_setup. An example is provided since this info should not be made public
