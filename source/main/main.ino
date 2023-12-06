#include "credentials.h" // Include the secrets file for WiFi and AWS credentials
#include <WiFiClientSecure.h> // Secure WiFi client for HTTPS connections
#include <MQTTClient.h> // MQTT client for AWS IoT
#include <ArduinoJson.h> // JSON library for data serialization
#include "WiFi.h" // WiFi library for ESP32
String device_id;

// HC-SR04 Pin definitions
#define triggerPin 5 // GPIO pin for the HC-SR04 trigger
#define echoPin 4    // GPIO pin for the HC-SR04 echo

// MQTT topics for the device
#define AWS_IOT_PUBLISH_TOPIC (String("sensors/") + device_id + "/pub").c_str()
#define AWS_IOT_SUBSCRIBE_TOPIC (String("sensors/") + device_id + "/sub").c_str()
// Define new topics for distance and liters
#define AWS_IOT_PUBLISH_TOPIC_DISTANCE (String("sensors/") + device_id + "/distance/pub").c_str()
#define AWS_IOT_PUBLISH_TOPIC_LITERS (String("sensors/") + device_id + "/liters/pub").c_str()

// Shadow topics for AWS IoT
#define SHADOW_UPDATE_DELTA_TOPIC (String("$aws/things/") + device_id + "/shadow/update/delta").c_str()
#define SHADOW_UPDATE_ACCEPTED_TOPIC (String("$aws/things/") + device_id + "/shadow/update/accepted").c_str()

// Define the shape sizes in centimeters for volume calculation
int cuboid_width = 50;
int cuboid_length = 40;
int cuboid_height = 300;

// Global variables for distance and volume measurement
float distance, liters;

// Function to measure distance using HC-SR04
float measureDistance() {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.034 / 2; // Calculate distance in cm
  return distanceCm;
}

// Function for calculating volume of a cuboid
float calculateCuboidVolume(float distance) {
  float height = cuboid_height - distance;
  float volume_cm3 = height * cuboid_length * cuboid_width;
  return volume_cm3 / 1000.0; // Convert from cubic cm to liters
}

WiFiClientSecure wifi_client; // Secure WiFi client instance
MQTTClient mqtt_client(256);  // MQTT client instance with 256 byte packet size

// Function to connect to AWS IoT
void connectAWS() {
  Serial.println("Setting WiFi to station mode");
  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Start WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.println("Setting up secure connection parameters");
  wifi_client.setCACert(AWS_CERT_CA);
  wifi_client.setCertificate(AWS_CERT_CRT);
  wifi_client.setPrivateKey(AWS_CERT_PRIVATE);
  Serial.print("Connecting to AWS IoT Broker at: ");
  Serial.println(AWS_IOT_ENDPOINT);
  mqtt_client.begin(AWS_IOT_ENDPOINT, 8883, wifi_client);
  Serial.print("Attempting MQTT connection...");
  while (!mqtt_client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nMQTT Connected");
  Serial.print("Subscribing to topic: ");
  Serial.println(AWS_IOT_SUBSCRIBE_TOPIC);
  mqtt_client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  mqtt_client.subscribe(SHADOW_UPDATE_DELTA_TOPIC);
  mqtt_client.subscribe(SHADOW_UPDATE_ACCEPTED_TOPIC);
  Serial.println("Subscribed to AWS IoT");
}

// Function to reconnect to MQTT
void reconnect() {
  unsigned long lastAttemptTime = 0;
  const long retryInterval = 5000; // Retry every 5 seconds
  int attemptCount = 0;
  const int maxAttempt = 10; // Maximum number of attempts

  while (!mqtt_client.connected() && attemptCount < maxAttempt) {
    if (millis() - lastAttemptTime > retryInterval) {
      lastAttemptTime = millis();
      attemptCount++;

      if (WiFi.status() != WL_CONNECTED) {
        // Reconnect WiFi if needed
        Serial.println("Reconnecting to WiFi...");
        WiFi.reconnect();
      }

      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (mqtt_client.connect(THINGNAME)) {
        Serial.println("connected");
        // Resubscribe to the topic after successful connection
        mqtt_client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
      } else {
        Serial.print("failed, rc=");
        // Replace with a method that gives actual error code if available
        Serial.print(mqtt_client.connected()); 
        Serial.println(" try again in 5 seconds");
      }
    }
    delay(10); // Small delay to prevent a totally 'tight' loop
  }
}

void incomingMessageHandler(String &topic, String &payload) {
  StaticJsonDocument<300> doc;
  deserializeJson(doc, payload);

  if (doc.containsKey("state") && doc["state"].containsKey("desired")) {
    JsonObject desired = doc["state"]["desired"];

    bool shouldRecalculate = false;

    if (desired.containsKey("cuboid_width")) {
      cuboid_width = desired["cuboid_width"].as<int>();
      shouldRecalculate = true;
    }
    if (desired.containsKey("cuboid_length")) {
      cuboid_length = desired["cuboid_length"].as<int>();
      shouldRecalculate = true;
    }
    if (desired.containsKey("cuboid_height")) {
      cuboid_height = desired["cuboid_height"].as<int>();
      shouldRecalculate = true;
    }

    if (shouldRecalculate) {
      // Calculate liters based on updated dimensions
      liters = calculateCuboidVolume(distance);
      // Publish both distance and liters data with updated values
      publishToTimestream(distance, "distance");
      publishToTimestream(liters, "liters");
    }
  }
}

void publishToTimestream(float value, const char* measureName) {
    // Create a JSON document
    StaticJsonDocument<256> jsonDoc;

    // Get the current timestamp (you may need to adjust this based on your time source)
    unsigned long currentTimestamp = millis();

    // Add data to the JSON document
    jsonDoc["device_id"] = THINGNAME;
    jsonDoc["measure_name"] = measureName;
    jsonDoc["time"] = currentTimestamp; // Include the timestamp
    jsonDoc["measure_value::double"] = static_cast<double>(value); 

    // Serialize JSON document to String
    String payload;
    serializeJson(jsonDoc, payload);

    // Publish data to a specific topic based on measureName
    if (strcmp(measureName, "distance") == 0) {
      mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC_DISTANCE, payload.c_str());
      Serial.println("Distance data sent to custom topic for Timestream");
    } else if (strcmp(measureName, "liters") == 0) {
      mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC_LITERS, payload.c_str());
      Serial.println("Liters data sent to custom topic for Timestream");
    }
}

// Setup function - runs once at startup
void setup() {
  Serial.begin(9600);
  Serial.println("Starting device...");
  device_id = WiFi.macAddress(); // Get MAC address
  device_id.replace(":", ""); // Remove colons from the MAC address
  Serial.println(device_id);
  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);
  connectAWS();
}

// Main loop function - runs repeatedly
void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  Serial.println("Measuring distance...");
  distance = measureDistance();

  // Calculate and publish distance data
  publishToTimestream(distance, "distance");
  
  if (distance >= 2 && distance <= 400) {
    // Calculate and publish liters data
    liters = calculateCuboidVolume(distance);
    publishToTimestream(liters, "liters");
    Serial.println("Distance and Liters data sent to custom topics for Timestream");
  } else {
    Serial.println("Distance out of range");
  }

  mqtt_client.loop();
  delay(10000); // Delay for 10 seconds before repeating the loop
}
