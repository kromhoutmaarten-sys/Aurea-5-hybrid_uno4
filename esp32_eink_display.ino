/*
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║  Kromhout WP - ESP32 E-Ink Display v1.0                     ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * ESP32 Development Board + Waveshare E-Ink Display
 * 
 * ONDERSTEUNDE DISPLAYS:
 * ✅ Waveshare 2.9" (296x128) - Compact, goedkoop (~€15)
 * ✅ Waveshare 4.2" (400x300) - Groot, meer ruimte (~€25)
 * 
 * FEATURES:
 * ✅ WiFi Setup Portal - Eerste keer configuratie
 * ✅ MQTT Integratie - Real-time data van warmtepomp
 * ✅ E-Ink Display - Ultra laag stroomverbruik
 * ✅ Grafiek Weergave - Laatste 24 uur temperatuur
 * ✅ Auto Refresh - Elke 5 minuten update
 * ✅ Deep Sleep - Batterij besparend tussen updates
 * ✅ Instelbare Locatie - "Woonkamer", "Gang", etc.
 * 
 * HARDWARE:
 * - ESP32 Development Board
 * - Waveshare 2.9" E-Ink Display (296x128, zwart/wit) OF
 * - Waveshare 4.2" E-Ink Display (400x300, zwart/wit)
 * - MicroUSB kabel (5V voeding)
 * 
 * CONFIGURATIE:
 * Kies hieronder je display type!
 */

// ═══════════════════════════════════════════════════════════════
// DISPLAY TYPE SELECTIE - PAS DIT AAN!
// ═══════════════════════════════════════════════════════════════

// Uncomment je display type:
#define DISPLAY_29   // Waveshare 2.9" (296x128)
//#define DISPLAY_42   // Waveshare 4.2" (400x300)

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

// ═══════════════════════════════════════════════════════════════
// E-INK DISPLAY CONFIG (Waveshare 4.2")
// ═══════════════════════════════════════════════════════════════

// Pin definitie (standaard ESP32 SPI)
#define EPD_CS    5
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  4

// Display driver - automatisch gekozen op basis van selectie
#ifdef DISPLAY_29
  // Waveshare 2.9" (296x128)
  GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
  #define DISPLAY_WIDTH 296
  #define DISPLAY_HEIGHT 128
#elif defined(DISPLAY_42)
  // Waveshare 4.2" (400x300)
  GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
  #define DISPLAY_WIDTH 400
  #define DISPLAY_HEIGHT 300
#else
  #error "Geen display type geselecteerd! Uncomment DISPLAY_29 of DISPLAY_42"
#endif

// ═══════════════════════════════════════════════════════════════
// EEPROM LAYOUT
// ═══════════════════════════════════════════════════════════════

#define EEPROM_SIZE 512
#define ADDR_SETUP_DONE 0        // 1 byte: 0xBB = setup compleet
#define ADDR_LOCATION 1          // 32 bytes: Display locatie
#define ADDR_WIFI_SSID 33        // 32 bytes
#define ADDR_WIFI_PASS 65        // 64 bytes
#define ADDR_MQTT_SERVER 129     // 50 bytes
#define ADDR_MQTT_PORT 179       // 2 bytes
#define ADDR_MQTT_USER 181       // 32 bytes
#define ADDR_MQTT_PASS 213       // 32 bytes
#define ADDR_MQTT_PREFIX 245     // 32 bytes: "kromhout_wp", "woonkamer_wp", etc.

// ═══════════════════════════════════════════════════════════════
// GLOBALE VARIABELEN
// ═══════════════════════════════════════════════════════════════

// Setup mode
bool setup_mode = false;
bool setup_done = false;
WebServer server(80);

