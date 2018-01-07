/*
* Name: Room Temperature Clock Device
*
* Description: Arduino temperature and clock device shield.
*   Features:
*     Displays temperature in Fahrenheit and Celsius
*     Displays Date and Time
*     Change Date and Time
*     Change RGB Light Strip Colors
*     Push buttons to interact with device
*     Custom greetings
*     Display dims on/off with environment light and human proximity to device
*     Date/Time stays updated even when device is unplugged
*     Buzzer sounds
*
*   Components:
*     Arduino Mega
*     20x4 LCD 
*     Tactile push buttons
*     HC-SR04 Ultrasonic Sensor
*     Photo Light Cell
*     DS18b20 Temperature Sensor
*     DS3231 Time Module
*     Buzzer
*     RGB Led Light Strip
*
* Author: Amar Bhatt
*/

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library
#include <OneWire.h>

// LCD library
#include <LiquidCrystal.h>

#include <TempSense.h>

#include "pitches.h"

#include "Wire.h"

#include "FastLED.h"

#define DS3231_I2C_ADDRESS 0x68
//http://tronixstuff.com/2014/12/01/tutorial-using-ds1307-and-ds3231-real-time-clock-modules-with-arduino/


#define LIGHT_THRESH  700
#define DIST_THRESH 30
#define ACTIVITY_THRESH 30000
#define PIN_FREQ 31

#define DATA_PIN 53
#define NUM_LEDS 60

//RGB LED Strip
CRGB leds[NUM_LEDS];
CRGB colors[4] = {CRGB::Black, CRGB::Red, CRGB::Green, CRGB::Blue};
String colorList[4] = {"Off","Red","Green","Blue"};

// Set I/O pins
int tempPin = 10; // pin for DS18b20 temperature sensor
int LCDTogglePin = 6; // turn LCD on/off
int distTrigPin = 9; // ultrasonic trigger pin
int distEchoPin = 8; // ultrasonic echo pin
int lightSensePin = A8; // environment light sense pin for photocell
int buzzPin = 23; // pin for buzzer

// Variables
int timeout = 5*DIST_THRESH*29.1*2; //timeout for ultrasonic set to 5x the distance threshold
int lastOn = 0; // last time LCD was on
long current; // current time since device was on

volatile long long timeoutPress = 1000; // 3 seconds
volatile long long last_change_time = 0; // button debouncer

// Constant arrays
String monthList[12] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
String weekDayList[12] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

// Interrupt Pins
int menuInterrupt = 18;
int selectInterrupt = 19;
int selectUpPin = 17;
int selectDownPin = 16;

// Main loop booleans
volatile bool setBool = false;
volatile bool selectOption = false;
volatile bool setColor = false;

float celsius, fahrenheit;
// Variables for OneWire library
byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];  
int16_t raw;

// sensor variables
long distance;
bool light;

// notes in the melody:
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

// Define teperature sensor pin
OneWire  ds(tempPin);  // on pin 10 (a 4.7K resistor is necessary)

// Create new LCD display on pins 12, 11, 5, 4, 3, 2
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


// Function Prototypes
long getDistance();


//Initialize
void setup(void) {
  Wire.begin();
  // set the initial time here:
  // DS3231 seconds, minutes, hours, day, date, month, year
  //setDS3231time(0,6,8,5,12,10,17);
  lcd.begin(20, 4); // 20x4 LCD
  lcd.clear(); //reset lcd

  //LCD on/off
  pinMode(LCDTogglePin, OUTPUT);
  digitalWrite(LCDTogglePin, HIGH);
  lastOn = millis();

  //Initialize Ultrasonic
  pinMode(distTrigPin, OUTPUT);
  pinMode(distEchoPin, INPUT);

  //Initialize Buzzer
  pinMode(buzzPin, OUTPUT);
  noTone(buzzPin);

  //Initialize Temperature Sensor  
  int stat = connectTemperatureSensor(&ds, addr, &type_s);

  //Initalize interrupts
  attachInterrupt(digitalPinToInterrupt(menuInterrupt), changeTimeDate, RISING);
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), changeColors, RISING);
  
  //Initialize Buttons
  pinMode(selectDownPin,INPUT);
  pinMode(selectUpPin, INPUT);

  // Play sound on Buzzer
  playTone();
  clearLine(0);

  // Set up RGB light strip
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  changeRGBColor(1);
  delay(500);
  changeRGBColor(2);
  delay(500);
  changeRGBColor(3);
  delay(500);
  changeRGBColor(0);
  delay(500);
}

