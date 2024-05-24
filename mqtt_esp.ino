//WiFi library for the esp32
#include <ESP8266WiFi.h>
//Library used as client for all mqtt services
#include <PubSubClient.h>

//**** WiFi credentials *****
//ssid
const char *ssid = "cancun";
//password
const char *password = "ozanozan";

// MQTT server information. Insert the ip address of your server
const char *mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char *mqtt_username = "ozan";  // MQTT username for authentication
const char *mqtt_password = "123456";
const char *subscribe_topic = "temperature/subscribe";
const char *publish_topic = "temperature/publish";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

unsigned long duration = 1000; 
unsigned long startTime = 0; 
unsigned long lastSampleTime = 0; 
bool collectingData = false; 
float totalSensorValue = 0;
int numReadings = 0; 
int sampleInterval = 100;

void connectToWiFi();

void connectToMQTTBroker();

void mqttCallback(char *topic, byte *payload, unsigned int length);


void setup() {
  Serial.begin(115200);
  Serial.println("Setup started.");
  setup_wifi();
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  duration = message.toInt() * 1000; // Convert duration to milliseconds
  if (duration > 0) {
    startTime = millis();
    lastSampleTime = millis();
    collectingData = true;
    totalSensorValue = 0;
    numReadings = 0;
    Serial.print("New interval: ");
    Serial.print(duration / 1000);
    Serial.println(" seconds");
  }
}

void connectToMQTTBroker() {
    while (!mqtt_client.connected()) {
        String client_id = "esp8266-client-" + String(WiFi.macAddress());
        Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
        if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            mqtt_client.subscribe(subscribe_topic);
            // Publish message upon successful connection
            mqtt_client.publish(publish_topic, "Hi EMQX I'm ESP8266 ^^");
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

float readLM35() {
  int analogValue = analogRead(A0);
  float millivolts = (analogValue / 1024.0) * 3300; // 3.3V reference
  float temperatureC = millivolts / 10; // 10mV per degree Celsius
  return temperatureC;
}

void loop() {
  if (!mqtt_client.connected()) {
      connectToMQTTBroker();
  }
  mqtt_client.loop();
  unsigned long currentMillis = millis();

  if (collectingData) {
    if (currentMillis - lastSampleTime >= sampleInterval) { // Sample the sensor every second
      float temperature = readLM35();
      totalSensorValue += temperature;
      numReadings++;
      lastSampleTime = currentMillis; // Update last sample time
    }

    if (currentMillis - startTime >= duration) {
      // Publish the mean temperature and reset for the next interval
      if (numReadings > 0) {
        float meanValue = totalSensorValue / numReadings;
        char meanString[8];
        dtostrf(meanValue, 1, 2, meanString);
        mqtt_client.publish(publish_topic, meanString);
        Serial.print("Published mean temperature: ");
        Serial.println(meanString);
      }

      // Reset for the next interval
      startTime = currentMillis;
      lastSampleTime = currentMillis;
      totalSensorValue = 0;
      numReadings = 0;
    }
  }
}