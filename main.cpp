#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Bounce2.h>
#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <Adafruit_SSD1306.h>
#include <TroykaMQ.h>
#include <ESP8266HTTPClient.h>

// REPLACE THESE VALUES
#define WIFI_SSID "***"
#define WIFI_PASS "***"
#define MQTT_SERVER "***"
#define MQTT_USER "***"
#define MQTT_PASSWORD "***"
#define MQTT_TOPIC "messages"
#define MQTT_ACK_TOPIC "streams" // subscribe to an acknowledgement topic
#define IP_STACK_ACCESS_KEY "***" // https://ipstack.com access key for geolocation
//////////////////////

#define BUTTON_PIN 0
#define PIN_MQ135  A0
#define LED 2
#define DHT_DATA_PIN 12
#define OLED_RESET 13

WiFiClient espClient;
PubSubClient mqtt(espClient);
Bounce button = Bounce();
DHT_Unified dht(DHT_DATA_PIN, DHT22);
Adafruit_SSD1306 display(OLED_RESET);
MQ135 mq135(PIN_MQ135);
StaticJsonBuffer<400> staticBuffer;

typedef struct {
  String ip;
  String ln;
  String lt;
} GeoData;
GeoData location;

char chipId[8];
char message[401];
bool onFlag = false;

void getGeo();
String getIP();
void printSerial(String label, float val, String unit);
void printDisplay(int row, String label, float val, String unit);
void callback(char* topic, byte* payload, unsigned int length);

// get data and publish to mqtt topic
void report(){
  display.clearDisplay();
  sensors_event_t event;

  // read temp + humidity
  dht.temperature().getEvent(&event);
  float celsius = event.temperature;
  float fahrenheit = (celsius * 1.8) + 32;
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;
  printSerial("celcius: ",celsius,"C");
  printSerial("fahrenheit: ",fahrenheit,"F");
  printSerial("humidity: ",humidity,"%");
  printDisplay(0,"Fahrenheit: ",fahrenheit,"");
  printDisplay(1,"Humidity: ",humidity,"%");

  // PPM of CO2 in the air
  float ppm = mq135.readCO2();
  printSerial("CO2: ",ppm,"ppm");
  printDisplay(2,"CO2: ",ppm," ppm");

  // update the OLED screen
  display.display();

  // get coordinates
  getGeo();
  float lat = location.lt.toFloat();
  float lng = location.ln.toFloat();
  // skip if missing location or CO2 data
  if(lat==0.0 || lng==0.0 || ppm==0.0 || ppm>1000000.0){
    Serial.printf("ERROR: %f,%f,%f",lat,lng,ppm);
    return;
  }
  JsonObject& obj = staticBuffer.createObject();
  obj["temperature"] = fahrenheit;
  obj["humidity"] = humidity;
  obj["lat"] = lat;
  obj["lng"] = lng;
  obj["ppm"] = ppm;
  obj.printTo(message);

  // publish to MQTT
  char topic[40];
  sprintf(topic, "%s/%s", MQTT_TOPIC, chipId);
  mqtt.publish(topic, message);
  char print[100];
  sprintf(print, "[%s] %s\n", topic, message);
  Serial.println(print);
}

/////SETUP_WIFI/////
void setup_wifi() {
  onFlag = false;
  delay(10);

  // set chip Id
  sprintf(chipId, "%08X", ESP.getChipId());
  Serial.print("CHIP ID:");
  Serial.println(chipId);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address

  // init dht22
  dht.begin();

  // init OLED screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
}

void setup() {

  mq135.calibrate();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.attach(BUTTON_PIN);
  button.interval(5);

  // LED
  pinMode(LED, OUTPUT);

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  setup_wifi();

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(callback); //register the callback function
}

void reconnect() {
  // Loop until we're reconnected to MQTT
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(chipId, MQTT_USER, MQTT_PASSWORD)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      onFlag = true; // turn on LED
      char topic[40];
      sprintf(topic, "%s/%s", MQTT_ACK_TOPIC, "#");
      mqtt.subscribe(topic); // subscribe to the acknowledgment topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  button.update();
  if(button.fell()){
    report();
  }

  digitalWrite(LED, !onFlag);
}

// mqtt message received
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.print("Non-JSON message: ");
    Serial.println(String(*payload));
    return;
  }
  root.printTo(Serial); //print out the parsed message
  Serial.println();
}

////// GET latitude / longitude from IP
void getGeo() {
  HTTPClient client;
  client.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + IP_STACK_ACCESS_KEY); //return IP as .json object
  int httpCode = client.GET();
  if (httpCode > 0) {
    if (httpCode == 200) {
      DynamicJsonBuffer jsonBuffer;
      String payload = client.getString();
      JsonObject& root = jsonBuffer.parse(payload);
      if (!root.success()) {
        Serial.println("parseObject() failed");
        return;
      }
      location.ip = root["ip"].as<String>();
      location.lt = root["latitude"].as<String>();
      location.ln = root["longitude"].as<String>();
    } else {
      Serial.println("Error in getGeo()");
    }
  }
}

// GET public IP
String getIP() {
  HTTPClient client;
  String ipAddress;
  client.begin("http://api.ipify.org/?format=json");
  int httpCode = client.GET();
  if (httpCode > 0) {
    if (httpCode == 200) {
      DynamicJsonBuffer jsonBuffer;
      String payload = client.getString();
      JsonObject& root = jsonBuffer.parse(payload);
      ipAddress = root["ip"].as<String>();
    } else {
      Serial.println("error in getIP");
      return "error";
    }
  }
  return ipAddress;
}

void printSerial(String label, float val, String unit){
  Serial.print(label);
  Serial.print(val);
  Serial.println(unit);
}
// print to screen
void printDisplay(int row, String label, float val, String unit){
  display.setCursor(0,row*12); // each row is 12 pixels below the last
  display.print(label);
  display.print(val);
  display.println(unit);
}
