#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

#define PUB_TOPIC "homeassistant/msg"
#define SUB_TOPIC "homeassistant/outmsg"

#define STATUS_LED D3 // Status LED on D3
#define DEBUG_EX_INFO 1

Ticker ticker;

// Callback for handling incoming mqtt message
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      
      // Once connected, publish an announcement...
      client.publish(PUB_TOPIC, "hello world");
      
      // ... and resubscribe
      client.subscribe(SUB_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void tick()
{
  //toggle state
  int state = digitalRead(STATUS_LED);  // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  //print the config portal SSID
  Serial.println(myWiFiManager->getConfigPortalSSID());

  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {

  // Default Init
  Serial.println("Device is Starting.....");
  Serial.begin(115200);
  pinMode(D2, INPUT);   // For Button
  pinMode(STATUS_LED, OUTPUT);  // For Status LED

  ticker.attach(1.0, tick); // Tick a bit slower to indicate waiting for user input

  // Instantiate WiFiManager Class
  WiFiManager wifi_manager;

  // Configure AP to use Static GW IP, web app can be accessed through 10.0.1.1
  wifi_manager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255,255,255,0));
  wifi_manager.setAPCallback(configModeCallback);

  /* This loop give the user 10s time to push the button for
  configuring the WiFi via web app */
  int counter = 0;
  for (counter = 0; counter < 100; counter++) {
    if (digitalRead(D2) == LOW) {
      if (!wifi_manager.startConfigPortal("Elapan Portal")) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again
        ESP.reset();
        delay(5000);
      }
      Serial.println("Device Connected");
      break;
    }
    delay(100);
  }

  // If WiFi is not connected yet
  if (!WiFi.isConnected()) {
    wifi_manager.autoConnect("Elapan Portal");
  }

  // Print acquired IP and SSID
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  
  // Start mDNS service to enable probing for HA Instance later
  if (!MDNS.begin("elapan")) {
    Serial.println("mDNS Fail...");
    delay(5000);
    ESP.reset();
  }
  else {
    Serial.println("mDNS Success...");
  }

  // Query the HA service as per documentation (_home-assistant._tcp)
  int answer = MDNS.queryService("_home-assistant", "_tcp", 2000);  // Set timeout 2s
  if (answer != 0) {
    Serial.print("Home Assistant Instance IP: ");
    Serial.println(MDNS.IP(0));

    // If HA Instance has been found, proceed to connect MQTT Client to Server
    IPAddress mqtt_server = MDNS.IP(0);
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    if (!client.connected()) {
      reconnect();
    }
  }
  else {
    Serial.println("Home Assistant Instance Not Found !");
    delay(5000);
    ESP.reset();
  }

  ticker.detach();
  
  //keep LED on
  digitalWrite(STATUS_LED, HIGH);
}

void loop() {
  // MQTT Handler here
  if (!client.connected()) {
    reconnect();
  }
  client.loop();


}