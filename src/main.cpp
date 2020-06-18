#include <ESP8266WiFi.h>
#include <Wire.h>
#include "PubSubClient.h"
#include "Adafruit_BME280.h"
#include "wifi_cred.h"
#define ROOM "bedroom/roy/desk"

const char *mqtt_id   = "Living";

const char RELAY_PIN = 2;
const char SWITCH_PIN = 14;
const char SDA_PIN = 12;
const char SLC_PIN = 13;
static volatile bool isOn;
static unsigned long switchLastTimePressed = 0;
static unsigned long bmeLastTimePolled = 0;
static bool bmeFound = false;

static char stringbuffer[64];


IPAddress mqtt_broker_host(192, 168, 1, 8);
Adafruit_BME280 bme280;

//forward declaration of functions
void update_state(void);
void bme_task(void);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Yay: \n");
  if(length != 1) {
    Serial.println("Unexpected payload!");
    return;
  }
  
  if(*payload == '1' || *payload == '0') {
    isOn = *payload == '1';
    update_state();
  } else {
    Serial.println("Unexpected payload!");
  }

  //handle message arrived
}

void switchHandler(void) {
  unsigned long currentTime = millis();
  unsigned long spentTime = currentTime - switchLastTimePressed;

  if(spentTime > 1000) {
    switchLastTimePressed = currentTime;
    isOn = !isOn;
    update_state();
  }
}

WiFiClient espClient;
PubSubClient client(mqtt_broker_host, 1883, callback, espClient);

void connect(void) {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  client.setCallback(callback);
  if( client.connect(mqtt_id, mqtt_user, mqtt_pass) ) {
    Serial.println("Connected, subscribing...");
    client.subscribe("lights/" ROOM "/in");
  }
}
 
void setup() {
  isOn = false;
  
  Serial.begin(115200);
  delay(100);
  connect();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SLC_PIN);
  bmeFound = bme280.begin(0x76);
  if (!bmeFound) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchHandler, FALLING);

}
 
void loop() {
  client.loop();

  unsigned long currentTime = millis();
  if(bmeFound) {
    if(currentTime - bmeLastTimePolled > 5000) {
      bme_task();
      bmeLastTimePolled = millis();
    }
  }
}

void update_state(void) {
  digitalWrite(RELAY_PIN, isOn ? HIGH : LOW);
  client.publish("lights/" ROOM "/out", isOn ? "1" : "0");
}

void bme_task(void) {
  float temperature = bme280.readTemperature();
  float humidity = bme280.readHumidity();
  float pressure = bme280.readPressure() / 100;
  
  sprintf(stringbuffer, "%f", temperature);
  Serial.println(stringbuffer);
  if(!client.publish("temperature/" ROOM, stringbuffer)) {
    connect();
  }

  sprintf(stringbuffer, "%f", humidity);
  client.publish("humidity/" ROOM, stringbuffer);

  sprintf(stringbuffer, "%f", pressure);
  client.publish("pressure/" ROOM, stringbuffer);
}