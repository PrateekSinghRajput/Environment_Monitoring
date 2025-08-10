// Prateek
// www.justdoelectronics.com

#include <Wire.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define rxPin 2
#define txPin 3
#define trigPin 11
#define echoPin 10
#define floatSensorPin 8  
#define buzzerPin 12  

SoftwareSerial sim800(rxPin, txPin);
String phoneNumbers[] = {"+918830584864", "+91xxxxxxxxxx", "+91xxxxxxxxxx"};
int totalNumbers = sizeof(phoneNumbers) / sizeof(phoneNumbers[0]);

int tankFullAlertSent = 0;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(floatSensorPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  Serial.begin(9600);
  sim800.begin(9600);
  Serial.println("SIM800L software serial initialized");

  sim800.println("AT");
  delay(1000);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To");
  lcd.setCursor(0, 1);
  lcd.print("  Our Projects");
  delay(2000);
  lcd.clear();
}

void loop() {

  long time_duration, distance_in_cm;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  time_duration = pulseIn(echoPin, HIGH);
  distance_in_cm = time_duration / 29 / 2;
  
  Serial.print(distance_in_cm);
  Serial.println(" cm");

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Water Dam");
  lcd.setCursor(0, 1);
  lcd.print("Level:");
  lcd.setCursor(6, 1);
  lcd.print(distance_in_cm);
  lcd.print("cm");

  // Check float sensor
  if (digitalRead(floatSensorPin) == HIGH) {
    lcd.setCursor(12, 1);
    lcd.print("FULL");
    digitalWrite(buzzerPin, HIGH); 

    if (tankFullAlertSent == 0) {
      String alertMessage = "EMERGENCY: Dam water level is FULL. Immediate Action Required!";
      sendMultipleSMS(alertMessage);
      tankFullAlertSent = 1;
    }
  } else {
    lcd.setCursor(12, 1);
    lcd.print("LOW ");
    digitalWrite(buzzerPin, LOW);
    tankFullAlertSent = 0; 
  }

  delay(1000);
}

// Function to send SMS to multiple numbers
void sendMultipleSMS(String message) {
  for (int i = 0; i < totalNumbers; i++) {
    sendSMS(phoneNumbers[i], message);
    delay(5000); 
  }
}

// Function to send SMS to a single number
void sendSMS(String number, String message) {
  Serial.print("Sending SMS to: ");
  Serial.println(number);

  sim800.println("AT+CMGF=1"); 
  delay(1000);
  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  delay(1000);
  sim800.println(message);
  delay(1000);
  sim800.write(26);  
  delay(5000);

  Serial.println("SMS sent.");
}
