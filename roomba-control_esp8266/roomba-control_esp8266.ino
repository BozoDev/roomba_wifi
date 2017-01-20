/******************************************************************************
 * @file    roomba ESP-8266 firmware
 * @author  Rémi Pincent - INRIA
 * @date    16 Jan. 2017   
 *
 * @brief ESP-8266 controlling Roomba over WIFI
 * 
 * Project : roomba_wifi - https://github.com/OpHaCo/roomba_wifi
 * Contact:  Rémi Pincent - remi.pincent@inria.fr
 * 
 * Revision History:
 * Refer https://github.com/OpHaCo/roomba_wifi
 * 
 * LICENSING
 * roomba_wifi (c) by Rémi Pincent
 * 
 * roomba_wifi is licensed under a
 * Creative Commons Attribution 3.0 Unported License.
 * 
 * Credits : 
 *    https://community.sparkdevices.com/t/sparkbot-manually-automatically-vacuum-your-living-room/625
 *    http://skaterj10.hackhut.com/2013/01/22/roomba-hacking-101/
 *    https://community.particle.io/t/sparkbot-spark-core-roomba/625
 *    https://github.com/incmve/roomba-esp8266
 * 
 * You should have received a copy of the license along with this
 * work.  If not, see <http://creativecommons.org/licenses/by/3.0/>.
 *****************************************************************************/

const String roomba_wifiVersion = "1.2.3";

/**************************************************************************
 * Include Files
 **************************************************************************/
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
// #include <ESP8266SSDP.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
/**************************************************************************
 * Taken from github.com/incmve/roomba-esp8266
 */
#include <TimeLib.h>
#include <ESP8266WebServer.h>
#include "base64.h"
#include <FS.h>

// Div
File UploadFile;
String fileName;
String BSlocal = "0";
int FSTotal;
int FSUsed;
//-------------- FSBrowser application -----------
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

/**************************************************************************
 * Macros
 **************************************************************************/
#define NO_BYTE_READ 0x100
// #define DEBUG_ESP
#if defined(DEBUG_ESP)
  #define LOG_SERIAL Serial
  #define SERIAL_ROOMBA_CMD_TX Serial1
  /** Only TX on Serial1 can be used */
  #define SERIAL_ROOMBA_CMD_RX Serial
#else
  #define LOG_SERIAL Serial1
  #define SERIAL_ROOMBA_CMD_TX Serial
  #define SERIAL_ROOMBA_CMD_RX Serial
#endif
#define USESPIFFS

/**************************************************************************
 * Manifest Constants
 **************************************************************************/
static const char* _mqttRoombaCmdsTopic = "roomba/roombaCmds";
static const char* _mqttRoombaAliveTopic = "roomba/alive";
static const char* _mqttRoombaLogTopic = "roomba/log";
static const char* _mqttRoombaName = "roomba";
static const char* _mqttServer = "192.168.1.92";

static const int _ddPin = 0;                      // _ddPin controls clean button was: D7
static const int sleepTimeS = 3480;               //58 mins 3480s - I like to put it to sleep at night from 20.00 to 800
                                                  //  by sending a sleep command every hour...
                                                  // Dammit - didn't work as expected - didn't come online again -
                                                  //  luckily I had the leads to the reset pin ;)
                                                  // ESP-01 needs hardware mod for this to work:
                                                  //  http://tim.jagenberg.info/2015/01/18/low-power-esp8266/
/**************************************************************************
 * Taken from github.com/incmve/roomba-esp8266
 */
// webserver
ESP8266WebServer  server(80);
MDNSResponder   mdns;
WiFiClient client;
// AP fallback mode when WIFI not available
const char *APssid = "Roombot";
const char *APpassword = "ThereIsAFork";  // Also used for OTA-auth
const char *espName = "Roombot";
static String WMode = "1";
#if defined(USESPIFFS)
  const char *roombaWifiCfgFile = "/private/wifi.cfg";
#endif
static boolean isInOTA = false;

/**************************************************************************
 * Type Definitions
 **************************************************************************/
typedef enum{
    OFF = 1,
    PASSIVE = 2,
    SAFE = 3,
    FULL = 4
}EIOMode;

typedef enum{
  NOTCHARGING = 1,
  RECONDITION = 2,
  FULLCH = 3,
  TRICKLE = 4,
  WAITING = 5,
  FAULTCH = 6
}EOICharge;

typedef struct{
    String cmdName;
    void (*controlCmd)(void);
}TsControlCommandMap;
typedef struct{
    String cmdName;
    void (*controlCmd)(int);
}TsControlCommandMapA;

typedef struct{
    String cmdName;
    void (*controlCmd)(void);
}TsCommandMap;

typedef struct{
    String cmdName;
    int (*inputCmd)(void);
}TsInputCommandMap;

/**************************************************************************
 * Local Functions Declarations
 **************************************************************************/
static void connectMQTT(void);
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void setupWifi(void);
static void start_AP(void);
static void setupOTA(void);
static void setupWebServer(void);
static void goNiki(void);
static void goForward();
static void goForwardAtSpeed(int _speed);
static void goBackward();
static void spinLeft();
static void spinRight();
static void stop();
static void updateSensors();
static void playSong();
static void createSong1();
static void playSong1();
static void createErrorBeep();
static void playErrorBeep();
static void vibgyor();
static void vacuumOn();
static void vacuumOff();
static void goHome();
static void clean();
static void spot();
static void powerOn();
static void powerOff();
static void gainControl();
static void setPassiveMode();
static void setSafeMode();
static void freeControl();
static EIOMode getMode(void);
static int8_t getBatTemp(void);
static uint16_t getBatteryCharge(void);
static uint16_t getBatteryCap(void);
static EOICharge getChargingMode(void);
static int readByte(int8_t& readByte, int timeout);
static int roombaControl(String command, int val=0);
/**************************************************************************
 * Taken from github.com/incmve/roomba-esp8266
 */
static void setup_SPIFFS(void);
static void setupWebServer(void);
static void handle_root();
static String getContentType(String filename);
static bool handle_fileRead(String path);
static void handleFormat();
static void handle_fupload_html();
static void handle_api();
static void handle_updatefwm_html();
static void handle_filemanager_ajax();
static void handleFileDelete();
static void handle_roomba_start();
static void handle_roomba_dock();
static void handle_esp_restart();
static void handle_config();
static void handle_wifi_configPost();
static void handle_mqtt_configPost();
static String get_next_config_var(File f);
static String get_next_config_val(File f);
static void http_error(String msg);

/*********************************************************
 * COMMANDS REQUIRING AT LEAST PASSIVE MODE - AFTER 
 * COMMAND EXECUTION ROOMBA GOES BACK IN PASSIVE MODE
 ********************************************************/ 
static TsControlCommandMap pToPControlCmds[] = 
{
    {"GOHOME",               &goHome},
    {"CLEAN",                &clean},
    {"SPOT",                 &spot},
    {"GONIKI",               &goNiki},
    {"POWEROFF",             &powerOff},
};