void loop(void) {
  
  if(!setBool) { // Regular loop
    if (setColor) { //Change RGB color
      changeRGBLights();
      setColor = false;
    }         
    distance = getDistance(); //Human proximity to device
    light = isLightOn(); // environment light
    current = millis();
    displayTemp();
    displayTime();
     
    if(light || distance < DIST_THRESH) {
      digitalWrite(LCDTogglePin, HIGH);
      lastOn = millis();
    } else if (!light && distance >= DIST_THRESH && (current-lastOn) < ACTIVITY_THRESH){
      digitalWrite(LCDTogglePin, HIGH);
    } 
    else { // turn off LCD
      digitalWrite(LCDTogglePin, LOW);
    }
  } else { // Change time
    changeTime();
  }
  
}

void changeTime() {
  byte second = 0;
  byte minute;
  byte hour;
  byte dayOfWeek;
  byte dayOfMonth;
  byte month;
  byte year;

  detachInterrupt(digitalPinToInterrupt(selectInterrupt));

  digitalWrite(LCDTogglePin, HIGH);
  
  lcd.clear();
  lcd.print("Change Time/Date");
  
  //Get Month
  clearLine(1);
  lcd.print("Select Month:");
  clearLine(2);
  lcd.print("January");
  month = 0;
  
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      month++;
      if(month >= 12) {
        month = 0;
      }   
      lcd.print(monthList[month]);    
    } else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      if(month == 0) {
        month = 11;
      } else {
        month--;
      }
      lcd.print(monthList[month]); 
    }
    
  }
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  month++;
  
  //Get Day
  clearLine(1);
  lcd.print("Select Day:");
  clearLine(2);
  dayOfMonth = 1;
  lcd.print(dayOfMonth,DEC); 
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      dayOfMonth++;
      if(dayOfMonth >= 32) {
        dayOfMonth = 1;
      }
      lcd.print(dayOfMonth,DEC);
    }else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      dayOfMonth--;
      if(dayOfMonth == 0) {
        dayOfMonth = 31;
      }
      lcd.print(dayOfMonth,DEC);
    }
    
  }
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  
  //Get Year
  clearLine(1);
  lcd.print("Select Year:");
  clearLine(2);
  int yearCounter = 2000;
  lcd.print(yearCounter,DEC);
  selectOption = false;
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      yearCounter++;
      if(yearCounter > 2255) {
        yearCounter = 2000;
      }
      lcd.print(yearCounter,DEC);      
    }else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      yearCounter--;
      if(yearCounter < 2000) {
        yearCounter = 2255;
      }
      lcd.print(yearCounter,DEC); 
    }
    
  }
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  year = yearCounter - 2000;
  
  //Get Day of Week
  clearLine(1);
  lcd.print("Select Day of Week:");
  clearLine(2);
  lcd.print("Sunday");
  dayOfWeek = 0;
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      dayOfWeek++;
      if(dayOfWeek >= 7) {
        dayOfWeek = 0;
      }   
      lcd.print(weekDayList[dayOfWeek]);    
    } else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      if(dayOfWeek == 0) {
        dayOfWeek = 6;
      } else {
        dayOfWeek--;
      }
      lcd.print(weekDayList[dayOfWeek]); 
    }
    
  }
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  dayOfWeek++;
  
  //Get Hour
  clearLine(1);
  lcd.print("Select Hour (24h):");
  clearLine(2);
  hour = 1;
  lcd.print(hour,DEC);
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      hour++;
      if(hour > 24) {
        hour = 1;
      }
      lcd.print(hour,DEC);
    }else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      hour--;
      if(hour == 0) {
        hour = 24;
      }
      lcd.print(hour,DEC);
    }
    
  }

  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  
  //Get Minutes
  clearLine(1);
  lcd.print("Select Minutes:");
  clearLine(2);
  minute = 0;
  lcd.print(minute,DEC);
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      minute++;
      if(minute > 59) {
        minute = 0;
      }
      lcd.print(minute,DEC);
    }else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      if(minute == 0) {
        minute = 59;
      } else {
        minute--;
      }
      lcd.print(minute,DEC);
    }
    
  }
  
  setBool = false;
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  selectOption = false;
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), changeColors, RISING);

    
  setDS3231time(0,minute,hour,dayOfWeek,dayOfMonth,month,year);

  lcd.clear();
}

