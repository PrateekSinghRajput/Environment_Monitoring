#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>


#define TRIG_PIN 18
#define ECHO_PIN 5
#define BUZZER_PIN 12
#define RX_PIN 16
#define TX_PIN 17


const char* ssid = "Prateek";
const char* password = "justdoelectronics@#12345";


const int MAX_HEIGHT = 30;
const char* phoneNumber = "+91xxxxxxxxxx";

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial gsmSerial(0);
LiquidCrystal_PCF8574 lcd(0x27);
AsyncWebServer server(80);

long currentDistance = 0;
unsigned long lastAlertTime = 0;
const unsigned long alertCooldown = 60000;

void setup() {
  Serial.begin(115200);
  gsmSerial.begin(9600);
  gpsSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.print("System Init...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println();
  Serial.println("WiFi Connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connected to WiFi");
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(2000);
  lcd.clear();
  lcd.print(WiFi.localIP());
  delay(4000);
  lcd.clear();

  gsmSerial.println("AT+CNMI=2,1,0,0,0");
  delay(500);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <title>Smart Garbage Monitoring</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; }
          #dustbin {
            width: 24cm;
            height: 20cm;
            border: 5px solid black;
            border-radius: 10px;
            margin: 20px auto;
            position: relative;
            background-color: #f3f3f3;
          }
          #garbage {
            position: absolute;
            bottom: 0;
            left: 0;
            width: 100%;
            transition: height 1s ease;
            background-color: green;
          }
        </style>
      </head>
      <body>
        <h1>Smart Garbage Monitoring System</h1>
        <div id="dustbin">
          <div id="garbage" style="height: 0%;"></div>
        </div>
        <p>Garbage Level: <span id="distance">0</span> cm</p>
        <p>GPS Coordinates:</p>
        <p>Latitude: <span id="latitude">N/A</span></p>
        <p>Longitude: <span id="longitude">N/A</span></p>
        <script>
          function updateData() {
            fetch('/data')
            .then(response => response.json())
            .then(data => {
              const distance = data.distance;
              const latitude = data.latitude;
              const longitude = data.longitude;

              const maxDistance = 20;
              let level = 100 - (distance / maxDistance) * 100;
              level = Math.max(0, Math.min(100, level));

              let color = 'green';
              if (level >= 70) color = 'red';
              else if (level >= 30) color = 'yellow';

              const garbageDiv = document.getElementById('garbage');
              garbageDiv.style.height = level + '%';
              garbageDiv.style.backgroundColor = color;

              document.getElementById('distance').textContent = distance;
              document.getElementById('latitude').textContent = latitude || 'N/A';
              document.getElementById('longitude').textContent = longitude || 'N/A';
            })
            .catch(err => console.error(err));
          }
          setInterval(updateData, 5000);
          updateData();
        </script>
      </body>
      </html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{";
    json += "\"distance\": " + String(currentDistance) + ",";
    if (gps.location.isValid()) {
      json += "\"latitude\":\"" + String(gps.location.lat(), 6) + "\",";
      json += "\"longitude\":\"" + String(gps.location.lng(), 6) + "\"";
    } else {
      json += "\"latitude\":\"19.585098\",";
      json += "\"longitude\":\"78.737760\"";
    }
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("System Initialized");
  delay(2000);
  lcd.clear();
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return MAX_HEIGHT;
  long distance = (duration * 0.0343) / 2;
  if (distance > MAX_HEIGHT) distance = MAX_HEIGHT;
  return distance;
}

void sendSMS(const char* message) {
  while (gsmSerial.available()) gsmSerial.read();

  gsmSerial.println("AT");
  delay(500);
  gsmSerial.println("AT+CMGF=1");
  delay(500);

  unsigned long startTime = millis();
  bool registered = false;
  while (millis() - startTime < 10000) {
    gsmSerial.println("AT+CREG?");
    delay(500);
    while (gsmSerial.available()) {
      String resp = gsmSerial.readString();
      if (resp.indexOf("+CREG: 0,1") >= 0 || resp.indexOf("+CREG: 0,5") >= 0) {
        registered = true;
        break;
      }
    }
    if (registered) break;
  }
  if (!registered) {
    Serial.println("GSM network not registered.");
    return;
  }

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);

  gsmSerial.print(message);
  delay(500);

  gsmSerial.write(26);
  delay(5000);

  Serial.println("SMS sent or attempted.");
}


