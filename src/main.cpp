
#include <EEPROM.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>

// Replace with your network credentials
const char* ssid     = "varmar";
const char* password = "xxx";
char hostname[] = "Liikumisandur2";

#define INPUT_PIN 4 // PIR data pin goes to nodemcu GPIO-2

// Set web server port number to 80
ESP8266WebServer server(80);


uint addr = 0;

// fake data
struct {
  uint mqtt_port = 0;
  uint idx = 0;
  char mqtt_server[20] = "";
  uint config = 0;
} data;

String temp_str;
char temp[80];

int oldInputState;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);


void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266! \n\n http://IP/set?mqtt_server=[MQTT SERVER]&mqtt_port=[MQTT port]&idx=[IDX]");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    server.handleClient(); // give a chance to set mqtt server
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("tele/esp8266_sensor_test/temp", "23");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into
  // a 512-byte-array cache in RAM
  EEPROM.begin(512);

  // read bytes (i.e. sizeof(data) from "EEPROM"),
  // in reality, reads from byte-array cache
  // cast bytes into structure called data
  EEPROM.get(addr,data);
  Serial.println("Old values are: "+String(data.idx)+","+String(data.mqtt_port)+","+String(data.mqtt_server));

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);

  wifi_station_set_hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/set", []() {
    Serial.println(server.arg(0));
    Serial.println(server.arg(1));
    Serial.println(server.arg(2));
    server.arg(0).toCharArray(data.mqtt_server, 20);
    data.mqtt_port = server.arg(1).toInt();
    data.idx = server.arg(2).toInt();
    data.config = true;
    EEPROM.put(addr,data);
    EEPROM.commit();

    server.send(200, "text/plain", "Done");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  mqtt_client.setServer(data.mqtt_server, data.mqtt_port);

}

void loop() {
  server.handleClient();

   if (!mqtt_client.connected() && data.config == 1) {
     reconnect();
   }
   mqtt_client.loop();

   int inputState = digitalRead(INPUT_PIN);

   if (inputState != oldInputState) {

     // This sends off your payload.
     temp_str = "{\"idx\":"+ String(data.idx) +",\"nvalue\":0,\"svalue\": \"" + String(inputState) + "\",\"Battery\":48,\"RSSI\":5}"; //converting ftemp (the float variable above) to a string
     temp_str.toCharArray(temp, temp_str.length() + 1); //packaging up the data to publish to mqtt whoa..
     mqtt_client.publish("domoticz/in", temp);

     oldInputState = inputState;
   }

}