/*********************************************************
 * COMMANDS REQUIRING SAFE MODE - AFTER COMMAND EXECUTION 
 * ROOMBA STAY IN THIS MODE
 ********************************************************/ 
static TsControlCommandMap sToSControlCmds[] = 
{
    {"STOP",                 &stop},
    {"BACKWARD",             &goBackward},
    {"FORWARD",              &goForward},
    {"RIGHT",                &spinRight},
    {"LEFT",                 &spinLeft},
    {"VACUUMON",             &vacuumOn},
    {"VACUUMOFF",            &vacuumOff},
    {"SONG",                 &playSong},
    {"SONG1",                &playSong1},
    {"ERRORBEEP",            &playErrorBeep},
    {"VIBGYOR",              &vibgyor}
};
static TsControlCommandMapA sToSControlCmdsA[] = 
{
  {"FORWARDS",              &goForwardAtSpeed},
};

/**********************************************
 * FOR THESE COMMANDS NO MODE REQUIRED
 *********************************************/ 
static TsControlCommandMap cmds[] = 
{
    {"POWERON",              &powerOn},
    {"FREECONTROL",          &freeControl},
    {"GAINCONTROL",          &gainControl}
};

/**********************************************
 * INPUT CMDS
 *********************************************/ 
static TsInputCommandMap inputCmds[] = 
{
    {"GETMODE",                (int (*)(void)) (&getMode)},
    {"GETBATT",                (int (*)(void)) (&getBatteryCharge)},
};


/**************************************************************************
 * Static Variables
 **************************************************************************/                              
static char _sensorbytes[10];
static WiFiClient _wifiClient;
static PubSubClient _mqttClient(_mqttServer, 1883, mqttCallback, _wifiClient);
/** connection indicator */
static bool _ledStatus = HIGH;

/**************************************************************************
 * Taken from github.com/incmve/roomba-esp8266
 */
// HTML
static String header           =  "<html lang='en'><head><title>Roomba-WiFi control panel</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js'></script><script src='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js'></script><meta http-equiv='refresh' content='50'></head><body>";
static String headerRedir      =  "<html lang='en'><head><meta http-equiv='refresh' content='2; url=http://" + String(espName) + "/' /></head><body>";
static String navbar           =  "<nav class='navbar navbar-default'><div class='container-fluid'><div class='navbar-header'><a class='navbar-brand' href='/'>Roomba-WiFi control panel</a></div><div><ul class='nav navbar-nav'><li><a href='/'><span class='glyphicon glyphicon-info-sign'></span> Status</a></li><li class='dropdown'><a class='dropdown-toggle' data-toggle='dropdown' href='#'><span class='glyphicon glyphicon-cog'></span> Tools<span class='caret'></span></a><ul class='dropdown-menu'><li><a href='/updatefwm'><span class='glyphicon glyphicon-upload'></span> Firmware</a></li><li><a href='/filemanager.html'><span class='glyphicon glyphicon-file'></span> File manager</a></li><li><a href='/fupload'> File upload</a></li><li><a href='/setup.html'><span class='glyphicon glyphicon-file'></span> Settings</a></li></ul></li><li><a href='https://github.com/incmve/roomba-eps8266/wiki' target='_blank'><span class='glyphicon glyphicon-question-sign'></span> Help</a></li></ul></div></div></nav>";
static String containerStart   =  "<div class='container'><div class='row'>";
static String containerEnd     =  "<div class='clearfix visible-lg'></div></div></div>";
static String siteEnd          =  "</body></html>";

static String panelHeaderName  =  "<div class='col-md-4'><div class='page-header'><h1>";
static String panelHeaderEnd   =  "</h1></div>";
static String panelEnd         =  "</div>";

static String panelBodySymbol  =  "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-";
static String panelBodyName    =  "'></span> ";
static String panelBodyValue   =  "<span class='pull-right'>";
static String panelcenter      =  "<div class='row'><div class='span6' style='text-align:center'>";
static String panelBodyEnd     =  "</span></div></div>";

static String inputBodyStart   =  "<form action='' method='POST'><div class='panel panel-default'><div class='panel-body'>";
static String inputBodyName    =  "<div class='form-group'><div class='input-group'><span class='input-group-addon' id='basic-addon1'>";
static String inputBodyPOST    =  "</span><input type='text' name='";
static String inputBodyClose   =  "' class='form-control' aria-describedby='basic-addon1'></div></div>";
static String roombactrl       =  "<a href='/roombastart'<button type='button' class='btn btn-default'><span class='glyphicon glyphicon-play' aria-hidden='true'></span> Start</button></a><a href='/roombadock'<button type='button' class='btn btn-default'><span class='glyphicon glyphicon-home' aria-hidden='true'></span> Dock</button></a></div>";
String ClientIP;
// I'm using an ESP-01 module with limited pins:
#define SERIAL_RX     3  // pin for SoftwareSerial RX 5
#define SERIAL_TX     1  // pin for SoftwareSerial TX 6
/**************************************************************************
 * Global Functions Defintions
 **************************************************************************/
void setup() {
  // Pinmode definitions and begin serial
  pinMode(_ddPin, INPUT_PULLUP);                     // sets the pins as input
  SERIAL_ROOMBA_CMD_TX.begin(115200);
  #if defined(DEBUG_ESP)
  // On ESP-01 the following would land in the Roomba, so we only use it whilst devel stage
    LOG_SERIAL.begin(115200);
    LOG_SERIAL.println("start roomba control");
  #endif

//  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  _ledStatus = HIGH;
//  digitalWrite(LED_BUILTIN, _ledStatus);
  roombaControl("POWERON");
  delay(10);
  setSafeMode();
  delay(10);
  createErrorBeep();
  delay(10);
  createSong1();
  #if defined(USESPIFFS)
    setup_SPIFFS();
  #endif
  setupWifi();
  connectMQTT();
  setupOTA();
  setupWebServer();
  
  int i=0;
  //while (!mdns.begin(espName.c_str(), WiFi.localIP()) && i < 10) {
  while (!mdns.begin(espName, WiFi.localIP()) && i < 10) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.print(",");
    #endif
    delay(500);
    ++i;
  }
  if ( i >= 10 ) {
    #if defined(DEBUG_ESP)
      String _dbg = "Error setting up MDNS responder for " + String(espName) + " on " + WiFi.localIP();
      LOG_SERIAL.println(_dbg);
    #endif
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("MDNS responder for " + String(espName) + " registered");
    #endif
  }
  setSafeMode();
  delay(10);
  playSong1();
  delay(5000);
  roombaControl("FREECONTROL");
  delay(10);
  roombaControl("POWEROFF");
}

void loop() {
  ArduinoOTA.handle();
  if ((WiFi.status() != WL_CONNECTED) && (WMode != "AP")) {
    setupWifi();
  } else {
    // Don't interfere with OTA:
    if (!isInOTA) {
      server.handleClient();
      // Own AP-mode would usually imply no connection to other servers...
      if ( WMode != "AP" ) {
        _mqttClient.loop();
        if (!_mqttClient.connected()) {
          connectMQTT();
        }
      }
    }
  }
}

