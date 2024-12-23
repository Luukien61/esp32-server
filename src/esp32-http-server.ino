#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED width,  in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels

// create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
// Defining the WiFi channel speeds up the connection:
#define WIFI_CHANNEL 6

#define DHTPIN 14     // Chân kết nối cảm biến DHT22
#define DHTTYPE DHT22 // Loại cảm biến DHT

DHT dht(DHTPIN, DHTTYPE);

float temperature = 0.0;
float humidity = 0.0;

int temperatureThreshold= 30;
int humidityThreshold=40;

WebServer server(80);

const int LED1 = 26;
const int LED2 = 27;
const int LED3 = 25;


bool led1State = false;
bool led2State = false;
bool led3State = false;
bool isAutoTurnLed3 = true;

int x, minX;

String userMessage=" ";


void setMessage(String userMessage){


    oled.clearDisplay(); // clear display
    oled.setTextSize(2);         // set text size
    oled.setTextColor(WHITE);
    oled.setTextWrap(false);
    oled.setCursor(x, 10);       // set position to display (x,y)
    oled.println(userMessage); // set text
    oled.display();
    x=x-3;
    if(x < minX) x= oled.width();

}

void setup(void)
{
  Serial.begin(115200);
  dht.begin();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextWrap(false);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  

  server.on("/data", []()
   {
     StaticJsonDocument<200> doc;
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["button1"] = led1State;
        doc["button2"] = led2State;
        doc["button3"] = led3State;
        doc["isAutoMode"] = isAutoTurnLed3;
        doc["temperatureThreshold"] = temperatureThreshold;;
        doc["humidityThreshold"] = humidityThreshold;
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    }
    );

  server.on(UriBraces("/message/{}"), HTTP_POST, []()
  {
      String userId = server.pathArg(0);

      if (server.hasArg("plain")) {
          String body = server.arg("plain");

          // Phân tích cú pháp JSON
          StaticJsonDocument<200> doc;
          DeserializationError error = deserializeJson(doc, body);

          if (error) {
              Serial.print("deserializeJson() failed: ");
              Serial.println(error.c_str());
              server.send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
              return;
          }

          // Lấy các trường trong JSON
          String message = doc["message"].as<String>();
          Serial.print("Received message from user ");
          Serial.print(userId);
          Serial.print(": ");
          Serial.println(message);
          userMessage= message;

          x = oled.width();
          minX = -12 * strlen(userMessage.c_str());

          server.send(200, "application/json", "{\"status\": \"success\"}");

      } else {
          server.send(400, "application/json", "{\"error\": \"No body provided\"}");
      }
  });


  server.on(UriBraces("/toggle/{}"), []()
            {
    String led = server.pathArg(0);
    Serial.print("Toggle LED #");
    Serial.println(led);
    bool state=false;
    switch (led.toInt()) {
      case 1:
        led1State = !led1State;
        digitalWrite(LED1, led1State);
        state=led1State;
        break;
      case 2:
        led2State = !led2State;
        digitalWrite(LED2, led2State);
        state=led2State;
        break;
    }
    server.send(200, "text/plain", state ? "true" : "false");
    });

  server.on("/auto", []()
    {
        isAutoTurnLed3=!isAutoTurnLed3;
        server.send(200, "text/plain", isAutoTurnLed3 ? "true" : "false");
    }
  );

  server.on("/led3", [](){
   if(isAutoTurnLed3==false){
        led3State=!led3State;
        digitalWrite(LED3, led3State);
    }
    server.send(200, "text/plain", led3State ? "true" : "false");
  });

  server.on(UriBraces("/threshold"), HTTP_POST, []()
    {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");

            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                server.send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
                return;
            }

            int userTemperatureThreshold = doc["temperature"].as<int>();
            int userHumidityThreshold = doc["humidity"].as<int>();
            temperatureThreshold=userTemperatureThreshold;
            humidityThreshold=userHumidityThreshold;
            checkTemperatureAndHumidity();

            sendSuccessResponse();

        } else {
            server.send(400, "application/json", "{\"error\": \"No body provided\"}");
        }
    });

  server.begin();
  Serial.println("HTTP server started");
}

void sendSuccessResponse(void){
    server.send(200, "application/json", "{\"status\": \"success\"}");
}

void checkTemperatureAndHumidity(void){
    if(isAutoTurnLed3==true){
        if(temperature>temperatureThreshold || humidity < humidityThreshold){
                led3State=true;
                digitalWrite(LED3, HIGH);
        }
        else{
            led3State=false;
            digitalWrite(LED3,LOW);
        }
    }

}

void loop(void)
{
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  checkTemperatureAndHumidity();
  server.handleClient();
  setMessage(userMessage);
  delay(2);
}
