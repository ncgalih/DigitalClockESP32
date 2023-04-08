#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define GMTsec 3600*7

const char* NTPServer = "pool.ntp.org";
const char* ssid = "SSID";
const char* pass = "Password";
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

class Alarm {
  public:
  time_t timer;
  bool active;
  bool check(time_t now){
    if(!active) return false;
    if(now < timer) return false;
    active = false;
    return true;
  }
  void set(tm* alarm, tm* now){
    time_t timenow = mktime(now);
    timer = timenow - timenow % (3600*24) + daysec(alarm) - GMTsec;
    if(timenow > timer) timer += (3600*24);
    active = true;
  }
  tm* info(){
    return localtime(&timer);
  }
  static time_t daysec(tm* t){
    return t->tm_hour*3600 + t->tm_min*60 + t->tm_sec;
  }
};

class Key {
  public:
  bool State = HIGH;
  bool LastState = HIGH;
  long LastDebounceTime;
};
class Keypad {
  public:
    int value;
    Key keys[4][4];
    int *colPin;
    int *rowPin;
    void setup(){
      if(!colPin || !rowPin) return;
      for(int i = 0; i < 4; i++){
        pinMode(colPin[i], OUTPUT);
        pinMode(rowPin[i], INPUT_PULLUP);
      }
    }
    void ReadKeyPad() {
      value = -1;
      for(int c = 0; c < 4; c++)
      {
        digitalWrite(colPin[c], LOW);
        for(int r = 0; r < 4; r++)
        {
          Key* key = &keys[r][c];
          bool changed = ReadKeyState(key, rowPin[r]);     
          if(!changed) continue;
          if(!key->State) continue;
          if(c == 1 && r == 3)
            value = 0;
          else if(c < 3 && r < 3)
            value = (r*3) + c + 1;
          else if(c == 3)
            value = 10 + r;
          else
            value = 20 + c;
          return;
        }
        digitalWrite(colPin[c], HIGH);
      }
    }
  private:
    bool ReadKeyState(Key* key, int pin){
      int read = digitalRead(pin);
      if (read != key->LastState){
        key->LastDebounceTime = millis();
      }
      if((millis() - key->LastDebounceTime) > 20){
        if(key->State != (read == LOW)){
          key->State = (read == LOW); 
          return true;
        }
      }
      key->LastState = read;
      return false;
    }
};

class SettingAlarm {
  public:
    tm* timeinfo;
    SettingAlarm(Alarm* _alarm){
      timeinfo = _alarm->info();
      display();
    }
    void display(){
      oled.clearDisplay();
      oled.setCursor(0, 8);
      oled.setTextSize(1);
      oled.println("Set Alarm\n");
      oled.setTextSize(2);
      oled.println(timeinfo, "%H:%M:%S");
      oled.println(space((typestate/2)*3 + typestate%2) + "^");
      oled.display();
    }
    bool display(int input){
      display();
      if(input == 22) typestate++;
      if(input == 20) typestate--;
      if(input < 0 ||  input > 9) return false;
      switch (typestate) {
        case 0: timeinfo->tm_hour = input*10; break;
        case 1: timeinfo->tm_hour += input;   break;
        case 2: timeinfo->tm_min = input*10;  break;
        case 3: timeinfo->tm_min += input;    break;
        case 4: timeinfo->tm_sec = input*10;  break;
        case 5: timeinfo->tm_sec += input;    return true;
      }
      typestate++;
      return false;
    }
    static String space(int count){
      String str = "";
      for(int i=0; i<count; i++)
        str += " ";
      return str;
    }
  private:
    int typestate = 0;
};

class Led {
  private:
    int pin;
    bool state;
    bool on = false;
    long lastBlink = 0;
  public:
    long interval = 250;
    void setup(int pin){
      this->pin = pin;
      pinMode(pin, OUTPUT);
    }
    void turnOn(){ 
      on = true; 
    }
    void turnOff(){
      on = false;
      digitalWrite(pin, LOW);
    }
    void loop(){
      if(!on) return;
      if((millis() - lastBlink) < (interval/2)) return;
      state = !state;
      digitalWrite(pin, state);      
      lastBlink = millis();      
    }
};

Keypad keypad;
tm timeinfo;
time_t lasttime;
Alarm alarm1;
SettingAlarm* settingAlarm;
Led led;

void displayTime(tm* timeinfo){
  oled.clearDisplay();
  oled.setCursor(0, 8);
  oled.setTextSize(1);
  oled.println(timeinfo, "%A\n\n%d %B %Y\n");
  oled.setTextSize(2);
  oled.println(timeinfo, "%H:%M:%S");
  oled.display();
}

void setup() {
  WiFi.begin(ssid, pass);

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextColor(WHITE);

  led.setup(18);
  
  keypad.colPin = new int[4] {26, 25, 33, 32}; //{27, 14, 12, 13};
  keypad.rowPin = new int[4] {13, 12, 14, 27};
  keypad.setup();

  configTime(GMTsec, 0, NTPServer);
}

void loop() {
  // put your main code here, to run repeatedly:
  keypad.ReadKeyPad();

  if(!getLocalTime(&timeinfo)) return;
  time_t now = mktime(&timeinfo); 
 
  if(keypad.value == 10) {
    if(settingAlarm == nullptr)
      settingAlarm = new SettingAlarm(&alarm1);
    else
      settingAlarm = nullptr;
    return;
  }
  if(settingAlarm != nullptr) {
    bool done = settingAlarm->display(keypad.value);  
    if(!done) return;  
    alarm1.set(settingAlarm->timeinfo, &timeinfo);
    settingAlarm = nullptr;    
  }

  if(now != lasttime) displayTime(&timeinfo);
  lasttime = now;  

  if(alarm1.check(now)) led.turnOn();
  if(keypad.value == 13) led.turnOff();

  led.loop();
}