void displayTemp() {
  raw = readTempSensor(&ds, addr, data, &present, &type_s);
  celsius = convertTemp(true,raw);
  fahrenheit = convertTemp(false,raw);

  //Serial.print("Temp: ");
  //Serial.println(fahrenheit);
  
  clearLine(1);
  //lcd.setCursor(11,0);
  //lcd.print("Fahrenheit = ");
  lcd.print(fahrenheit);
  lcd.print((char)223);
  lcd.print("F    ");
  lcd.print(celsius);
  lcd.print((char)223);
  lcd.print("C");
}

long getDistance() {
  long duration, distance;
  digitalWrite(distTrigPin, LOW);  // Added this line
  delayMicroseconds(2); // Added this line
  digitalWrite(distTrigPin, HIGH);
  delayMicroseconds(10); // Added this line
  digitalWrite(distTrigPin, LOW);
  duration = pulseIn(distEchoPin, HIGH, timeout);
  //Serial.println(duration);
  if(duration == 0) {
    duration = 100000;
  }
  distance = (duration/2) / 29.1; //in CM
  //Serial.print("Distance: ");
  //Serial.println(distance);
  return distance;
}

bool isLightOn(){
//  int light = analogRead(lightSensePin);
//  if(light < LIGHT_THRESH) {
//    return true;
//  } else {
//    return false;
//  }
  digitalWrite(distEchoPin,LOW);
  //Serial.print("Light: ");
  //Serial.println(analogRead(lightSensePin));
  return analogRead(lightSensePin) > LIGHT_THRESH;
}

void playTone() {
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 8; thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(buzzPin, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(buzzPin);
  }
}

byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}
void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}
void readDS3231time(byte *second,
byte *minute,
byte *hour,
byte *dayOfWeek,
byte *dayOfMonth,
byte *month,
byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}
void displayTimeSerial()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  // send it to the serial monitor
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute<10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second<10)
  {
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Sunday");
    break;
  case 2:
    Serial.println("Monday");
    break;
  case 3:
    Serial.println("Tuesday");
    break;
  case 4:
    Serial.println("Wednesday");
    break;
  case 5:
    Serial.println("Thursday");
    break;
  case 6:
    Serial.println("Friday");
    break;
  case 7:
    Serial.println("Saturday");
    break;
  }
}

