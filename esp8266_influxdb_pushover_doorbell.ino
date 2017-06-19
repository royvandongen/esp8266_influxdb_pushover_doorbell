#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>
#include <WiFiUdp.h>

const int buttonPin = 12;
const int relayPin = 5;

int doRing = 0;
int ringing = 0;
unsigned long buttonPushed = millis();
unsigned long startRing = millis();

long debouncing_time = 10000; //Debouncing Time in Milliseconds ( EG How long should the doorbell knob be ignored )
volatile unsigned long last_micros;

// InfluxDB Server
char INFLUXDB_SERVER[40];             // Your InfluxDB Server FQDN
char INFLUXDB_PORT[5] = "8089";       // Default InfluxDB UDP Port
char INFLUXDB_INTERVAL[6] = "10000";  // Milliseconds between measurements 
char SENSOR_LOCATION[20] = "test";    // This location is used for the "device=" part of the InfluxDB update

// Pushover
char PUSHOVER_TOKEN[40];              // Your Pushover Token
char PUSHOVER_USER[40];               // Your Pushover User


int length;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiClientSecure client;
WiFiUDP udp;

void setup() {
  Serial.begin ( 115200 );
  
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(INFLUXDB_SERVER, json["INFLUXDB_SERVER"]);
          strcpy(INFLUXDB_PORT, json["INFLUXDB_PORT"]);
          strcpy(INFLUXDB_INTERVAL, json["INFLUXDB_INTERVAL"]);
          strcpy(SENSOR_LOCATION, json["SENSOR_LOCATION"]);
          strcpy(PUSHOVER_TOKEN, json["PUSHOVER_TOKEN"]);
          strcpy(PUSHOVER_USER, json["PUSHOVER_USER"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_influxdb_server("server", "InfluxDB Server", INFLUXDB_SERVER, 40);
  WiFiManagerParameter custom_influxdb_port("port", "8089", INFLUXDB_PORT, 5);
  WiFiManagerParameter custom_influxdb_interval("interval", "10000", INFLUXDB_INTERVAL, 6);
  WiFiManagerParameter custom_sensor_location("location", "Location", SENSOR_LOCATION, 20);
  WiFiManagerParameter custom_pushover_token("token", "Pushover Token", PUSHOVER_TOKEN, 40);
  WiFiManagerParameter custom_pushover_user("token", "Pushover User", PUSHOVER_TOKEN, 40);

  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_influxdb_server);
  wifiManager.addParameter(&custom_influxdb_port);
  wifiManager.addParameter(&custom_influxdb_interval);
  wifiManager.addParameter(&custom_sensor_location);
  wifiManager.addParameter(&custom_pushover_token);
  wifiManager.addParameter(&custom_pushover_user);

  String hostname = "Doorbell-" + String(ESP.getChipId());
  WiFi.hostname("Doorbell-" + String(ESP.getChipId()));
  
  String ssid = "Doorbell-" + String(ESP.getChipId());
  if(!wifiManager.autoConnect(ssid.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(INFLUXDB_SERVER, custom_influxdb_server.getValue());
  strcpy(INFLUXDB_PORT, custom_influxdb_port.getValue());
  strcpy(INFLUXDB_INTERVAL, custom_influxdb_interval.getValue());
  strcpy(SENSOR_LOCATION, custom_sensor_location.getValue());
  strcpy(PUSHOVER_TOKEN, custom_pushover_token.getValue());
  strcpy(PUSHOVER_USER, custom_pushover_user.getValue());


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["INFLUXDB_SERVER"] = INFLUXDB_SERVER;
    json["INFLUXDB_PORT"] = INFLUXDB_PORT;
    json["INFLUXDB_INTERVAL"] = INFLUXDB_INTERVAL;
    json["SENSOR_LOCATION"] = SENSOR_LOCATION;
    json["INFLUXDB_INTERVAL"] = PUSHOVER_TOKEN;
    json["SENSOR_LOCATION"] = PUSHOVER_USER;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  Serial.println("");
  Serial.print ( "Connected to your network" );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  pinMode(relayPin, OUTPUT);      
  pinMode(buttonPin, INPUT);

  digitalWrite(relayPin, HIGH);
  delay(10);
  digitalWrite(relayPin, LOW);

  attachInterrupt(buttonPin, buttonPress, RISING);

  if(strlen(INFLUXDB_SERVER) == 0 || strlen(INFLUXDB_PORT) == 0 || strlen(INFLUXDB_INTERVAL) == 0 || strlen(SENSOR_LOCATION) == 0 || strlen(PUSHOVER_TOKEN) == 0 || strlen(PUSHOVER_USER) == 0) {
    Serial.print("Config Faulty, Kicking config");
    SPIFFS.format();
    wifiManager.resetSettings();
    delay(2000);
    ESP.reset();
  }
}

void loop() {
  if(doRing == 1) {
    Serial.println("doRing started");
    ringOn();
    startRing = millis();
    doRing = 0;
  }
  if(ringing == 1) {
    if(millis() - 1000 >= startRing) {
      ringOff();
    }
  }
}


void ringOn() {
  Serial.println("ringOn started");
  digitalWrite(relayPin, HIGH);
  ringing = 1;
}

void ringOff() {
  Serial.println("ringOff started");
  digitalWrite(relayPin, LOW);
  ringing = 0;
  pushover("Someone is at the door!");
  sendData();
}


void buttonPress () {
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    doRing = 1;
    
  }
  last_micros = micros();
  Serial.print("last Micros is : ");
  Serial.println(last_micros);
  Serial.println("Interrupted");
}

byte pushover(char *pushovermessage) {
  
  String Msg = pushovermessage;
  String Message = "token="+String(PUSHOVER_TOKEN)+"&user="+String(PUSHOVER_USER)+"&sound=bugle&message="+Msg;
  Serial.println(Message);
  length = Message.length();
    if (client.connect("api.pushover.net", 443)) {
      Serial.println("Sending message…");
      client.println("POST /1/messages.json HTTP/1.1");
      client.println("Host: api.pushover.net");
      client.println("Connection: close\r\nContent-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.print(length);
      client.println("\r\n");
      client.print(Message);
      /* Uncomment this to receive a reply from Pushover server:
      while(client.connected()) {
        while(client.available()) {
          char ch = client.read();
          Serial.write(ch);
        }
      }
      */
      client.stop();
      Serial.println("Done");
      Serial.println("");
      delay(100);
    }
}

void sendData() {
 
  String line;
  line = String("doorbell,device=" + String(SENSOR_LOCATION) + " value=1");
  Serial.println(line);

  // send the packet
  Serial.println("Sending first UDP packet...");
  udp.beginPacket(INFLUXDB_SERVER, atoi(INFLUXDB_PORT));
  udp.print(line);
  udp.endPacket();
}
