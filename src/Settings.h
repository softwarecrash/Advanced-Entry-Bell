//Settings: Stores persistant settings, loads and saves to EEPROM

#include <Arduino.h>
#include <EEPROM.h>

class Settings
{
public:
  bool _valid = false;
  String deviceName;
  short coolDownTime;    
  short bellSignalTime;    
  short signalTimeout;
  String rtspUrl = "";  //mqtt passwort

  short readShort(int offset)
  {
    byte b1 = EEPROM.read(offset + 0);
    byte b2 = EEPROM.read(offset + 1);
    return ((short)b1 << 8) | b2;
  }

  void writeShort(short value, int offset)
  {
    byte b1 = (byte)((value >> 8) & 0xFF);
    byte b2 = (byte)((value >> 0) & 0xFF);

    EEPROM.write(offset + 0, b1);
    EEPROM.write(offset + 1, b2);
  }

  void readString(String &s, int maxLen, int offset)
  {
    int i;
    s = "";
    for (i = 0; i < maxLen; ++i)
    {
      char c = EEPROM.read(offset + i);
      if (c == 0)
        break;
      s += c;
    }
  }

  void writeString(String &s, int maxLen, int offset)
  {
    int i;
    //leave space for null termination
    maxLen--;
    if ((int)s.length() < maxLen - 1)
      maxLen = s.length();

    for (i = 0; i < maxLen; ++i)
    {
      EEPROM.write(offset + i, s[i]);
    }
    //null terminate the string
    EEPROM.write(offset + i, 0);
  }

  void load()
  {
    EEPROM.begin(512);

    _valid = true;
    _valid &= EEPROM.read(0) == 0xDB;
    _valid &= EEPROM.read(1) == 0xEE;
    _valid &= EEPROM.read(2) == 0xAE;
    _valid &= EEPROM.read(3) == 0xDF;

    if (_valid)
    {
      coolDownTime = readShort(0x20);
      bellSignalTime = readShort(0x60);
      signalTimeout = readShort(0x40);
      readString(deviceName, 0x20, 0x80);
      readString(rtspUrl, 0x20, 0xE0);
      
    }

    EEPROM.end();
  }

  void save()
  {
    EEPROM.begin(512);

    EEPROM.write(0, 0xDB);
    EEPROM.write(1, 0xEE);
    EEPROM.write(2, 0xAE);
    EEPROM.write(3, 0xDF);

    writeShort(coolDownTime, 0x20);
    writeShort(bellSignalTime, 0x60);
    writeShort(signalTimeout, 0x40);
    writeString(deviceName, 0x20, 0x80);
    writeString(rtspUrl, 0x20, 0xE0);
    EEPROM.commit();

    _valid = true;

    EEPROM.end();
  }

  void reset(){
  deviceName = "";
  coolDownTime = 2000;
  bellSignalTime = 1500;
  signalTimeout = 1000;
  save();
  }

  Settings()
  {
    load();
  }
};
