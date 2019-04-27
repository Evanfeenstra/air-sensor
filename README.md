# open source pollution / weather sensor

**This sensor reports CO2 levels, temperature, humidity, and current location to an MQTT broker.**

![sensor](https://github.com/Evanfeenstra/air-sensor/blob/master/sensor.png)

**To build it, you will need:**

- Feather ESP8266 [☆](https://www.adafruit.com/product/2821)
- DHT22 temperature/humidity sensor [☆](https://www.adafruit.com/product/385)
- MQ-135 gas sensor [☆](https://www.amazon.com/gp/product/B00LSG5IZ2)
- SSD1306 OLED Screen [☆](https://www.amazon.com/Xiuxin-I2C-OLED-Display-SSD1306/dp/B07B8JT1ZZ)
- button [☆](https://www.adafruit.com/product/1119)
- breadboard [☆](https://www.adafruit.com/product/239)
- wire and wirestrippers
- soldering iron (or buy the components with headers already attached)

![pollutionsensor](https://github.com/Evanfeenstra/air-sensor/blob/master/pollution-sensor.png)

### configuring 

You can configure the connectivity settings at the top of the main.cpp file. Here are the values that you need to change:
```
#define WIFI_SSID "***"
#define WIFI_PASS "***"
#define MQTT_SERVER "***"
#define MQTT_USER "***"
#define MQTT_PASSWORD "***"
#define MQTT_TOPIC "messages"
#define MQTT_ACK_TOPIC "streams" // subscribe to an acknowledgement topic
#define IP_STACK_ACCESS_KEY "***"
```

Notes:
- IP Stack allows you to get geolocation data from your public IP address. 
- The sensor posts to the topic specified in the MQTT_TOPIC value, suffixed with the unique chip ID of the esp8266. (messages/009481B5)