/**************************************************************************
 * Local Functions Definitions
 **************************************************************************/
#if defined(USESPIFFS)
static void setup_SPIFFS() {
  // Check if SPIFFS is OK
  size_t FSTotal;
  size_t FSUsed;
  if (!SPIFFS.begin()) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("SPIFFS failed, needs formatting");
    #endif
    handleFormat();
    delay(500);
    ESP.restart();
  } else {
    #if defined(DEBUG_ESP)
      FSInfo fs_info;
      if (!SPIFFS.info(fs_info)) {
        LOG_SERIAL.println("fs_info failed");
      } else {
        FSTotal = fs_info.totalBytes;
        FSUsed = fs_info.usedBytes;
        LOG_SERIAL.println("SPIFFS Size:" + String(FSTotal, DEC) + " and used: " + String(FSUsed, DEC));
      }
    #endif
  }
  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("SPIFFS setup");
  #endif
}
#endif

 /**************************************************************************
 * MQTT Part
 */
static void connectMQTT(void) {
  if (WMode == "AP") return;
  int tries=0;
  #if defined(DEBUG_ESP)
    LOG_SERIAL.print("Connecting to ");
    LOG_SERIAL.print(_mqttServer);
    LOG_SERIAL.print(" as ");
    LOG_SERIAL.println(_mqttRoombaName);
    // Beware on ESP-01 this is Ser-TX
    _ledStatus = HIGH;
    digitalWrite(LED_BUILTIN, _ledStatus);
  #endif
  while (tries < 15) {
    if (_mqttClient.connect(_mqttRoombaName)) {
      _mqttClient.loop();
      _mqttClient.subscribe(_mqttRoombaCmdsTopic);
      #if defined(DEBUG_ESP)
        LOG_SERIAL.println("Connected to MQTT broker");
        LOG_SERIAL.print("sending alive message to topic ");
        LOG_SERIAL.println(_mqttRoombaAliveTopic);
      #endif
      if (!_mqttClient.publish(_mqttRoombaAliveTopic, "alive")) {
        #if defined(DEBUG_ESP)
          LOG_SERIAL.println("Publish failed");
        #endif
      }
      /** if mqtt client loop not called when several mqtt api calls are done.
      * At some point, mqtt client api calls fail
      * */
      _mqttClient.loop();
      #if defined(DEBUG_ESP)
        _ledStatus = LOW;
        digitalWrite(LED_BUILTIN, _ledStatus);
      #endif
      return;
    } else {
      #if defined(DEBUG_ESP)
        LOG_SERIAL.println("connection to MQTT failed\n");
      #endif
    }
    #if defined(DEBUG_ESP)
      _ledStatus = !_ledStatus;
      digitalWrite(LED_BUILTIN, _ledStatus);
    #endif
    delay(1000);
    ++tries;
  }
  if (tries >14) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("Too many connection to MQTT failed\n");
    #else
      roombaControl("ERRORBEEP");
    #endif
  }
}
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);
  #if defined(DEBUG_ESP)
    LOG_SERIAL.print("received MQTT payload ");
    LOG_SERIAL.print(message);
    LOG_SERIAL.print(" on topic ");
    LOG_SERIAL.println(topic);
  #endif
  if(strcmp(topic, _mqttRoombaCmdsTopic) == 0) {
      roombaControl(message);
  }
}

/**************************************************************************
 * WiFi Part
 */
static void setupWifi(void) {

  #if defined(DEBUG_ESP)
    _ledStatus = HIGH;
    digitalWrite(LED_BUILTIN, _ledStatus);
  #endif
  
  #if defined(USESPIFFS)
    if (SPIFFS.exists(roombaWifiCfgFile)) {
      File f = SPIFFS.open(roombaWifiCfgFile, "r");
      
    }
  #endif
  WiFi.begin();
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31) {
    #if defined(DEBUG_ESP)
      _ledStatus = !_ledStatus;
      digitalWrite(LED_BUILTIN, _ledStatus);
      printf(".");
    #endif
    delay(500);
    ++i;
  }
  if (WiFi.status() == WL_CONNECTED) {
    #if defined(DEBUG_ESP)
      IPAddress ip = WiFi.localIP();
      LOG_SERIAL.println("WiFi connected");
      LOG_SERIAL.println("My IP: " + String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]));
    #endif
    return;
  }
  #if defined(USESPIFFS)
    else {
      #if defined(DEBUG_ESP)
        LOG_SERIAL.println("");
        LOG_SERIAL.println("Couldn't connect to network :( ");
        LOG_SERIAL.println("Trying to read SPIFFS-config next...");
      #else
        roombaControl("ERRORBEEP");
      #endif
      if (SPIFFS.exists(roombaWifiCfgFile)) {
        String _SSID;
        String _Pass;
        File f = SPIFFS.open(roombaWifiCfgFile, "r");
        while ((_SSID != "SSID") && f.available()) _SSID = get_next_config_var(f);
        if (f.available()) _SSID = get_next_config_val(f);
        else _SSID = "";
        f.seek(0, SeekSet);
        while ((_Pass != "PASS") && f.available()) _Pass = get_next_config_var(f);
        if (f.available()) _Pass = get_next_config_val(f);
        else _Pass = "";
        if (_SSID != "") {
          #if defined(DEBUG_ESP)
            LOG_SERIAL.println("Trying from Config-read:");
            LOG_SERIAL.println("SSID \"" + _SSID + "\"");
            LOG_SERIAL.println("Pass \"" + _Pass + "\"");
          #endif
          WiFi.begin(_SSID.c_str(), _Pass.c_str());
          i = 0;
          while (WiFi.status() != WL_CONNECTED && i < 31) {
            #if defined(DEBUG_ESP)
              _ledStatus = !_ledStatus;
              digitalWrite(LED_BUILTIN, _ledStatus);
              printf(".");
            #endif
            delay(500);
            ++i;
          }
          if (WiFi.status() == WL_CONNECTED) {
            #if defined(DEBUG_ESP)
              IPAddress ip = WiFi.localIP();
              LOG_SERIAL.println("WiFi connected");
              LOG_SERIAL.println("My IP: " + String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]));
            #endif
            return;
          }
        }
      }
    }
  #endif
  if (WiFi.status() != WL_CONNECTED && i >= 30) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("");
      LOG_SERIAL.println("Couldn't connect to network :( ");
      LOG_SERIAL.println("Setting up access point");
    #else
      roombaControl("ERRORBEEP");
    #endif
    start_AP();
  }
}
static void start_AP() {
    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid, APpassword);
    WMode = "AP";
    #if defined(DEBUG_ESP)
      IPAddress myIP = WiFi.softAPIP();
      LOG_SERIAL.print("Connected to ");
      LOG_SERIAL.println(APssid);
      LOG_SERIAL.print("IP address: ");
      LOG_SERIAL.println(myIP);
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on
    #endif
}