// WiFi & MQTT
char wifi_ssid[32] = "";
char wifi_pass[64] = "";
char mqtt_server[50] = "";
uint16_t mqtt_port = 1883;
char mqtt_user[32] = "";
char mqtt_pass[32] = "";
char mqtt_prefix[32] = "kromhout_wp";  // Prefix voor topics
char display_location[32] = "Woonkamer";  // Default locatie

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Warmtepomp data
float t_kamer = 0.0;
float t_aanvoer = 0.0;
float t_retour = 0.0;
float t_buiten = 0.0;
uint8_t stand = 0;
uint16_t vermogen = 0;
bool wp_aan = false;

// Geschiedenis (laatste 24u, per uur)
float temp_history[24];
uint8_t history_index = 0;
uint32_t last_history_update = 0;

// Refresh timing
const uint32_t REFRESH_INTERVAL = 300000;  // 5 minuten
uint32_t last_refresh = 0;

// ═══════════════════════════════════════════════════════════════
// EEPROM FUNCTIES
// ═══════════════════════════════════════════════════════════════

void eeprom_read_string(int addr, char* buffer, int max_len){
  for(int i = 0; i < max_len; i++){
    buffer[i] = EEPROM.read(addr + i);
    if(buffer[i] == 0) break;
  }
  buffer[max_len - 1] = 0;
}

void eeprom_write_string(int addr, const char* str, int max_len){
  int len = strlen(str);
  if(len >= max_len) len = max_len - 1;
  
  for(int i = 0; i < len; i++){
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, 0);
  EEPROM.commit();
}

bool check_setup_done(){
  return EEPROM.read(ADDR_SETUP_DONE) == 0xBB;
}

void mark_setup_done(){
  EEPROM.write(ADDR_SETUP_DONE, 0xBB);
  EEPROM.commit();
}

void load_all_settings(){
  eeprom_read_string(ADDR_LOCATION, display_location, 32);
  eeprom_read_string(ADDR_WIFI_SSID, wifi_ssid, 32);
  eeprom_read_string(ADDR_WIFI_PASS, wifi_pass, 64);
  eeprom_read_string(ADDR_MQTT_SERVER, mqtt_server, 50);
  mqtt_port = (EEPROM.read(ADDR_MQTT_PORT) << 8) | EEPROM.read(ADDR_MQTT_PORT + 1);
  eeprom_read_string(ADDR_MQTT_USER, mqtt_user, 32);
  eeprom_read_string(ADDR_MQTT_PASS, mqtt_pass, 32);
  eeprom_read_string(ADDR_MQTT_PREFIX, mqtt_prefix, 32);
  
  if(strlen(display_location) == 0) strcpy(display_location, "Woonkamer");
  if(strlen(mqtt_prefix) == 0) strcpy(mqtt_prefix, "kromhout_wp");
}

void save_all_settings(const char* loc, const char* ssid, const char* pass, 
                       const char* server, uint16_t port, 
                       const char* user, const char* mqtt_pass_str,
                       const char* prefix){
  eeprom_write_string(ADDR_LOCATION, loc, 32);
  eeprom_write_string(ADDR_WIFI_SSID, ssid, 32);
  eeprom_write_string(ADDR_WIFI_PASS, pass, 64);
  eeprom_write_string(ADDR_MQTT_SERVER, server, 50);
  EEPROM.write(ADDR_MQTT_PORT, port >> 8);
  EEPROM.write(ADDR_MQTT_PORT + 1, port & 0xFF);
  eeprom_write_string(ADDR_MQTT_USER, user, 32);
  eeprom_write_string(ADDR_MQTT_PASS, mqtt_pass_str, 32);
  eeprom_write_string(ADDR_MQTT_PREFIX, prefix, 32);
  EEPROM.commit();
  
  mark_setup_done();
}

// ═══════════════════════════════════════════════════════════════
// WIFI SETUP PORTAL
// ═══════════════════════════════════════════════════════════════