void displayTime()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);

  int adjust = 0;
  String label = "AM";
  
  clearLine(2);
  String monthName = getMonthName(month);
  lcd.print(monthName);
  lcd.print(" ");
  lcd.print(dayOfMonth, DEC);
  lcd.print(", ");
  lcd.print(year+2000, DEC);

  clearLine(3);
  String dayName = getDayName(dayOfWeek);
  lcd.print(dayName);
  dayName == "Wednesday"?lcd.print(","):lcd.print(", ");
  //Serial.println(hour);
  if(hour > 12) {
    adjust = 12;
    label = "PM";
  } else if (hour == 0) {
    adjust = -12;
    label = "AM";
  } else if (hour == 12) {
    adjust = 0;
    label = "PM";
  } else {
    adjust = 0;
    label = "AM";
  }

  lcd.print(hour-adjust, DEC);
  // convert the byte variable to a decimal number when displayed
  lcd.print(":");
  if (minute<10)
  {
    lcd.print("0");
  }
  lcd.print(minute, DEC);
  lcd.print(":");
  if (second<10)
  {
    lcd.print("0");
  }
  lcd.print(second, DEC);
  //lcd.print(" ");
  lcd.print(label);

  clearLine(0);
  if(hour > 4 && hour < 12) {
    lcd.print("Good Morning, Amar!");
  } else if(hour >= 12 && hour < 17) {
    lcd.print("Good Afternoon,Amar!");
  } else if(hour >=17 && hour < 21) {
    lcd.print("Good Evening, Amar!");
  } else {
    lcd.print("Good Night, Amar!");
  }
}

void clearLine(int lineNum) {
  lcd.setCursor(0,lineNum);
  lcd.print("                    ");
  lcd.setCursor(0,lineNum);
}

String getMonthName(byte month) {
  switch(month) {
    case 1: 
      return "January";
    case 2: 
      return "February";
    case 3: 
      return "March";
    case 4: 
      return "April";
    case 5: 
      return "May";
    case 6: 
      return "June";
    case 7: 
      return "July";
    case 8: 
      return "August";
    case 9: 
      return "September";
    case 10: 
      return "October";
    case 11: 
      return "November";
    case 12: 
      return "December";
    default:
      return "Error";
  }
}

String getDayName(byte dayOfWeek) {
  switch(dayOfWeek){
  case 1:
    return "Sunday";
  case 2:
    return "Monday";
  case 3:
    return "Tuesday";
    break;
  case 4:
    return "Wednesday";
    break;
  case 5:
    return "Thursday";
    break;
  case 6:
    return "Friday";
    break;
  case 7:
    return "Saturday";
  default:
    return "Error";
  }
}

void changeTimeDate() {
  setBool = true;
}

void changeColors() {
  setColor = true;
}

void selectOptionRecieved() {
   //detachInterrupt(digitalPinToInterrupt(selectInterrupt)); 
   selectOption = true;
}

void changeRGBLights() {
  int color;
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));

  digitalWrite(LCDTogglePin, HIGH);
  
  lcd.clear();
  lcd.print("Change RGB Lights");
  
  //Get Color
  clearLine(1);
  lcd.print("Select Color:");
  clearLine(2);
  lcd.print("Off");
  color = 0;
  
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), selectOptionRecieved, RISING);
  delay(1000);
  selectOption = false;
  while(!selectOption) {
    if(digitalRead(selectUpPin) == HIGH) {
      while(digitalRead(selectUpPin) == HIGH);
      clearLine(2);
      color++;
      if(color >= 4) {
        color = 0;
      }   
      lcd.print(colorList[color]); 
      //Serial.println(color);   
    } else if(digitalRead(selectDownPin) == HIGH){
      while(digitalRead(selectDownPin) == HIGH);
      clearLine(2);
      if(color == 0) {
        color = 3;
      } else {
        color--;
      }
      lcd.print(colorList[color]); 
      //Serial.println(color);
    }
    
  }
  detachInterrupt(digitalPinToInterrupt(selectInterrupt));
  setColor = false;
  delay(1000);
  attachInterrupt(digitalPinToInterrupt(selectInterrupt), changeColors, RISING);
   
  changeRGBColor(color);

  lcd.clear();
  
}

void changeRGBColor(int c) {
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = colors[c];
  }
  FastLED.show();
}