/**************************************************************************
 * OTA update part
 */
static void setupOTA(void) {
  // The basics
  ArduinoOTA.setPort(8266); // Default: 8266
  ArduinoOTA.setHostname(espName);
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("8d83b63a2954a8f31c6673e28a36e69b"); // echo "ThereIsAFork"|md5sum
  ArduinoOTA.setPassword(APpassword);
  //Set actions for OTA
  ArduinoOTA.onStart([]() {
    String type;
    isInOTA = true;
    if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
    else type = "filesystem";
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("Start updating " + type);
    #endif
  });
  ArduinoOTA.onEnd([]() {
    isInOTA = false;
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("\nUpload done...");
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    isInOTA = false;
    #if defined(DEBUG_ESP)
    // TODO: Publish errors via MTTQ?
      LOG_SERIAL.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) LOG_SERIAL.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) LOG_SERIAL.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) LOG_SERIAL.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) LOG_SERIAL.println("Receive Failed");
      else if (error == OTA_END_ERROR) LOG_SERIAL.println("End Failed");
    #endif
  });
  ArduinoOTA.begin();
  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("OTA initialized");
  #endif
}

/**************************************************************************
 * Webserver part
 * Taken from github.com/incmve/roomba-esp8266
 */
static void setupWebServer(void) {
  server.on("/", handle_root);
#if defined(USESPIFFS)
  server.on("/", handle_fupload_html);
#endif
  server.on("/api", handle_api);
#if defined(USESPIFFS)
  server.on("/format", handleFormat);
  server.on("/updatefwm", handle_updatefwm_html);
  server.on("/fupload", handle_fupload_html);
  server.on("/filemanager_ajax", handle_filemanager_ajax);
  server.on("/delete", handleFileDelete);
#endif
  server.on("/roombastart", handle_roomba_start);
  server.on("/roombadock", handle_roomba_dock);
  server.on("/restart", handle_esp_restart);
  server.on("/setup.html", handle_config);
  server.on("/wifisetup", HTTP_POST, handle_wifi_configPost);
#if defined(USESPIFFS)
  server.on("/mqttsetup", HTTP_POST, handle_mqtt_configPost);
#endif

  // Upload firmware:
  server.on("/updatefw2", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []()
  {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      fileName = upload.filename;
      LOG_SERIAL.setDebugOutput(true);
      LOG_SERIAL.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(LOG_SERIAL);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(LOG_SERIAL);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        LOG_SERIAL.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      LOG_SERIAL.setDebugOutput(false);
    }
    yield();
  });
  
#if defined(USESPIFFS)
  // upload file to SPIFFS
  server.on("/fupload2", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      fileName = upload.filename;
      LOG_SERIAL.setDebugOutput(true);
      //fileName = upload.filename;
      LOG_SERIAL.println("Upload Name: " + fileName);
      String path;
      if (fileName.indexOf(".css") >= 0) {
        path = "/css/" + fileName;
      } else if (fileName.indexOf(".js") >= 0) {
        path = "/js/" + fileName;
      } else if (fileName.indexOf(".otf") >= 0 || fileName.indexOf(".eot") >= 0 || fileName.indexOf(".svg") >= 0 || fileName.indexOf(".ttf") >= 0 || fileName.indexOf(".woff") >= 0 || fileName.indexOf(".woff2") >= 0) {
        path = "/fonts/" + fileName;
      } else {
        path = "/data/" + fileName;
      }
      UploadFile = SPIFFS.open(path, "w");
      // already existing file will be overwritten!
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (UploadFile) UploadFile.write(upload.buf, upload.currentSize);
      #if defined(DEBUG_ESP)
        LOG_SERIAL.println(fileName + " size: " + upload.currentSize);
      #endif
    } else if (upload.status == UPLOAD_FILE_END) {
      #if defined(DEBUG_ESP)
        LOG_SERIAL.print("Upload Size: ");
        LOG_SERIAL.println(upload.totalSize);  // need 2 commands to work!
      #endif
      if (UploadFile) UploadFile.close();
    }
    yield();
  });
#endif

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    #if defined(USESPIFFS)
      if (!handle_fileRead(server.uri())) server.send(404, "text/plain", "FileNotFound");
    #else 
      server.send(404, "text/plain", "FileNotFound");
    #endif
  });
  server.begin();
  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("HTTP server started");
  #endif
}

