// Reads the string value from the specified file
// Returns true on success
boolean readFile(File dataFile, String& value) {
  if (dataFile) {
    while (dataFile.available() > 0) {
      char c = dataFile.read();
      if (c == '\n') {
        break;
      } else {
        value += c;
      }
    }
    dataFile.close();
    return true;
  } else {
//    Console.print(F("File open err"));
    return false;
  }
}