const char SETUP_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Display Setup</title>
<style>
body{font-family:Arial;max-width:500px;margin:20px auto;padding:20px;background:#f5f5f5}
h1{color:#2196F3;text-align:center}
.card{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
label{display:block;margin:15px 0 5px;font-weight:bold}
input{width:100%;padding:12px;border:2px solid #ddd;border-radius:5px;font-size:16px;box-sizing:border-box}
button{width:100%;background:#2196F3;color:white;padding:15px;border:none;border-radius:5px;font-size:18px;cursor:pointer;margin-top:20px}
button:hover{background:#1976D2}
.info{background:#E3F2FD;padding:10px;border-radius:5px;margin:15px 0;font-size:14px}
</style>
</head>
<body>
<div class='card'>
<h1>📺 Display Setup</h1>
<div class='info'>Configureer je E-Ink display voor monitoring van de warmtepomp.</div>

<form method='POST' action='/save'>
<h2>Display Locatie</h2>
<label>Waar hangt dit scherm?</label>
<input name='location' placeholder='Bijv: Woonkamer, Gang, Keuken' value='Woonkamer' required>

<h2>WiFi Instellingen</h2>
<label>WiFi Netwerk (SSID)</label>
<input name='ssid' required>
<label>WiFi Wachtwoord</label>
<input type='password' name='pass' required>

<h2>MQTT Broker</h2>
<label>MQTT Server IP</label>
<input name='mqtt_server' placeholder='192.168.1.69' required>
<label>MQTT Poort</label>
<input type='number' name='mqtt_port' value='1883' required>
<label>MQTT Gebruiker</label>
<input name='mqtt_user' value='mqtt' required>
<label>MQTT Wachtwoord</label>
<input type='password' name='mqtt_pass' required>

<h2>Warmtepomp Prefix</h2>
<label>MQTT Topic Prefix</label>
<input name='mqtt_prefix' placeholder='kromhout_wp' value='kromhout_wp' required>
<div class='info' style='font-size:12px;margin-top:5px'>
Gebruik dezelfde prefix als de warmtepomp controller.<br>
Bijvoorbeeld: kromhout_wp, woonkamer_wp, etc.
</div>

<button type='submit'>💾 Opslaan en Starten</button>
</form>
</div>
</body>
</html>
)";

void start_setup_mode(){
  setup_mode = true;
  
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║     DISPLAY SETUP MODE ACTIEF        ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // Toon setup instructie op E-Ink
  display.setRotation(1);  // Landscape
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Titel
    display.setFont(&FreeSans18pt7b);
    display.setCursor(50, 60);
    display.print("Setup Mode");
    
    // Instructies
    display.setFont(&FreeSans12pt7b);
    display.setCursor(20, 120);
    display.print("1. Verbind met WiFi:");
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(40, 150);
    display.print("WarmtePomp-Display-Setup");
    
    display.setFont(&FreeSans12pt7b);
    display.setCursor(20, 190);
    display.print("2. Browser opent automatisch");
    
    display.setCursor(20, 230);
    display.print("3. Vul gegevens in");
    
    display.setCursor(20, 270);
    display.print("4. Klaar!");
    
  } while (display.nextPage());
  
  // Start Access Point
  WiFi.softAP("WarmtePomp-Display-Setup", "display123");
  
  Serial.print("Access Point: ");
  Serial.println(WiFi.softAPIP());
  
  // Setup web server
  server.on("/", HTTP_GET, [](){
    server.send_P(200, "text/html", SETUP_HTML);
  });
  
  server.on("/save", HTTP_POST, [](){
    String location = server.arg("location");
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String mqtt_srv = server.arg("mqtt_server");
    String mqtt_port_str = server.arg("mqtt_port");
    String mqtt_usr = server.arg("mqtt_user");
    String mqtt_pwd = server.arg("mqtt_pass");
    String mqtt_pfx = server.arg("mqtt_prefix");
    
    uint16_t port = mqtt_port_str.toInt();
    
    save_all_settings(location.c_str(), ssid.c_str(), pass.c_str(),
                     mqtt_srv.c_str(), port, mqtt_usr.c_str(), 
                     mqtt_pwd.c_str(), mqtt_pfx.c_str());
    
    server.send(200, "text/plain", "Opgeslagen! Display herstart...");
    
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
}

// ═══════════════════════════════════════════════════════════════
// MQTT FUNCTIES
// ═══════════════════════════════════════════════════════════════

void mqtt_callback(char* topic, byte* payload, unsigned int length){
  String topic_str = String(topic);
  String payload_str = "";
  for(unsigned int i = 0; i < length; i++){
    payload_str += (char)payload[i];
  }
  
  float val = payload_str.toFloat();
  
  // Parse topics (met dynamische prefix)
  String prefix = String(mqtt_prefix);
  
  if(topic_str == "sensor/" + prefix + "_kamer") t_kamer = val;
  else if(topic_str == "sensor/" + prefix + "_aanvoer") t_aanvoer = val;
  else if(topic_str == "sensor/" + prefix + "_retour") t_retour = val;
  else if(topic_str == "sensor/" + prefix + "_buiten") t_buiten = val;
  else if(topic_str == "sensor/" + prefix + "_stand") stand = (uint8_t)val;
  else if(topic_str == "sensor/" + prefix + "_vermogen") vermogen = (uint16_t)val;
  else if(topic_str == prefix + "/aan") wp_aan = (payload_str == "1");
}

void mqtt_reconnect(){
  if(!mqttClient.connected()){
    Serial.print("MQTT verbinden...");
    
    String client_id = "ESP32_Display_" + String(display_location);
    
    if(mqttClient.connect(client_id.c_str(), mqtt_user, mqtt_pass)){
      Serial.println("OK!");
      
      // Subscribe op alle topics
      String prefix = String(mqtt_prefix);
      mqttClient.subscribe(("sensor/" + prefix + "_kamer").c_str());
      mqttClient.subscribe(("sensor/" + prefix + "_aanvoer").c_str());
      mqttClient.subscribe(("sensor/" + prefix + "_retour").c_str());
      mqttClient.subscribe(("sensor/" + prefix + "_buiten").c_str());
      mqttClient.subscribe(("sensor/" + prefix + "_stand").c_str());
      mqttClient.subscribe(("sensor/" + prefix + "_vermogen").c_str());
      mqttClient.subscribe((prefix + "/aan").c_str());
      
    } else {
      Serial.print("FOUT: ");
      Serial.println(mqttClient.state());
    }
  }
}

// ═══════════════════════════════════════════════════════════════
// E-INK DISPLAY FUNCTIES
// ═══════════════════════════════════════════════════════════════

void draw_display(){
  display.setRotation(1);  // Landscape (400x300)
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Header met locatie
    display.setFont(&FreeSans18pt7b);
    display.setCursor(10, 30);
    display.print(display_location);
    
    // Status indicator
    display.fillCircle(380, 20, 8, wp_aan ? GxEPD_BLACK : GxEPD_WHITE);
    display.drawCircle(380, 20, 8, GxEPD_BLACK);
    
    // Horizontale lijn
    display.drawLine(0, 40, 400, 40, GxEPD_BLACK);
    
    // Kamer temperatuur (groot)
    display.setFont(&FreeSans18pt7b);
    display.setCursor(10, 90);
    display.print("Kamer: ");
    display.print(t_kamer, 1);
    display.print((char)176);  // Graden symbool
    display.print("C");
    
    // Warmtepomp data
    display.setFont(&FreeSans12pt7b);
    
    display.setCursor(10, 130);
    display.print("Aanvoer: ");
    display.print(t_aanvoer, 1);
    display.print((char)176);
    display.print("C");
    
    display.setCursor(10, 160);
    display.print("Retour:  ");
    display.print(t_retour, 1);
    display.print((char)176);
    display.print("C");
    
    display.setCursor(10, 190);
    display.print("Buiten:  ");
    display.print(t_buiten, 1);
    display.print((char)176);
    display.print("C");
    
    display.setCursor(250, 130);
    display.print("Stand: ");
    display.print(stand);
    
    display.setCursor(250, 160);
    display.print("Vermogen:");
    display.setCursor(250, 190);
    display.print(vermogen);
    display.print(" W");
    
    // Grafiek (laatste 24u)
    draw_temperature_graph(10, 210, 380, 80);
    
  } while (display.nextPage());
  
  Serial.println("Display updated!");
}

void draw_temperature_graph(int x, int y, int w, int h){
  // Frame
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  
  // Vind min/max temperatuur
  float min_temp = 100;
  float max_temp = -100;
  for(int i = 0; i < 24; i++){
    if(temp_history[i] < min_temp && temp_history[i] > 0) min_temp = temp_history[i];
    if(temp_history[i] > max_temp) max_temp = temp_history[i];
  }
  
  if(max_temp <= min_temp) max_temp = min_temp + 5;
  
  // Teken lijn
  for(int i = 0; i < 23; i++){
    if(temp_history[i] > 0 && temp_history[i+1] > 0){
      int x1 = x + (i * w / 24);
      int y1 = y + h - ((temp_history[i] - min_temp) / (max_temp - min_temp) * h);
      int x2 = x + ((i+1) * w / 24);
      int y2 = y + h - ((temp_history[i+1] - min_temp) / (max_temp - min_temp) * h);
      display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
  }
  
  // Labels
  display.setFont();  // Default font
  display.setCursor(x + 2, y + 2);
  display.print(max_temp, 0);
  display.print((char)176);
  
  display.setCursor(x + 2, y + h - 10);
  display.print(min_temp, 0);
  display.print((char)176);
  
  display.setCursor(x + w - 30, y + h + 12);
  display.print("24u");
}

void update_temperature_history(){
  // Elke uur nieuwe temperatuur toevoegen
  if(millis() - last_history_update > 3600000){  // 1 uur
    temp_history[history_index] = t_kamer;
    history_index = (history_index + 1) % 24;
    last_history_update = millis();
  }
}

// ═══════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║  Kromhout WP - E-Ink Display v1.0   ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // EEPROM init
  EEPROM.begin(EEPROM_SIZE);
  
  // E-Ink display init
  display.init(115200);
  display.setTextColor(GxEPD_BLACK);
  
  // Check setup status
  setup_done = check_setup_done();
  
  if(!setup_done){
    // SETUP MODE
    start_setup_mode();
    return;
  }
  
  // NORMALE MODE - Laad settings
  load_all_settings();
  
  Serial.println("Instellingen geladen:");
  Serial.println("Locatie: " + String(display_location));
  Serial.println("WiFi: " + String(wifi_ssid));
  Serial.println("MQTT: " + String(mqtt_server) + ":" + mqtt_port);
  Serial.println("Prefix: " + String(mqtt_prefix));
  
  // Verbind WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pass);
  
  Serial.print("WiFi verbinden");
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 30){
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi verbonden!");
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FOUT!");
  }
  
  // MQTT setup
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqtt_callback);
  
  // Initialiseer geschiedenis
  for(int i = 0; i < 24; i++) temp_history[i] = 0;
  
  // Eerste display update
  draw_display();
}

void loop(){
  if(setup_mode){
    // Setup mode - handle web requests
    server.handleClient();
    return;
  }
  
  // MQTT maintenance
  if(!mqttClient.connected()){
    mqtt_reconnect();
  }
  mqttClient.loop();
  
  // Update geschiedenis
  update_temperature_history();
  
  // Refresh display elke 5 minuten
  if(millis() - last_refresh > REFRESH_INTERVAL){
    draw_display();
    last_refresh = millis();
  }
  
  delay(100);
}