// ROOT page
void handle_root() {
  String s_currMode = "Undef.";
  String s_chargeMode = "Undef.";
  String s_chargeState = String(getBatteryCharge(),DEC);
  String s_chargeCap = String(getBatteryCap());
  
  EIOMode e_currMode = getMode();
  switch (e_currMode) {
    case OFF:
      s_currMode = "Off";
      break;
    case PASSIVE:
      s_currMode = "Off";
      break;
    case SAFE:
      s_currMode = "Safe";
      break;
    case FULL:
      s_currMode = "Full";
      break;
    default:
      s_currMode = "Unknown (" + String(e_currMode, DEC) + ")";
      break;
  }
  EOICharge e_chargeMode = getChargingMode();
  switch (e_chargeMode) {
    case NOTCHARGING:
      s_chargeMode = "Not charging";
      break;
    case RECONDITION:
      s_chargeMode = "Reconditioning charge";
      break;
    case FULLCH:
      s_chargeMode = "Full charge";
      break;
    case TRICKLE:
      s_chargeMode = "Trickle charge";
      break;  
    case WAITING:
      s_chargeMode = "Waiting charge";
      break;
    case FAULTCH:
      s_chargeMode = "Charging Fault";
      break;
    default:
      s_chargeMode = "Unknown (" + String(e_chargeMode, DEC) + ")";
      break;
  }

  // get IP
  IPAddress ip = WiFi.localIP();
  ClientIP = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  delay(500);

  String title1       = panelHeaderName + String("Roomba-WiFi Main Page") + panelHeaderEnd;
  String IPAddClient  = panelBodySymbol + String("globe") + panelBodyName + String("IP Address") + panelBodyValue + ClientIP + panelBodyEnd;
  String ClientName   = panelBodySymbol + String("tag") + panelBodyName + String("Client Name") + panelBodyValue + espName + panelBodyEnd;
  String Version      = panelBodySymbol + String("info-sign") + panelBodyName + String("Roomba-WiFi Version") + panelBodyValue + roomba_wifiVersion + panelBodyEnd;
  String OIState      = panelBodySymbol + String("info-sign") + panelBodyName + String("Roomba Mode") + panelBodyValue + s_currMode + panelBodyEnd;
  String Charge       = panelBodySymbol + String("info-sign") + panelBodyName + String("Charging state:") + panelBodyValue + s_chargeState + String(" of ") + s_chargeCap + String("mAh in ") + s_chargeMode + String(" mode") + panelBodyEnd + panelEnd;
  String Uptime       = panelBodySymbol + String("time") + panelBodyName + String("Uptime") + panelBodyValue + hour() + String(" h ") + minute() + String(" min ") + second() + String(" sec")+ panelBodyEnd + panelEnd;
  
  String title2       = panelHeaderName + String("MQTT server") + panelHeaderEnd;
  String IPAddServ    = panelBodySymbol + String("Broker") + panelBodyName + String("IP Address") + panelBodyValue + _mqttServer + panelBodyEnd;
  String User         = panelBodySymbol + String("Publisher") + panelBodyName + String("ID") + panelBodyValue + _mqttRoombaName + panelBodyEnd + panelEnd;

  String title3 = panelHeaderName + String("Commands") + panelHeaderEnd;
  String commands = panelBodySymbol + panelBodyName + panelcenter + roombactrl + panelBodyEnd;

  server.send ( 200, "text/html", header + navbar + containerStart + title1 + IPAddClient + ClientName + Version + OIState + Charge + Uptime + title2 + IPAddServ + User + title3 + commands + containerEnd + siteEnd);
}
String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
#if defined(USESPIFFS)
bool handle_fileRead(String path) {
  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("Trying to read: " + path);
  #endif
  if ( path.startsWith("/private/")) return false; // Don't serve private config data to simple http-gets
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  String pathWithData = "/data" + path;
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(pathWithData) || SPIFFS.exists(path))  {
    if (SPIFFS.exists(pathWithGz)) { path += ".gz"; }
    else if (SPIFFS.exists(pathWithData)) { path = pathWithData; }
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("File " + path + " found");
    #endif
    File file = SPIFFS.open(path, "r");
    if ( (path.startsWith("/css/") || path.startsWith("/js/") || path.startsWith("/fonts/")) &&  !path.startsWith("/js/insert")) {
      server.sendHeader("Cache-Control", " max-age=31104000");
    } else {
      server.sendHeader("Connection", "close");
    }
    size_t sent = server.streamFile(file, contentType);
    size_t contentLength = file.size();
    file.close();
    return true;
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("Blank: " + path);
    #endif
  }
  return false;
}
void handleFormat() {
  int FSTotal;
  int FSUsed;

  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("Format SPIFFS");
  #endif
  if (SPIFFS.format()) {
    delay(10000);
    if (!SPIFFS.begin()) {
      #if defined(DEBUG_ESP)
        LOG_SERIAL.println("Format SPIFFS failed");
      #endif
    }
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("Format SPIFFS failed");
    #endif
  }
  if (!SPIFFS.begin()) {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("SPIFFS failed, needs formatting");
    #endif
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("SPIFFS mounted");
    #endif
    FSInfo fs_info;
    FSTotal = fs_info.totalBytes;
    FSUsed = fs_info.usedBytes;
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("SPIFFS Size:" + String(FSTotal, DEC) + " and used: " + String(FSUsed, DEC));
    #endif
  }
  server.send ( 200, "text/html", String(headerRedir) + "OK" + String(siteEnd));
}
void handle_fupload_html() {
  String HTML = "<br>Files on flash:<br>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    HTML += fileName.c_str();
    HTML += " ";
    HTML += formatBytes(fileSize).c_str();
    HTML += " , ";
    HTML += fileSize;
    HTML += "<br>";
    //Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  server.send ( 200, "text/html", "<form method='POST' action='/fupload2' enctype='multipart/form-data'><input type='file' name='update' multiple><input type='submit' value='Update'></form><br<b>For webfiles only!!</b>Multiple files possible<br>" + HTML);
}
#endif
void handle_api() {
  // Get vars for all commands
  String action = server.arg("action");
  String value = server.arg("value");
  String api = server.arg("api");

  if (action == "clean" && value == "start") handle_roomba_start();
  else if (action == "dock" && value == "home") handle_roomba_dock();
  else if (action == "reset" && value == "true") ESP.restart();
  else if (action == "sleep" && value == "set") ESP.deepSleep(sleepTimeS * 1000000, RF_CAL); // see comment in define section
  else if (action == "battery" && value == "get") { server.send(200, "text/plain", String(getBatteryCharge()/655)); return; }
  else if (action == "mode" && value == "get") { server.send(200, "text/plain", String(getMode())); return; }
  else if (action == "battemp" && value == "get") { server.send(200, "text/plain", String(getBatTemp())); return; }
  else {
    server.send(404, "text/plain", "CommandNotFound");
    roombaControl("ERRORBEEP");
    return;
  }
  server.send ( 200, "text/html", String(headerRedir) + "OK" + String(siteEnd));
}
void handle_updatefwm_html() {
  server.send ( 200, "text/html", "<form method='POST' action='/updatefw2' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form><br<b>For firmware only!!</b>");
}
void handle_filemanager_ajax() {
  String form = server.arg("form");
  if (form != "filemanager") {
    String HTML;
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      HTML += String("<option>") + fileName + String("</option>");
    }
    // Glue everything together and send to client
    server.send(200, "text/html", HTML);
  }
}
void handleFileDelete() {
  if (server.args() == 0) return http_error("BAD ARGS");
  String path = server.arg(0);
  if (!path.startsWith("/")) path = "/" + path;
  #if defined(DEBUG_ESP)
    LOG_SERIAL.println("handleFileDelete: " + path);
  #endif
  if (path == "/")
    return http_error("BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}
void handle_roomba_start() {
  server.send ( 200, "text/html", String(headerRedir) + "OK" + String(siteEnd));
  roombaControl("CLEAN");
}
void handle_roomba_dock() {
  server.send ( 200, "text/html", String(headerRedir) + "OK" + String(siteEnd));
  roombaControl("GOHOME");
}
void handle_esp_restart() {
  server.send ( 200, "text/html", String(headerRedir) + "OK" + String(siteEnd));
  ESP.restart();
}
void handle_config() {
  String HTML;
  // get IP
  IPAddress ip = WiFi.localIP();
  String ClientIP = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String settingName;
  String l_ssid;
  String l_pass;
  String l_broker;
  String l_topic;

  #if defined(USESPIFFS)
  // Read entire config and sort in vals
  if (SPIFFS.begin()) {
    if (SPIFFS.exists(roombaWifiCfgFile)) {
      File f = SPIFFS.open(roombaWifiCfgFile, "r");
      while (f.available()) {
        settingName = get_next_config_var(f);
        // Apply the value to the parameter
        if (settingName == "SSID") l_ssid = get_next_config_val(f);
        else if (settingName == "PASS") l_pass = get_next_config_val(f);
        else if (settingName == "BROKER") l_broker = get_next_config_val(f);
        else if (settingName == "TOPIC") l_topic = get_next_config_val(f);
        // Reset Strings
        settingName = "";
      }
      f.close();
    }
  }
  #endif
  // Build HTML page with existing vals pre-filled
  HTML = header + navbar + containerStart;
  HTML += panelHeaderName + String("Roomba WiFi Config") + panelHeaderEnd;
  HTML += panelBodySymbol + String("globe") + panelBodyName + String("IP Address") + panelBodyValue + ClientIP + panelBodyEnd;
  HTML += panelBodySymbol + String("globe") + panelBodyName + "<form method='POST' action='/wifisetup' enctype='multipart/form-data'>SSID: <input type='text' name='ssid' value='" + String(l_ssid) +"'><BR>Pass: <input type='password' name='wifipass'><BR><input type='submit' value='Save'></form>" + "Beware: no \"[]=\" allowed in password" + panelBodyEnd;
  HTML += panelBodySymbol + String("globe") + panelBodyName + "<form method='POST' action='/mqttsetup' enctype='multipart/form-data'>Broker: <input type='text' name='broker' value='" + String(l_broker) +"'><BR>Command-Topic: <input type='text' name='topic' value='" + String(l_topic) + "'><BR><input type='submit' value='Save'></form>"+ panelBodyEnd;

  HTML += containerEnd + siteEnd;
  server.send ( 200, "text/html", HTML);
}
#if defined(USESPIFFS)
void handle_wifi_configPost() {
  if(!(server.hasArg("ssid") && server.hasArg("wifipass")))  return http_error("BAD ARGS");
  String _SSID = server.arg("ssid");
  String _PASS = server.arg("wifipass");
  String settingName;
  String _s;

  // My vals:
  _s = "[SSID=" + String(_SSID) + "]\n";
  _s += "[PASS=" + String(_PASS) + "]\n";
  
  // Existing vals:
  if (SPIFFS.exists(roombaWifiCfgFile)){
    File f = SPIFFS.open(roombaWifiCfgFile, "r");
    while (f.available()) {
      settingName = get_next_config_var(f);
      if ((settingName != "SSID") && (settingName != "PASS") && (settingName != "")){
        _s += "[" + String(settingName) + "=" + get_next_config_val(f) + "]\n";
      }
    }
    f.close();
  }

  // Overwrite config with freshly sorted new version:
  File f = SPIFFS.open(roombaWifiCfgFile, "w");
  f.printf(_s.c_str());
  f.close();
  server.send(200, "text/html", String(headerRedir) + "Set SSID to \"" + String(_SSID) + "\" and pass \"" + String(_PASS) + "\"" + String(siteEnd));
  // Turns out, that apparently a WiFi.begin with no parameters connects to the last (good?) one
  // So, let's see if we can fire up Wifi, successfully connect, and hope the ESP-Wifi
  //  then takes care of storing the credentials for us...
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(_SSID.c_str(), _PASS.c_str());
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31) {
    #if defined(DEBUG_ESP)
      _ledStatus = !_ledStatus;
      digitalWrite(LED_BUILTIN, _ledStatus);
      printf(".");
    #endif
    delay(500);
    ++i;
  }
  if (WiFi.status() == WL_CONNECTED && i < 32) {
    #if defined(DEBUG_ESP)
      IPAddress ip = WiFi.localIP();
      LOG_SERIAL.println("WiFi connected");
      LOG_SERIAL.println("My IP: " + String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]));
    #endif
    
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("WiFi connect failed");
      LOG_SERIAL.println("Calling setupWifi to try again or (?re-)create AP");
    #endif
    WiFi.disconnect();
    delay(1000);
    setupWifi();
  }
}
void handle_mqtt_configPost() {
  if(!(server.hasArg("broker") && server.hasArg("topic")))  return http_error("BAD ARGS");
  String _broker = server.arg("broker");
  String _topic = server.arg("topic");
  String settingName;
  String _s;

  _s = "[BROKER=" + String(_broker)  + "]\n";
  _s += "[TOPIC=" + String(_topic)  + "]\n";
  if (SPIFFS.exists(roombaWifiCfgFile)){
    File f = SPIFFS.open(roombaWifiCfgFile, "r");
    while (f.available()) {
      settingName = get_next_config_var(f);
      if ((settingName != "BROKER") && (settingName != "TOPIC") && (settingName != "")){
        _s += "[" + String(settingName) + "=" + get_next_config_val(f) + "]\n";
      } 
    }
    f.close();
  }
  File f = SPIFFS.open(roombaWifiCfgFile, "w");
  f.printf(_s.c_str());
  f.close();
  server.send(200, "text/html", String(headerRedir) + "Set MQTT-Broker to \"" + String(_broker) + "\" and Topic \"" + String(_topic) + "\"" + String(siteEnd));
}

