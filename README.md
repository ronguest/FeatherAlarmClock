# FeatherAlarmClock

This is based on a Feather Huzzah, Adafruit Sound Board, and MAX98306 Stereo amp. Audio file is stored on the Sound Board.
Simply plug the board into my computer when the Feather is not powered up.

Alarm time of 0000 is a disabled alarm
Alarm time of 9999 means to continuously sound the alarm

It requires a URL (php assumed) on a server that returns the alarm time as 4 digits in 24 hour time
It requires a WiFi connection
Both are configured in alarm_setup. An example is provided since this info should not be made public
