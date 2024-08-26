#include <WiFi.h>
#include <PubSubClient.h>
#include <Keypad.h>
#include "DHT.h"

// Ubidots parameters
const char *UBIDOTS_TOKEN = "";  // Ubidots token for authentication
const char *WIFI_SSID = "Wokwi-GUEST";    // WiFi SSID
const char *WIFI_PASS = "";     // WiFi password
const char *DEVICE_LABEL = "esp32dht22b";   // Device label in Ubidots
const char *VARIABLE_LABEL = "tempvalue";  // Variable label for temperature in Ubidots
const char *VARIABLE_LABEL2 = "humidityvalue";  // Variable label for humidity in Ubidots
const char *VARIABLE_LABEL3 = "buzzerstate";  // Variable label for buzzer state in Ubidots

// Update rate in milliseconds
const int PUBLISH_FREQUENCY = 5000;  // Interval for publishing data to Ubidots
unsigned long timer;  // Timer for tracking the publish interval

// DHT sensor configuration
#define DHTPIN 23     // Pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);  // Initialize DHT sensor

// Variables to store temperature range
int minTemp = 0;
int maxTemp = 0;
int minHumidity = 0;
int maxHumidity = 0;

// Variable to store pressed key
char key;  

// Variables to store temperature and humidity readings
float temperature;
float humidity;

// Pin for the buzzer
const int buzzerPin = 25;
int buzzerState = 0;  // Variable to store buzzer state

// Keypad setup
const int BARIS = 4;  // Number of rows in the keypad
const int KOLOM = 4;  // Number of columns in the keypad
char keys[BARIS][KOLOM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[BARIS] = {22, 21, 19, 18}; // Pins connected to the row outputs of the keypad
byte colPins[KOLOM] =  {5, 4, 2, 15}; // Pins connected to the column outputs of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, BARIS, KOLOM);  // Initialize keypad

// Wi-Fi and MQTT setup
WiFiClient wifiClient;  // Create a WiFi client
PubSubClient client(wifiClient);  // Create an MQTT client

void setup() {
  // Initialize buzzer pin as output
  pinMode(buzzerPin, OUTPUT);
  
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize DHT sensor
  dht.begin();
  
  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set MQTT server
  client.setServer("industrial.api.ubidots.com", 1883);
  client.setCallback(callback);
  
  // Connect to MQTT broker
  connectToMqtt();
}

void loop() {
  key = keypad.getKey();  // Get the key pressed on the keypad

  if (key != NO_KEY) {
    // Set initial temperature range based on the key pressed
    setTemperatureRange(key);

    // Monitor temperature and humidity within the specified range
    while (true) {
      temperature = dht.readTemperature();  // Read temperature from the DHT sensor
      humidity = dht.readHumidity(); // Read humidity from the DHT sensor
      
      // Check if the sensor readings are valid
      if (isnan(temperature) || isnan(humidity)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;  // Exit the loop if reading failed
      }
      
      // Print the min, max, and current temperature and humidity values
      printData(minTemp, maxTemp, minHumidity, maxHumidity, temperature, humidity);
      
      // Sound the buzzer if the temperature or humidity is outside the specified range
      if ((temperature < minTemp || temperature > maxTemp) && (humidity < minHumidity || humidity > maxHumidity)) {
        Serial.println(F("Temperature and Humidity out of range! Buzzing..."));
        tone(buzzerPin, 1000, 300); // Activate the buzzer
        buzzerState = 1;  // Buzzer is on
      } else if (temperature < minTemp || temperature > maxTemp) {
        Serial.println(F("Temperature out of range! Buzzing..."));
        tone(buzzerPin, 800, 100);  // Activate the buzzer
        buzzerState = 1;  // Buzzer is on
      } else if(humidity < minHumidity || humidity > maxHumidity) {
        Serial.println(F("Humidity out of range! Buzzing..."));
        tone(buzzerPin, 900, 200);  // Activate the buzzer
        buzzerState = 1;  // Buzzer is on
      } else {
        noTone(buzzerPin);  // Deactivate the buzzer if temperature and humidity are within range
        buzzerState = 0;  // Buzzer is off
      }
      
      // Send data to Ubidots
      if (!client.connected()) {
        connectToMqtt();
      }
      client.loop();

      // Publish data to Ubidots at the specified interval
      if (millis() - timer > PUBLISH_FREQUENCY) {
        publishToUbidots(temperature, humidity, buzzerState);
        timer = millis();
      }

      // Wait a few seconds between measurements
      delay(3000);
    }
  }
}

// Function to connect to MQTT broker
void connectToMqtt() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    if (client.connect("ESP32Client", UBIDOTS_TOKEN, "")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Function to publish data to Ubidots
void publishToUbidots(float temperature, float humidity, int buzzerState) {
  char payload[150];
  snprintf(payload, sizeof(payload), "{\"%s\": %.2f, \"%s\": %.2f, \"%s\": %d}", VARIABLE_LABEL, temperature, VARIABLE_LABEL2, humidity, VARIABLE_LABEL3, buzzerState);

  char topic[150];
  snprintf(topic, sizeof(topic), "/v1.6/devices/%s", DEVICE_LABEL);

  // Publish the payload to Ubidots
  if (client.publish(topic, payload)) {
    Serial.println("Publish successful");
    Serial.println("==============================");
  } else {
    Serial.println("Publish failed");
    Serial.println("==============================");
  }
}

// Function to set the temperature range based on the key pressed
void setTemperatureRange(char key) {
  switch (key) {
    case '1':
      minTemp = 0;
      maxTemp = 5;
      minHumidity = 10;
      maxHumidity = 20;
      break;
    case '2':
      minTemp = 5;
      maxTemp = 10;
      minHumidity = 20;
      maxHumidity = 30;
      break;
    case '3':
      minTemp = 10;
      maxTemp = 15;
      minHumidity = 30;
      maxHumidity = 40;
      break;
    default:
      minTemp = 0;
      maxTemp = 5;
      minHumidity = 40;
      maxHumidity = 50;
      break;
  }
}

// Function to print the temperature and humidity data
void printData(int minTemp, int maxTemp, int minHumidity, int maxHumidity, float currentTemp, float currentHumidity) {
  Serial.println("==============================");
  Serial.print(F("min temp: "));
  Serial.print(minTemp);
  Serial.println("°C ");
  Serial.print("max temp: ");
  Serial.print(maxTemp);
  Serial.println("°C ");
  Serial.print("current temp: ");
  Serial.print(currentTemp);
  Serial.println(F("°C "));
  Serial.print(("min humidity: "));
  Serial.print(minHumidity);
  Serial.println("°C ");
  Serial.print("max humidity: ");
  Serial.println(maxHumidity);
  Serial.print("current humidity: ");
  Serial.println(currentHumidity);
}

// Callback function for MQTT messages
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