String get_next_config_var(File f){
  char character = '0';
  String settingName;

  while((f.available()) && (character != '[')){
    character = f.read();
  }
  character = f.read();
  while((f.available()) && (character != '=')){
    settingName = settingName + character;
    character = f.read();
  }
    if(character == '=') return settingName;
 }
String get_next_config_val(File f){
  char character;
  String settingValue;

  character = f.read();
  while((f.available()) && (character != ']')){
    settingValue = settingValue + character;
    character = f.read();
  }
  if(character == ']') return settingValue;
}
#else
void handle_wifi_configPost() {
  if(!(server.hasArg("ssid") && server.hasArg("wifipass")))  return http_error("BAD ARGS");
  String _SSID = server.arg("ssid");
  String _PASS = server.arg("wifipass");

  server.send(200, "text/html", String(headerRedir) + "Set SSID to \"" + String(_SSID) + "\" and pass \"" + String(_PASS) + "\"" + String(siteEnd));
  // Turns out, that it's a mess trying to read this data at setup time
  // I also had problems on an ESP-01 with 512K flash - somehow formating the SPIFFS
  //  screwed up the sketch on the module... (possibly due to crossing 256K... and trying to also use OTA)
  // Stuff like changing static const is no fun... (with undefined variable length)
  // So, let's see if we can fire up Wifi, successfully connect, and hope the ESP-Wifi
  //  then takes care of storing the credentials for us...
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(_SSID.c_str(), _PASS.c_str());
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31) {
    #if defined(DEBUG_ESP)
      _ledStatus = !_ledStatus;
      digitalWrite(LED_BUILTIN, _ledStatus);
      printf(".");
    #endif
    delay(500);
    ++i;
  }

  if (WiFi.status() != WL_CONNECTED && i >= 30) {
    WiFi.disconnect();
    delay(1000);
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("");
      LOG_SERIAL.println("Couldn't connect to network :( ");
      LOG_SERIAL.println("Setting up access point");
      LOG_SERIAL.println("SSID: ");
      LOG_SERIAL.println(APssid);
      LOG_SERIAL.println("password: ");
      LOG_SERIAL.println(APpassword);
    #else
      roombaControl("ERRORBEEP");
    #endif
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid, APpassword);
    WMode = "AP";
    IPAddress myIP = WiFi.softAPIP();
    #if defined(DEBUG_ESP)
      LOG_SERIAL.print("Connected to ");
      LOG_SERIAL.println(APssid);
      LOG_SERIAL.print("IP address: ");
      LOG_SERIAL.println(myIP);
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on
    #endif
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.println("");
      LOG_SERIAL.println("WiFi connected");  
      LOG_SERIAL.println("IP address: ");
      LOG_SERIAL.println(WiFi.localIP());
    #endif
    WMode = "1";
  }
}