void checkForIncomingSMS() {
  if (!gsmSerial.available()) return;

  String gsmBuffer = gsmSerial.readString();

  if (gsmBuffer.indexOf("+CMTI:") != -1) {
    int startIndex = gsmBuffer.indexOf(",") + 1;
    String smsIndexStr = gsmBuffer.substring(startIndex);
    smsIndexStr.trim();
    int smsIndex = smsIndexStr.toInt();

    if (smsIndex > 0) {
      // Read the SMS content
      gsmSerial.println("AT+CMGR=" + String(smsIndex));
      delay(1000);

      String smsContent = "";
      while (gsmSerial.available()) {
        smsContent += gsmSerial.readString();
        delay(10);
      }


      int numStart = smsContent.indexOf("\"+");
      int numEnd = smsContent.indexOf("\"", numStart + 1);
      String senderNumber = "";
      if (numStart != -1 && numEnd != -1) {
        senderNumber = smsContent.substring(numStart + 1, numEnd);
      }

      int msgStart = smsContent.indexOf("\n", numEnd) + 1;
      int msgEnd = smsContent.indexOf("\n", msgStart);
      String message = "";
      if (msgStart != -1 && msgEnd != -1) {
        message = smsContent.substring(msgStart, msgEnd);
      }
      message.trim();

      Serial.print("Received SMS From: ");
      Serial.println(senderNumber);
      Serial.print("Message: ");
      Serial.println(message);


      if (message.equalsIgnoreCase("STATUS")) {
        String reply = "Garbage Status:\n";
        reply += "Distance: " + String(currentDistance) + " cm\nLocation: ";

        if (gps.location.isValid()) {
          reply += "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
        } else {
          reply += "https://maps.google.com/?q=18.585098,73.737760";
        }


        sendSMSTo(reply.c_str(), senderNumber.c_str());
      }


      gsmSerial.println("AT+CMGD=" + String(smsIndex));
      delay(1000);
    }
  }
}


void sendSMSTo(const char* message, const char* number) {
  while (gsmSerial.available()) gsmSerial.read();

  gsmSerial.println("AT");
  delay(500);
  gsmSerial.println("AT+CMGF=1");
  delay(500);

  unsigned long startTime = millis();
  bool registered = false;
  while (millis() - startTime < 10000) {
    gsmSerial.println("AT+CREG?");
    delay(500);
    while (gsmSerial.available()) {
      String resp = gsmSerial.readString();
      if (resp.indexOf("+CREG: 0,1") >= 0 || resp.indexOf("+CREG: 0,5") >= 0) {
        registered = true;
        break;
      }
    }
    if (registered) break;
  }
  if (!registered) {
    Serial.println("GSM network not registered.");
    return;
  }

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(number);
  gsmSerial.println("\"");
  delay(1000);

  gsmSerial.print(message);
  delay(500);

  gsmSerial.write(26);  // Ctrl+Z
  delay(5000);

  Serial.println("SMS replied or attempted.");
}

void loop() {
  currentDistance = measureDistance();

  Serial.print("Distance: ");
  Serial.print(currentDistance);
  Serial.println(" cm");

  lcd.setCursor(0, 0);
  lcd.print("Dist: ");
  lcd.print(currentDistance);
  lcd.print(" cm   ");

  unsigned long now = millis();

  if (currentDistance < 8 && (now - lastAlertTime > alertCooldown)) {
    String alertMsg = "Garbage level high! Distance: " + String(currentDistance) + " cm.\nLocation: ";
    if (gps.location.isValid()) {
      alertMsg += "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    } else {
      alertMsg += "https://maps.google.com/?q=18.585098,73.737760";
    }
    digitalWrite(BUZZER_PIN, HIGH);
    lcd.setCursor(0, 1);
    lcd.print("Alert! Garbage  ");

    sendSMS(alertMsg.c_str());

    lastAlertTime = now;
  }

  if (currentDistance >= 8) {
    digitalWrite(BUZZER_PIN, LOW);
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }

  checkForIncomingSMS();

  delay(3000);
}