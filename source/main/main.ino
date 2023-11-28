
  #include "credentials.h" // Include the secrets file for WiFi and AWS credentials
  #include <WiFiClientSecure.h> // Secure WiFi client for HTTPS connections
  #include <MQTTClient.h> // MQTT client for AWS IoT
  #include <ArduinoJson.h> // JSON library for data serialization
  #include "WiFi.h" // WiFi library for ESP32
  String device_id;
  
  // HC-SR04 Pin definitions
  #define triggerPin 5 // GPIO pin for the HC-SR04 trigger
  #define echoPin 4    // GPIO pin for the HC-SR04 echo
  // Define the shape sizes in centimeters for volume calculation
  #define cuboid_width 50
  #define cuboid_length 40
  #define cuboid_height 300
  #define cylinder_radius 45
  #define cylinder_height 100
  int calculationMode = 0; // 0 for cuboid, 1 for cylinder
  String calculationType; // Add this line
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
    float distanceCm = duration * 0.034 / 2; // Calculate distance (in cm)
    return distanceCm;
  }
  // Function for calculating volume of a cuboid
  float calculateCuboidVolume(float distance) {
    float height = cuboid_height - distance;
    float volume_cm3 = height * cuboid_length * cuboid_width;
    return volume_cm3 / 1000.0; // Convert fromcubic cm to liters
  }
  // Function for calculating volume of a cylinder
  float calculateCylinderVolume(float distance) {
    float height = cylinder_height - distance;
    float volume_cm3 = M_PI * pow(cylinder_radius, 2) * height;
    return volume_cm3 / 1000.0; // Convert from cubic cm to liters
  }
  // MQTT topics for the device
  #define AWS_IOT_PUBLISH_TOPIC (device_id + "/pub").c_str()
  #define AWS_IOT_SUBSCRIBE_TOPIC (device_id + "/sub").c_str()
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

  // Function to publish messages to AWS IoT
  void publishMessage() {
    StaticJsonDocument<200> doc;
    doc["device_id"] = device_id; 
    doc["distance"] = distance;
    doc["liters"] = liters;
    doc["containerType"] = calculationType; 
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
    Serial.println("Message sent to AWS IoT");
  }
  // Handler for incoming MQTT messages
  void incomingMessageHandler(String &topic, String &payload) {
    Serial.println("Message received:");
    Serial.println("Topic: " + topic);
    Serial.println("Payload: " + payload);
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    if (doc.containsKey("mode")) {
      calculationMode = doc["mode"].as<int>();
      Serial.print("Calculation mode changed to: ");
      Serial.println(calculationMode == 0 ? "Cuboid" : "Cylinder");
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
    
    if (calculationMode == 0) {
      liters = calculateCuboidVolume(distance);
      calculationType = "cuboid";
    } else if (calculationMode == 1) {
      liters = calculateCylinderVolume(distance);
      calculationType = "cylinder";
    }
  
    if (distance >= 2 && distance <= 400) { // Validate distance range
      Serial.println("Publishing data...");
      publishMessage();
    } else {
      Serial.println("Distance out of range");
    }
    mqtt_client.loop();
    delay(10000); 
  }