#endif
void http_error(String msg){
  server.send(500, "text/plain", msg);
}

static void goNiki(){
  int _speed = 250;            // Full speed is 500
  roombaControl("BACKWARD");
  delay(12000);
  roombaControl("STOP");
  roombaControl("RIGHT");
  delay(800);
  roombaControl("FORWARDS", _speed);
  delay(14000);
  roombaControl("LEFT");
  delay(350);
  roombaControl("FORWARD");
  delay(1500);
  roombaControl("STOP");
  delay(10);
  roombaControl("SPOT");
}

/**************************************************************************
 * Actual Roomba serial commands
 */
static void stop() {
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);              // 0 means stop
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x80);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
}
static void goForward() {
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x01);              // 0x01F4 = 500 = full speed ahead!
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xF4);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x80);              // Our roomba has a limp, so it this correction keeps it straight
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
}
static void goForwardAtSpeed(int _speed) {
  if ((_speed >= 0) && (_speed < 501)) {
    SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
    SERIAL_ROOMBA_CMD_TX.write(highByte(_speed));
    SERIAL_ROOMBA_CMD_TX.write(lowByte(_speed));
    SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x80);
    SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
  } else {
    #if defined(DEBUG_ESP)
      LOG_SERIAL.printf("Wrong speed value");
    #else
      roombaControl("ERRORBEEP");
    #endif
  }
}
static void goBackward() {
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xff);              // 0xff38 == -200
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x38);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x80);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
}
static void spinLeft() {
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);              // 0x00c8 == 200
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xc8);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x01);              // 0x0001 == 1 == spin left
}
static void spinRight() {
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 137);               // DRIVE command
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0x00);              // 0x00c8 == 200
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xc8);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xff);
  SERIAL_ROOMBA_CMD_TX.write((unsigned char) 0xff);              // 0xffff == -1 == spin right
}
static void clean() {                                      
  SERIAL_ROOMBA_CMD_TX.write(135);                               // Starts a cleaning cycle, so command 143 can be initiated
}
static void spot() {
  SERIAL_ROOMBA_CMD_TX.write(134);
}
static void powerOn() {
    //GPIO connected to clean button
    pinMode(_ddPin, OUTPUT);
    digitalWrite(_ddPin, LOW);
    delay(300);
    pinMode(_ddPin, INPUT_PULLUP);
}
static void powerOff() {
/*    //GPIO connected to clean button
    pinMode(_ddPin, OUTPUT);
    digitalWrite(_ddPin, LOW);
    delay(5000);
    pinMode(_ddPin, INPUT_PULLUP);
    */
    
    SERIAL_ROOMBA_CMD_TX.write(133);
}
static void goHome() {                                     // Sends the Roomba back to it's charging base
  clean();                                          // Starts a cleaning cycle, so command 143 can be initiated
  delay(5000);
  SERIAL_ROOMBA_CMD_TX.write(143);                               // Sends the Roomba home to charge
  delay(50);
}
static void playSong() {                                   // Makes the Roomba play a little ditty
  SERIAL_ROOMBA_CMD_TX.write(140);                         	    // Define a new song
  SERIAL_ROOMBA_CMD_TX.write(0);                                 // Write to song slot #0
  SERIAL_ROOMBA_CMD_TX.write(8);                                 // 8 notes long
  SERIAL_ROOMBA_CMD_TX.write(60);                                // Everything below defines the C Major scale 
  SERIAL_ROOMBA_CMD_TX.write(32); 
  SERIAL_ROOMBA_CMD_TX.write(62);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(64);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(65);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(67);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(69);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(71);
  SERIAL_ROOMBA_CMD_TX.write(32);
  SERIAL_ROOMBA_CMD_TX.write(72);
  SERIAL_ROOMBA_CMD_TX.write(32);
 
  SERIAL_ROOMBA_CMD_TX.write(141);                               // Play a song
  SERIAL_ROOMBA_CMD_TX.write(0);                                 // Play song slot #0
}
static void createSong1() {
  SERIAL_ROOMBA_CMD_TX.write(140);                               // Define a new song
  SERIAL_ROOMBA_CMD_TX.write(1);                                 // Write to song slot #1
  SERIAL_ROOMBA_CMD_TX.write(4);                                 // 4 notes long
  SERIAL_ROOMBA_CMD_TX.write(60);                                // Everything below defines the C Major scale 
  SERIAL_ROOMBA_CMD_TX.write(32); 
  SERIAL_ROOMBA_CMD_TX.write(62);
  SERIAL_ROOMBA_CMD_TX.write(22);
  SERIAL_ROOMBA_CMD_TX.write(64);
  SERIAL_ROOMBA_CMD_TX.write(12);
  SERIAL_ROOMBA_CMD_TX.write(65);
  SERIAL_ROOMBA_CMD_TX.write(6);
}
static void playSong1() {                                   // Makes the Roomba play a little ditty
  setPassiveMode();
  SERIAL_ROOMBA_CMD_TX.write(141);                               // Play a song
  SERIAL_ROOMBA_CMD_TX.write(1);                                 // Play song slot #1
}
static void createErrorBeep() {
  SERIAL_ROOMBA_CMD_TX.write(140);                               // Define a new song
  SERIAL_ROOMBA_CMD_TX.write(2);                                 // Write to song slot #2
  SERIAL_ROOMBA_CMD_TX.write(3);                                 // 3 notes long
  SERIAL_ROOMBA_CMD_TX.write(60);                                // Everything below defines the C Major scale 
  SERIAL_ROOMBA_CMD_TX.write(22); 
  SERIAL_ROOMBA_CMD_TX.write(72);
  SERIAL_ROOMBA_CMD_TX.write(12);
  SERIAL_ROOMBA_CMD_TX.write(74);
  SERIAL_ROOMBA_CMD_TX.write(12);
}
static void playErrorBeep() {
  SERIAL_ROOMBA_CMD_TX.write(141);                               // Play a song
  SERIAL_ROOMBA_CMD_TX.write(2);                                 // Play song slot #2
}
static void vacuumOn() {                                   // Turns on the vacuum
  SERIAL_ROOMBA_CMD_TX.write(138);
  SERIAL_ROOMBA_CMD_TX.write(7);
}
static void vacuumOff() {                                  // Turns off the vacuum
  SERIAL_ROOMBA_CMD_TX.write(138);
  SERIAL_ROOMBA_CMD_TX.write(0);
}
static void vibgyor() {                                    // Makes the main LED behind the power button on the Roomba pulse from Green to Red
  SERIAL_ROOMBA_CMD_TX.write(139);
  for (int i=0;i<255;i++) { 
    SERIAL_ROOMBA_CMD_TX.write(139);                             // LED seiral command
    SERIAL_ROOMBA_CMD_TX.write(0);                               // We don't want any of the other LEDs
    SERIAL_ROOMBA_CMD_TX.write(i);                               // Color dependent on i
    SERIAL_ROOMBA_CMD_TX.write(255);                             // FULL INTENSITY!
    delay(5);                                       // Wait between cycles so the transition is visible
  }
  for (int i=0;i<255;i++) {
    SERIAL_ROOMBA_CMD_TX.write(139);
    SERIAL_ROOMBA_CMD_TX.write(0);
    SERIAL_ROOMBA_CMD_TX.write(i);
    SERIAL_ROOMBA_CMD_TX.write(255);
    delay(5);
  }
  for (int i=0;i<255;i++) {
    SERIAL_ROOMBA_CMD_TX.write(139);
    SERIAL_ROOMBA_CMD_TX.write(0);
    SERIAL_ROOMBA_CMD_TX.write(i);
    SERIAL_ROOMBA_CMD_TX.write(255);
    delay(5);
  }
  for (int i=0;i<255;i++) {
    SERIAL_ROOMBA_CMD_TX.write(139);
    SERIAL_ROOMBA_CMD_TX.write(0);
    SERIAL_ROOMBA_CMD_TX.write(i);
    SERIAL_ROOMBA_CMD_TX.write(255);
    delay(5);
  }
  SERIAL_ROOMBA_CMD_TX.write(139);
  SERIAL_ROOMBA_CMD_TX.write(0);
  SERIAL_ROOMBA_CMD_TX.write(0);
  SERIAL_ROOMBA_CMD_TX.write(0);
}
static void gainControl() {                                // Gain control of the Roomba once it's gone back into passive mode
  setSafeMode();
  SERIAL_ROOMBA_CMD_TX.write(132);                               // Full control mode
  delay(50);
}
static void setPassiveMode() {                                // Gain control of the Roomba once it's gone back into passive mode
  // Get the Roomba into control mode 
  SERIAL_ROOMBA_CMD_TX.write(128);   
  delay(50);    
}
static void setSafeMode() {                                // Gain control of the Roomba once it's gone back into passive mode
  setPassiveMode(); 
  SERIAL_ROOMBA_CMD_TX.write(130);                               // Safe mode
  delay(50);  
}
static void freeControl() {
  // Get the Roomba into control mode 
  SERIAL_ROOMBA_CMD_TX.write(128);                               // Passive mode
  delay(50);
}
static EIOMode getMode(void) {
  int8_t loc_readByte = 0;

  SERIAL_ROOMBA_CMD_TX.write(142);
  SERIAL_ROOMBA_CMD_TX.write(35);

  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return (EIOMode)-1;
  } else {
    /** +1 increment... in order to not returining 0 */
    return  (EIOMode)(loc_readByte + 1);
  }
}
static int8_t getBatTemp(void) {
  int8_t loc_readByte = 0;
  
  SERIAL_ROOMBA_CMD_TX.write(142);
  SERIAL_ROOMBA_CMD_TX.write(24);
  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return -1;
  }
  return loc_readByte;
}
static uint16_t getBatteryCharge() {
  int8_t loc_readByte = 0;
  uint16_t battLevel = 0;

  SERIAL_ROOMBA_CMD_TX.write(142);
  SERIAL_ROOMBA_CMD_TX.write(25);

  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return -1;
  }
  battLevel = loc_readByte << 8;
  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return -1;
  }
  return loc_readByte + battLevel;
}
static uint16_t getBatteryCap() {
  int8_t loc_readByte = 0;
  uint16_t battLevel = 0;

  SERIAL_ROOMBA_CMD_TX.write(142);
  SERIAL_ROOMBA_CMD_TX.write(26);
  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return -1;
  }
  battLevel = loc_readByte << 8;
  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return -1;
  }
  return loc_readByte + battLevel;
}
static EOICharge getChargingMode(void) {
  int8_t loc_readByte = 0;

  SERIAL_ROOMBA_CMD_TX.write(142);
  SERIAL_ROOMBA_CMD_TX.write(21);
  if(readByte(loc_readByte, 50) == NO_BYTE_READ) {
    return (EOICharge)-1;
  } else {
  return (EOICharge)(loc_readByte + 1);
  }
}
static int readByte(int8_t& readByte, int timeout) {
  int count = 0;
  const uint8_t DELAY = 5;
  //ceil 
  const int MAX_INDEX = 1 + ((timeout -1)/DELAY);

  for(count = 0; count <= MAX_INDEX; count++) {
    if(SERIAL_ROOMBA_CMD_RX.available()) {
      readByte = SERIAL_ROOMBA_CMD_RX.read();
      return  0;
    }
    delay(DELAY);
  }
  return NO_BYTE_READ;
}

