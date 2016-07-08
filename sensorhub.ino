#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#define DHTTYPE DHT22
#define DHTPIN D6

DHT dht(DHTPIN, DHTTYPE);
float humidity, temp_c, temp_f;          // Values read from DHT22
unsigned long previousMillis = 0;        // will store last temp was read
const long interval = 5000;              // interval at which to read sensor

//OTA Config
const char* deviceName = "ESP8266-Hall-Sensors";
const char* OTAPass = "update";

//Pins for sensors
const int pir = D7;
const int door = D8;
const int light = A0;

//Temp
char* tempTopic = "/sensorbox/one/temp";
char* currentTemp;
String prevTemp;

//Humidity
char* humTopic = "/sensorbox/one/humidity";
char* currentHum;
String prevHum;

//Motion
char* pirTopic = "/sensorbox/one/pir";
String currentPir;
String prevPir;

//Light
char* lightTopic = "/sensorbox/one/light";
int lightReading;
float currentLight;
char* currentLightState;
String prevLightState;

//Door
char* doorTopic = "/sensorbox/one/door";
String currentDoor;
String prevDoor;

// Wifi + Server Configuration
const char* ssid = "*****";
const char* password = "*****";
const char* mqtt_server = "*****";

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  
}

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

//networking functions
void setup_wifi() {

  delay(10);
  //Connect to Wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Sensor-Hub-Hallway")) {
      Serial.println("connected");
      // Once connected, publish some sensor values because Home-Assistant forgot
      client.publish(doorTopic, "closed", true);
      client.publish(pirTopic, "off", true);
      client.publish(lightTopic, currentLightState, true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  pinMode(light, INPUT);    
  pinMode(pir, INPUT);
  pinMode(door, INPUT);

  //start the serial line for debugging
  Serial.begin(115200);
  dht.begin(); 
  delay(100);

  //OTA Setup
  WiFi.mode(WIFI_STA);
  ArduinoOTA.setPassword(OTAPass);
  ArduinoOTA.setHostname(deviceName);
  
  Serial.println("");
  Serial.println("Sensor Box Initiating");

  //Connect to wifi and MQTT server
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void gettemperature() {
  // Wait at least 2 seconds seconds between measurements.
  unsigned long currentMillis = millis();
  
    if(currentMillis - previousMillis >= interval) {
      // save the last time you read the sensor 
      previousMillis = currentMillis;   
 
      // Reading temperature for humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
      humidity = dht.readHumidity();          // Read humidity (percent)
      temp_f = dht.readTemperature(true);     // Read temperature as Fahrenheit
      temp_c = (((temp_f-32.0)*5/9)-1);       // Convert temperature to Celsius + Correction
      // Check if any reads failed and exit early (to try again).
      if (isnan(humidity) || isnan(temp_c)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
      }      
    }
}

void sendTempHum() {
    // Define 
  String str1 = String(temp_c); 
  // Length (with one extra character for the null terminator)
  int str1_len = str1.length() + 1; 
  // Prepare the character array (the buffer) 
  char char_array_temp[str1_len];
  // Copy it over 
  str1.toCharArray(char_array_temp, str1_len);
  
  String str2;
  if(humidity <= 100){
     str2 = String(int(humidity)); 
  }else{
    str2 = "nan"; 
  }
  int str2_len = str2.length() + 1; 
  char char_array_hum[str2_len];
  str2.toCharArray(char_array_hum, str2_len);
 
  //publish the new temperature
  if(String((float)temp_c) != prevTemp && String((float)temp_c) != "nan") {
    client.publish(tempTopic, char_array_temp, true);
    //Serial.println(String((float)temp_c)+" Degrees");
  }
  if(String((int)humidity) != prevHum && humidity < 100) {
    client.publish(humTopic, char_array_hum, true);
    //Serial.println(String((int)humidity)+ "% Humidity");
  }
  prevTemp = String((float)temp_c);
  prevHum = String((int)humidity);
}

void sendMotion() {
  currentPir = String(digitalRead(pir));

  if(currentPir != prevPir) {
    if(currentPir == "1") {
      client.publish(pirTopic, "on", true);
      //Serial.println("Movement detected"); 
    } else {
      client.publish(pirTopic, "off", true);
      //Serial.println("Movement NOT detected"); 
    }  
  }
  prevPir = currentPir;
}

void sendDoor() {
  currentDoor = String(digitalRead(door));

  if(currentDoor != prevDoor) {
    if(currentDoor == "1") {
      client.publish(doorTopic, "closed", true);
      //Serial.println("Door is closed"); 
    } else {
      client.publish(doorTopic, "open", true);
      //Serial.println("Door is open"); 
    }
  }
  prevDoor = currentDoor;
}

void sendLight() {
  lightReading = analogRead(light);
  currentLight = (lightReading * 3.3 / 1023.0);

  if(currentLight > 2.52 && prevLightState != "bright") {
    //Serial.println("bright");
    currentLightState = "bright";
    client.publish(lightTopic, currentLightState, true);
  } else if (currentLight > 1.72 && currentLight <= 2.50 && prevLightState != "light") {
    //Serial.println("light");
    currentLightState = "light";
    client.publish(lightTopic, currentLightState, true);
  } else if (currentLight > 0.92 && currentLight <= 1.70 && prevLightState != "dim") {
    //Serial.println("dim");
    currentLightState = "dim";
    client.publish(lightTopic, currentLightState, true);
  } else if (currentLight <= 0.90 && prevLightState != "dark") {
    //Serial.println("dark");
    currentLightState = "dark";
    client.publish(lightTopic, currentLightState, true);
  }
  
  prevLightState = currentLightState;
}
void loop(){

  //Run sensors
  gettemperature(); //get data from DHT22
  sendTempHum(); //send data from DHT22
  sendMotion();
  sendDoor();
  sendLight();

  //maintain MQTT connection
  client.loop();

  //OTA handler
  ArduinoOTA.handle();
  
  if (!client.connected()) {
    reconnect();
  }
}