static int roombaControl(String command, int val) {
  int cmdIndex = 0;    
  /****************************************************
   * HANDLE COMMANDS NOT REQUIRING ANY MODE
   * *************************************************/
  for(cmdIndex = 0; cmdIndex < sizeof(cmds)/sizeof(cmds[0]); cmdIndex++) {
    if(cmds[cmdIndex].cmdName.equals(command)) {
      cmds[cmdIndex].controlCmd();
      return 0;
    }
  }
  /****************************************************
   * HANDLE COMMANDS REQURING PASSIVE MODE
   * *************************************************/
  for(cmdIndex = 0; cmdIndex < sizeof(pToPControlCmds)/sizeof(pToPControlCmds)[0]; cmdIndex++) {
    if(pToPControlCmds[cmdIndex].cmdName.equals(command)) {
      setPassiveMode();
      pToPControlCmds[cmdIndex].controlCmd();
      return 0;
    }
  }
  /****************************************************
   * HANDLE COMMANDS REQURING SAFE MODE
   * *************************************************/
  for(cmdIndex = 0; cmdIndex < sizeof(sToSControlCmds)/sizeof(sToSControlCmds)[0]; cmdIndex++) {
    if(sToSControlCmds[cmdIndex].cmdName.equals(command)) {
      setSafeMode();
      sToSControlCmds[cmdIndex].controlCmd();
      /** ROOMBA NOW IN SAFE MODE!!! **/
      return 0;
    }
  }
  for(cmdIndex = 0; cmdIndex < sizeof(sToSControlCmdsA)/sizeof(sToSControlCmdsA)[0]; cmdIndex++) {
    if(sToSControlCmdsA[cmdIndex].cmdName.equals(command)) {
      setSafeMode();
      sToSControlCmdsA[cmdIndex].controlCmd(val);
      /** ROOMBA NOW IN SAFE MODE!!! **/
      return 0;
    }
  }
   /****************************************************
   * HANDLE INPUT COMMANDS
   * *************************************************/
  for(cmdIndex = 0; cmdIndex < sizeof(inputCmds)/sizeof(inputCmds)[0]; cmdIndex++) {
    if(inputCmds[cmdIndex].cmdName.equals(command)) {
      setPassiveMode();
      return inputCmds[cmdIndex].inputCmd();
    }
  }
  roombaControl("ERRORBEEP");
  // If none of the commands were executed, return false
  return -1;
}  

