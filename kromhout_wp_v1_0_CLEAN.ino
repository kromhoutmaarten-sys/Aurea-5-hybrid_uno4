/*
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║  Kromhout WP Controller v1.0 FINAL                          ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * Arduino UNO R4 WiFi
 * Chofu AEYC-0643XU-CH Warmtepomp + Atlantic Aurea Controlbox
 * 
 * v1.0 - FINAL PRODUCTION VERSION:
 * ✅ Gebalanceerde hysteresis: 0.1°C AAN / 0.2°C UIT
 * ✅ MQTT Logging (chofu/log/INFO, /WARNING, /ERROR)
 * ✅ Dynamische max (Anna setpoint + 0.5°C, max 25°C)
 * ✅ Remote Serial Monitor (logs in Home Assistant)
 * ✅ Stabiele basis (v3.9 proven code)
 * 
 * HYSTERESIS (bij Anna = 20.5°C):
 * 21.0°C ════════ Dynamische max (Anna + 0.5) ⛔
 * 20.7°C ════════ UIT trigger (0.2°C boven) ✋
 * 20.6°C          Binnen tolerantie
 * 20.5°C ──────── DOEL (Anna setpoint) ✅
 * 20.4°C ════════ AAN trigger (0.1°C onder) 🔥
 * 20.3°C          Te koud
 * 
 * VERWACHT GEDRAG:
 * ├─ Start bij 20.4°C
 * ├─ Bereikt 20.5-20.6°C
 * ├─ Doorschiet naar 20.7°C
 * ├─ Stopt
 * ├─ Koelt af (~2 uur)
 * └─ Herstart bij 20.4°C (2-3x per nacht)
 * 
 * BASE FEATURES (van v3.9):
 * ✅ Agressieve grens verlaagd: 1.5°C (was 2.0°C)
 * ✅ Agressieve correctie: ×30 (was ×25)
 * ✅ Normale correctie: ×20 (was ×15)
 * ✅ Minimum PID: 55% bij >1.5°C (was 60% bij >2.0°C)
 * ✅ Snellere response op kamer fout
 * 
 * v1.0 features:
 * ✅ Test commando's subscriben nu correct
 * ✅ Duidelijke waarschuwing als test commando in productie modus
 * 
 * v1.0 features:
 * ✅ Bij >2°C fout: Kamer correctie ×25 (was ×10)
 * ✅ Bij >2°C fout: Minimum PID 60% (stand 5+)
 * ✅ Bij >2°C fout: Hysteresis 30 sec (was 10 min)
 * ✅ Normale fout: Kamer correctie ×15 (was ×10)
 * ✅ Uitgebreide debug output met fout weergave
 */

#include <SoftwareSerial.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ═══════════════════════════════════════════════════════════════
//  CONFIGURATIE - PAS HIER AAN
// ═══════════════════════════════════════════════════════════════

// WiFi & MQTT
const char* SSID = "KromhoutWiFi";
const char* PASS = "Gijs2018!#";
const char* MQTT_BROKER = "192.168.1.69";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "mqtt";
const char* MQTT_PASS = "mariska4!#";

// Hardware pins
#define CHOFU_TX_PIN 2  // Naar warmtepomp (via T2 transistor)
#define CHOFU_RX_PIN 3  // Van warmtepomp (afluisteren)
#define USE_LCD true

// PID Parameters (TWEAKBAAR via web/MQTT!)
float Kp = 0.8;   // Proportional gain
float Ki = 0.01;  // Integral gain  
float Kd = 0.3;   // Derivative gain

// Hysteresis tijden (voorkomt te veel schakelen)
long HYST_SLOW_MS = 600000;  // 10 minuten (conservatief)
long HYST_FAST_MS = 120000;  // 2 minuten (agressief bij grote fout)

// Stooklijn parameters
float STOOKLIJN_GRENS = 5.0;   // Onder 5°C buiten
float STOOKLIJN_FACTOR = 0.5;  // +0.5°C setpoint per graad onder grens

// Vermogen per stand (Watt)
const int VERMOGEN[] = {0, 240, 420, 640, 850, 1050, 1250, 1450};

// ═══════════════════════════════════════════════════════════════
//  GLOBALE VARIABELEN
// ═══════════════════════════════════════════════════════════════

// Hardware
SoftwareSerial chofuSerial(CHOFU_RX_PIN, CHOFU_TX_PIN);
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiServer webServer(80);

// Warmtepomp data (gelezen van protocol)
float t_supply = 25.0, t_return = 20.0, t_outside = 5.0;
uint8_t comp_hz = 0;
uint8_t pomp_snelheid_wp = 0;  // Van warmtepomp gelezen
bool defrost = false;
float werkelijk_vermogen_w = 0;

// Kamertemperatuur (van Anna via MQTT)
float t_kamer = 21.0;
float t_kamer_gewenst = 21.5;

// Regelparameters
float setpoint = 40.0;  // Doel aanvoertemperatuur
float delta_t = 5.0;
uint8_t stand = 0;  // 0-7
bool wp_aan = false;
bool lcd_enabled = true;
String modus = "auto";  // "auto" of "handmatig"
uint8_t handmatig_stand = 1;

// PID variabelen
float pid_integraal = 0;
float pid_vorige_fout = 0;
float pid_output = 0;

// Protocol variabelen
uint8_t telegram_buffer[25];
uint8_t buffer_index = 0;
bool telegram_compleet = false;

// Timers
uint8_t discovery_fase = 0;
uint32_t vorige_discovery_ms = 0;
uint32_t vorige_data_ms = 0;
uint32_t vorige_lcd_ms = 0;
uint32_t vorige_pid_ms = 0;
uint32_t vorige_stand_wijz_ms = 0;
uint32_t vorige_telegram_ms = 0;
uint32_t vorige_web_check_ms = 0;

// ═══════════════════════════════════════════════════════════════
// MQTT LOGGING (v1.0) - Remote Serial Monitor
// ═══════════════════════════════════════════════════════════════

bool mqtt_logging_enabled = true;
uint32_t laatste_log_ms = 0;
const uint32_t LOG_THROTTLE_MS = 500;  // Max 1 log per 0.5 sec

void mqtt_log(String message, String level = "INFO"){
  Serial.println(message);  // Altijd naar Serial
  
  // Throttle (voorkom spam)
  uint32_t nu = millis();
  if(nu - laatste_log_ms < LOG_THROTTLE_MS && level != "ERROR"){
    return;
  }
  laatste_log_ms = nu;
  
  // Stuur naar MQTT
  if(mqtt_logging_enabled && mqttClient.connected()){
    String topic = "chofu/log/" + level;
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();
  }
}

// EEPROM adressen
#define EEPROM_MAGIC 0xAB
#define ADDR_MAGIC 0
#define ADDR_SETPOINT 1
#define ADDR_KP 5
#define ADDR_KI 9
#define ADDR_KD 13
#define ADDR_STOOKLIJN_GRENS 17
#define ADDR_STOOKLIJN_FACTOR 21

// ═══════════════════════════════════════════════════════════════
//  EEPROM FUNCTIES
// ═══════════════════════════════════════════════════════════════

void eeprom_init(){
  if(EEPROM.read(ADDR_MAGIC) != EEPROM_MAGIC){
    Serial.println("EEPROM: Eerste keer - schrijf defaults");
    eeprom_save();
  } else {
    Serial.println("EEPROM: Lees opgeslagen settings");
    eeprom_load();
  }
}

void eeprom_save(){
  EEPROM.write(ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.put(ADDR_SETPOINT, setpoint);
  EEPROM.put(ADDR_KP, Kp);
  EEPROM.put(ADDR_KI, Ki);
  EEPROM.put(ADDR_KD, Kd);
  EEPROM.put(ADDR_STOOKLIJN_GRENS, STOOKLIJN_GRENS);
  EEPROM.put(ADDR_STOOKLIJN_FACTOR, STOOKLIJN_FACTOR);
  Serial.println("EEPROM: Settings opgeslagen");
}

void eeprom_load(){
  EEPROM.get(ADDR_SETPOINT, setpoint);
  EEPROM.get(ADDR_KP, Kp);
  EEPROM.get(ADDR_KI, Ki);
  EEPROM.get(ADDR_KD, Kd);
  EEPROM.get(ADDR_STOOKLIJN_GRENS, STOOKLIJN_GRENS);
  EEPROM.get(ADDR_STOOKLIJN_FACTOR, STOOKLIJN_FACTOR);
  Serial.print("EEPROM: Geladen - Setpoint:");
  Serial.print(setpoint,1);
  Serial.print(" PID:");
  Serial.print(Kp,2);Serial.print("/");Serial.print(Ki,3);Serial.print("/");Serial.println(Kd,2);
}

// ═══════════════════════════════════════════════════════════════
//  WARMTEPOMP PROTOCOL (0x19/0x91 TELEGRAMS)
// ═══════════════════════════════════════════════════════════════

uint8_t bereken_checksum(uint8_t *buf, uint8_t len){
  uint16_t sum = 0;
  for(uint8_t i=0; i<len; i++){
    sum += buf[i];
  }
  return (sum & 0xFF);
}

void stuur_stand_telegram(){
  // Stuur 0x19 telegram naar warmtepomp
  uint8_t telegram[25] = {0};
  telegram[0] = 0x19;  // Header: controlbox -> warmtepomp
  telegram[1] = stand; // Stand 0-7
  telegram[2] = 0x00;  // Reserve
  
  // Voeg checksum toe
  telegram[23] = bereken_checksum(telegram, 23);
  telegram[24] = 0x00; // End marker
  
  // Verstuur via SoftwareSerial
  chofuSerial.write(telegram, 25);
  
  Serial.print("TX: Stand ");
  Serial.print(stand);
  Serial.println(" naar WP");
}

void verwerk_telegram_0x91(){
  // 0x91 = Warmtepomp -> Controlbox
  // Bevat: temperaturen, compressor Hz, pompsnelheid, etc.
  
  if(telegram_buffer[0] != 0x91) return;
  
  // Check checksum
  uint8_t calc_cs = bereken_checksum(telegram_buffer, 23);
  if(calc_cs != telegram_buffer[23]){
    Serial.println("RX: Checksum fout!");
    return;
  }
  
  // Parse warmtepomp data
  // Byte 3-4: Aanvoer temperatuur (signed 16-bit, /10)
  int16_t temp_raw = (telegram_buffer[3] << 8) | telegram_buffer[4];
  t_supply = temp_raw / 10.0;
  
  // Byte 5-6: Retour temperatuur
  temp_raw = (telegram_buffer[5] << 8) | telegram_buffer[6];
  t_return = temp_raw / 10.0;
  
  // Byte 7-8: Buiten temperatuur
  temp_raw = (telegram_buffer[7] << 8) | telegram_buffer[8];
  t_outside = temp_raw / 10.0;
  
  // Byte 9: Compressor Hz (0-120)
  comp_hz = telegram_buffer[9];
  
  // Byte 10: Pompsnelheid (0-100%)
  pomp_snelheid_wp = telegram_buffer[10];
  
  // Byte 11: Status bits
  defrost = (telegram_buffer[11] & 0x01);  // Bit 0 = defrost
  
  // Bereken werkelijk vermogen op basis van Hz
  if(comp_hz > 0){
    // Schatting: 240W @ 30Hz, lineair tot 1450W @ 120Hz
    werkelijk_vermogen_w = 240 + ((comp_hz - 30) / 90.0) * 1210;
    if(werkelijk_vermogen_w < 0) werkelijk_vermogen_w = 0;
    if(werkelijk_vermogen_w > 1450) werkelijk_vermogen_w = 1450;
  } else {
    werkelijk_vermogen_w = 0;
  }
  
  delta_t = t_supply - t_return;
  
  // Debug output (beknopt)
  static uint8_t debug_count = 0;
  if(debug_count++ % 10 == 0){  // Elke 10e telegram
    Serial.print("RX WP: A:");Serial.print(t_supply,1);
    Serial.print(" R:");Serial.print(t_return,1);
    Serial.print(" B:");Serial.print(t_outside,1);
    Serial.print(" Hz:");Serial.print(comp_hz);
    Serial.print(" P:");Serial.print(pomp_snelheid_wp);
    Serial.println("%");
  }
}

void lees_warmtepomp_data(){
  // Lees telegrams van warmtepomp via SoftwareSerial
  while(chofuSerial.available()){
    uint8_t byte = chofuSerial.read();
    
    // Start van telegram detecteren
    if(byte == 0x91 || byte == 0x19){
      buffer_index = 0;
      telegram_buffer[buffer_index++] = byte;
    }
    else if(buffer_index > 0 && buffer_index < 25){
      telegram_buffer[buffer_index++] = byte;
      
      // Telegram compleet?
      if(buffer_index == 25){
        if(telegram_buffer[0] == 0x91){
          verwerk_telegram_0x91();
        }
        buffer_index = 0;
        vorige_telegram_ms = millis();
      }
    }
  }
  
  // Stuur periodiek stand update (elke 5 sec)
  if(millis() - vorige_telegram_ms > 5000){
    stuur_stand_telegram();
    vorige_telegram_ms = millis();
  }
}

// ═══════════════════════════════════════════════════════════════
//  PID REGELING
// ═══════════════════════════════════════════════════════════════

void pas_pid_aan(){
  if(modus != "auto"){
    stand = handmatig_stand;
    wp_aan = (stand > 0);
    return;
  }
  
  uint32_t nu = millis();
  if(nu - vorige_pid_ms < 5000) return;  // Elke 5 sec
  vorige_pid_ms = nu;
  
  float kamer_fout = t_kamer_gewenst - t_kamer;
  
  // ═══════════════════════════════════════════════════════════
  // VORSTBEVEILIGING - ALTIJD EERST CHECKEN!
  // ═══════════════════════════════════════════════════════════
  if(t_outside < 5.0){
    if(stand == 0){
      stand = 1;
      wp_aan = true;
      vorige_stand_wijz_ms = nu;
      mqtt_log("❄️ VORSTBEVEILIGING! Buiten: " + String(t_outside,1) + "°C → Stand 1", "WARNING");
    }
    // Als vorst EN kamer koud, dan normale regeling
    // Maar minimaal stand 1!
  }
  
  // ═══════════════════════════════════════════════════════════
  // DYNAMISCHE MAXIMUM (v1.0) - Anna + 0.5°C
  // ═══════════════════════════════════════════════════════════
  
  float absolute_max = t_kamer_gewenst + 0.5;  // Anna + 0.5°C
  if(absolute_max > 25.0) absolute_max = 25.0;  // Hard limit veiligheid
  
  if(t_kamer > absolute_max){
    wp_aan = false;
    stand = 0;
    pid_integraal = 0;
    mqtt_log("⛔ MAX! Kamer: " + String(t_kamer,1) + "°C (max: " + String(absolute_max,1) + 
             "°C) Anna: " + String(t_kamer_gewenst,1) + "°C + 0.5", "ERROR");
    return;
  }
  
  // ═══════════════════════════════════════════════════════════
  // NORMALE REGELING (v1.0: 0.1°C AAN / 0.2°C UIT)
  // ═══════════════════════════════════════════════════════════
  
  if(kamer_fout > 0.1){  // AAN bij 20.4°C (was 0.2 → 20.3°C)
    // KAMER TE KOUD
    wp_aan = true;
    
    // Bereken doel setpoint met stooklijn
    float doel_setpoint = setpoint;
    if(t_outside < STOOKLIJN_GRENS){
      doel_setpoint += (STOOKLIJN_GRENS - t_outside) * STOOKLIJN_FACTOR;
      if(doel_setpoint > 45.0) doel_setpoint = 45.0;
    }
    
    float aanvoer_fout = doel_setpoint - t_supply;
    
    // Delta T correctie
    float dt_correctie = 0;
    if(delta_t < 4.0){
      dt_correctie = (delta_t - 5.0) * 3.0;  // Negatief
    } else if(delta_t > 6.0){
      dt_correctie = (delta_t - 5.0) * 2.0;  // Positief
    }
    
    // Kamer correctie - AGRESSIEVER!
    float kamer_correctie = 0;
    if(kamer_fout > 1.5){  // Was 2.0 - NU LAGER!
      // Grote fout: ZEER agressief
      kamer_correctie = kamer_fout * 30.0;  // Was 25.0 - NU STERKER!
      Serial.print("GROTE KAMER FOUT (");
      Serial.print(kamer_fout, 1);
      Serial.print("°C) → Extra agressief! Correctie:");
      Serial.println(kamer_correctie, 1);
    } else {
      // Normale correctie
      kamer_correctie = kamer_fout * 20.0;  // Was 15.0 - NU STERKER!
    }
    
    // PID berekening
    pid_integraal += aanvoer_fout * 0.005;
    if(pid_integraal > 50) pid_integraal = 50;
    if(pid_integraal < -50) pid_integraal = -50;
    
    float diff = (aanvoer_fout - pid_vorige_fout) / 0.005;
    pid_vorige_fout = aanvoer_fout;
    
    pid_output = Kp * aanvoer_fout + Ki * pid_integraal + Kd * diff;
    pid_output += dt_correctie + kamer_correctie;
    
    // Bij grote kamer fout: minimum PID output
    if(kamer_fout > 1.5 && pid_output < 55){  // Was 2.0 en 60
      pid_output = 55;  // Minimaal stand 4
      Serial.println("Minimum PID 55% geforceerd bij grote fout");
    }
    
    if(pid_output < 0) pid_output = 0;
    if(pid_output > 100) pid_output = 100;
    
    // Vertaal naar stand
    uint8_t nieuwe_stand = 0;
    if(pid_output < 5) nieuwe_stand = 0;
    else if(pid_output < 15) nieuwe_stand = 1;
    else if(pid_output < 25) nieuwe_stand = 2;
    else if(pid_output < 40) nieuwe_stand = 3;
    else if(pid_output < 55) nieuwe_stand = 4;
    else if(pid_output < 70) nieuwe_stand = 5;
    else if(pid_output < 85) nieuwe_stand = 6;
    else nieuwe_stand = 7;
    
    // VORSTBEVEILIGING: minimaal stand 1 bij vorst
    if(t_outside < 5.0 && nieuwe_stand == 0){
      nieuwe_stand = 1;
    }
    
    // Hysteresis - BIJ GROTE FOUT ZEER KORT!
    long hyst = HYST_SLOW_MS;  // 10 minuten default
    if(kamer_fout > 1.5){  // Was 2.0
      hyst = 30000;  // 30 seconden bij grote fout!
      Serial.println("Versnelde hysteresis: 30 sec");
    } else if(kamer_fout > 1.0){
      hyst = HYST_FAST_MS;  // 2 minuten
    }
    
    if(nieuwe_stand != stand && (nu - vorige_stand_wijz_ms >= hyst)){
      stand = nieuwe_stand;
      vorige_stand_wijz_ms = nu;
      Serial.print("AUTO: Kamer ");Serial.print(t_kamer,1);
      Serial.print("→");Serial.print(t_kamer_gewenst,1);
      Serial.print(" (fout:");Serial.print(kamer_fout,1);
      Serial.print("°C) DT:");Serial.print(delta_t,1);
      Serial.print(" St→");Serial.print(stand);
      Serial.print(" PID:");Serial.print(pid_output,0);
      Serial.println("%");
    }
    
  } else if(kamer_fout < -0.2){  // UIT bij 20.7°C (was -0.3 → 20.8°C)
    // KAMER TE WARM - Gebalanceerde hysteresis (0.3°C band totaal)
    // Maar bij vorst NOOIT onder stand 1!
    if(t_outside < 5.0){
      if(stand > 1){
        wp_aan = true;
        stand = 1;  // Minimum bij vorst
        Serial.print("Kamer warm maar VORST (");
        Serial.print(t_outside,1);
        Serial.println("°C) → Stand 1 minimum");
      }
    } else {
      wp_aan = false;
      stand = 0;
      pid_integraal = 0;
      mqtt_log("Kamer te warm (" + String(t_kamer,1) + "°C) → WP UIT", "INFO");
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  MQTT FUNCTIES
// ═══════════════════════════════════════════════════════════════

void mqtt_ontvang(int len){
  String topic = mqttClient.messageTopic();
  String payload = "";
  while(mqttClient.available()){
    payload += (char)mqttClient.read();
  }
  
  Serial.print("MQTT: ");Serial.print(topic);Serial.print("=");Serial.println(payload);
  
  if(topic == "chofu/cmd/lcd"){
    lcd_enabled = (payload == "1");
    if(lcd_enabled) lcd.backlight(); else { lcd.noBacklight(); lcd.clear(); }
  }
  else if(topic == "chofu/cmd/power"){
    modus = "handmatig";
    handmatig_stand = (payload == "1") ? 1 : 0;
  }
  else if(topic == "chofu/cmd/setpoint"){
    float val = payload.toFloat();
    if(val >= 20 && val <= 45){
      setpoint = val;
      eeprom_save();
    }
  }
  else if(topic == "chofu/cmd/kp"){
    Kp = payload.toFloat();
    eeprom_save();
  }
  else if(topic == "chofu/cmd/ki"){
    Ki = payload.toFloat();
    eeprom_save();
  }
  else if(topic == "chofu/cmd/kd"){
    Kd = payload.toFloat();
    eeprom_save();
  }
  else if(topic == "chofu/cmd/modus"){
    if(payload == "auto" || payload == "handmatig"){
      modus = payload;
      if(modus == "auto") pid_integraal = 0;
    }
  }
  else if(topic == "anna/setpoint"){
    float val = payload.toFloat();
    if(val >= 14 && val <= 30) t_kamer_gewenst = val;
  }
  else if(topic == "anna/temperatuur"){
    float val = payload.toFloat();
    if(val >= 5 && val <= 35) t_kamer = val;
  }
  else if(topic == "chofu/cmd/force_start"){
    // Reset hysteresis timer = forceer immediate start
    vorige_stand_wijz_ms = 0;
    Serial.println("⚡ FORCE START - Hysteresis timer gereset");
  }
  
  stuur_data();
}

void discovery_fase1(){
  Serial.println("Discovery F1");
  String dev = "\"device\":{\"identifiers\":[\"chofu_hp\"],\"name\":\"Chofu Warmtepomp\",\"manufacturer\":\"Chofu\",\"model\":\"AEYC\",\"sw_version\":\"3.9\"}";
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/supply/config");
  mqttClient.print("{\"name\":\"Chofu Aanvoer\",\"uniq_id\":\"chofu_hp_supply\",\"stat_t\":\"chofu/supply\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/return/config");
  mqttClient.print("{\"name\":\"Chofu Retour\",\"uniq_id\":\"chofu_hp_return\",\"stat_t\":\"chofu/return\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/power/config");
  mqttClient.print("{\"name\":\"Chofu Vermogen\",\"uniq_id\":\"chofu_hp_power\",\"stat_t\":\"chofu/vermogen\",\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/stage/config");
  mqttClient.print("{\"name\":\"Chofu Stand\",\"uniq_id\":\"chofu_hp_stage\",\"stat_t\":\"chofu/stand\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/outside/config");
  mqttClient.print("{\"name\":\"Chofu Buiten\",\"uniq_id\":\"chofu_hp_outside\",\"stat_t\":\"chofu/outside\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/switch/chofu_hp/power/config");
  mqttClient.print("{\"name\":\"Chofu Power\",\"uniq_id\":\"chofu_hp_sw\",\"cmd_t\":\"chofu/cmd/power\",\"stat_t\":\"chofu/aan\",\"pl_on\":\"1\",\"pl_off\":\"0\"," + dev + "}");
  mqttClient.endMessage();
  
  stuur_data();
}

void discovery_fase2(){
  Serial.println("Discovery F2");
  String dev = "\"device\":{\"identifiers\":[\"chofu_hp\"],\"name\":\"Chofu Warmtepomp\",\"manufacturer\":\"Chofu\",\"model\":\"AEYC\",\"sw_version\":\"3.9\"}";
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/delta_t/config");
  mqttClient.print("{\"name\":\"Chofu Delta T\",\"uniq_id\":\"chofu_hp_delta_t\",\"stat_t\":\"chofu/delta_t\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/kamer/config");
  mqttClient.print("{\"name\":\"Chofu Kamer\",\"uniq_id\":\"chofu_hp_kamer\",\"stat_t\":\"chofu/kamer\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/kamer_gewenst/config");
  mqttClient.print("{\"name\":\"Chofu Kamer Gewenst\",\"uniq_id\":\"chofu_hp_kamer_gewenst\",\"stat_t\":\"chofu/kamer_gewenst\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/setpoint/config");
  mqttClient.print("{\"name\":\"Chofu Setpoint\",\"uniq_id\":\"chofu_hp_setpoint\",\"stat_t\":\"chofu/setpoint\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/modus/config");
  mqttClient.print("{\"name\":\"Chofu Modus\",\"uniq_id\":\"chofu_hp_modus\",\"stat_t\":\"chofu/modus\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/switch/chofu_hp/lcd/config");
  mqttClient.print("{\"name\":\"Chofu LCD\",\"uniq_id\":\"chofu_hp_lcd\",\"cmd_t\":\"chofu/cmd/lcd\",\"stat_t\":\"chofu/lcd\",\"pl_on\":\"1\",\"pl_off\":\"0\"," + dev + "}");
  mqttClient.endMessage();
  
  stuur_data();
}

void discovery_fase3(){
  Serial.println("Discovery F3");
  String dev = "\"device\":{\"identifiers\":[\"chofu_hp\"],\"name\":\"Chofu Warmtepomp\",\"manufacturer\":\"Chofu\",\"model\":\"AEYC\",\"sw_version\":\"3.9\"}";
  
  mqttClient.beginMessage("homeassistant/binary_sensor/chofu_hp/defrost/config");
  mqttClient.print("{\"name\":\"Chofu Defrost\",\"uniq_id\":\"chofu_hp_defrost\",\"stat_t\":\"chofu/defrost\",\"pl_on\":\"1\",\"pl_off\":\"0\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/pid/config");
  mqttClient.print("{\"name\":\"Chofu PID\",\"uniq_id\":\"chofu_hp_pid\",\"stat_t\":\"chofu/pid\",\"unit_of_meas\":\"%\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/pomp/config");
  mqttClient.print("{\"name\":\"Chofu Pomp\",\"uniq_id\":\"chofu_hp_pomp\",\"stat_t\":\"chofu/pomp\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/sensor/chofu_hp/comp_hz/config");
  mqttClient.print("{\"name\":\"Chofu Compressor Hz\",\"uniq_id\":\"chofu_hp_comp_hz\",\"stat_t\":\"chofu/comp_hz\",\"unit_of_meas\":\"Hz\"," + dev + "}");
  mqttClient.endMessage();
  delay(2000);
  
  mqttClient.beginMessage("homeassistant/select/chofu_hp/modus_sel/config");
  mqttClient.print("{\"name\":\"Chofu Modus Select\",\"uniq_id\":\"chofu_hp_modus_sel\",\"cmd_t\":\"chofu/cmd/modus\",\"stat_t\":\"chofu/modus\",\"options\":[\"auto\",\"handmatig\"]," + dev + "}");
  mqttClient.endMessage();
  
  stuur_data();
}

void stuur_data(){
  delta_t = t_supply - t_return;
  int verm = (werkelijk_vermogen_w > 0) ? (int)werkelijk_vermogen_w : VERMOGEN[stand];
  
  mqttClient.beginMessage("chofu/supply");mqttClient.print(t_supply,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/return");mqttClient.print(t_return,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/vermogen");mqttClient.print(verm);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/stand");mqttClient.print(stand);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/outside");mqttClient.print(t_outside,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/aan");mqttClient.print(wp_aan?"1":"0");mqttClient.endMessage();
  mqttClient.beginMessage("chofu/delta_t");mqttClient.print(delta_t,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/kamer");mqttClient.print(t_kamer,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/kamer_gewenst");mqttClient.print(t_kamer_gewenst,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/setpoint");mqttClient.print(setpoint,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/modus");mqttClient.print(modus);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/lcd");mqttClient.print(lcd_enabled?"1":"0");mqttClient.endMessage();
  mqttClient.beginMessage("chofu/defrost");mqttClient.print(defrost?"1":"0");mqttClient.endMessage();
  mqttClient.beginMessage("chofu/pid");mqttClient.print(pid_output,1);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/pomp");mqttClient.print(pomp_snelheid_wp);mqttClient.endMessage();
  mqttClient.beginMessage("chofu/comp_hz");mqttClient.print(comp_hz);mqttClient.endMessage();
  
  vorige_data_ms = millis();
}

// ═══════════════════════════════════════════════════════════════
//  WEB INTERFACE
// ═══════════════════════════════════════════════════════════════

void handle_web_client(){
  if(millis() - vorige_web_check_ms < 100) return;
  vorige_web_check_ms = millis();
  
  WiFiClient client = webServer.available();
  if(!client) return;
  
  Serial.println("Web client verbonden");
  
  String request = "";
  while(client.connected()){
    if(client.available()){
      char c = client.read();
      request += c;
      if(c == '\n' && request.endsWith("\r\n\r\n")){
        break;
      }
    }
  }
  
  // Parse GET parameters
  if(request.indexOf("GET /?") >= 0){
    if(request.indexOf("setpoint=") >= 0){
      int idx = request.indexOf("setpoint=") + 9;
      String val_str = request.substring(idx, request.indexOf("&", idx));
      if(val_str.length() == 0) val_str = request.substring(idx, request.indexOf(" ", idx));
      setpoint = val_str.toFloat();
      eeprom_save();
    }
    if(request.indexOf("kp=") >= 0){
      int idx = request.indexOf("kp=") + 3;
      String val_str = request.substring(idx, request.indexOf("&", idx));
      if(val_str.length() == 0) val_str = request.substring(idx, request.indexOf(" ", idx));
      Kp = val_str.toFloat();
      eeprom_save();
    }
    if(request.indexOf("ki=") >= 0){
      int idx = request.indexOf("ki=") + 3;
      String val_str = request.substring(idx, request.indexOf("&", idx));
      if(val_str.length() == 0) val_str = request.substring(idx, request.indexOf(" ", idx));
      Ki = val_str.toFloat();
      eeprom_save();
    }
    if(request.indexOf("kd=") >= 0){
      int idx = request.indexOf("kd=") + 3;
      String val_str = request.substring(idx, request.indexOf("&", idx));
      if(val_str.length() == 0) val_str = request.substring(idx, request.indexOf(" ", idx));
      Kd = val_str.toFloat();
      eeprom_save();
    }
    if(request.indexOf("modus=") >= 0){
      int idx = request.indexOf("modus=") + 6;
      String val_str = request.substring(idx, request.indexOf("&", idx));
      if(val_str.length() == 0) val_str = request.substring(idx, request.indexOf(" ", idx));
      modus = val_str;
    }
  }
  
  // Stuur HTML response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  
  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  client.println("<title>Kromhout WP v3.9</title>");
  client.println("<style>");
  client.println("body{font-family:Arial;margin:20px;background:#f0f0f0}");
  client.println("h1{color:#2c3e50}");
  client.println(".card{background:white;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}");
  client.println(".temp{font-size:24px;font-weight:bold;color:#e74c3c}");
  client.println("input,select{padding:8px;margin:5px;border:1px solid #ccc;border-radius:4px}");
  client.println("button{background:#3498db;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer}");
  client.println("button:hover{background:#2980b9}");
  client.println(".status{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:5px}");
  client.println(".on{background:#27ae60}");
  client.println(".off{background:#95a5a6}");
  client.println("</style></head><body>");
  
  client.println("<h1>🔥 Kromhout Warmtepomp v3.9</h1>");
  
  // Status card
  client.println("<div class='card'>");
  client.println("<h2>Status</h2>");
  client.print("<div><span class='status ");
  client.print(wp_aan ? "on" : "off");
  client.print("'></span>Warmtepomp: <b>");
  client.print(wp_aan ? "AAN" : "UIT");
  client.println("</b></div>");
  client.print("<div>Modus: <b>");
  client.print(modus);
  client.println("</b></div>");
  client.print("<div>Stand: <b>");
  client.print(stand);
  client.print("</b> (");
  client.print(werkelijk_vermogen_w > 0 ? (int)werkelijk_vermogen_w : VERMOGEN[stand]);
  client.println(" W)</div>");
  client.println("</div>");
  
  // Temperaturen card
  client.println("<div class='card'>");
  client.println("<h2>Temperaturen</h2>");
  client.print("<div>Aanvoer: <span class='temp'>");
  client.print(t_supply, 1);
  client.println("°C</span></div>");
  client.print("<div>Retour: <span class='temp'>");
  client.print(t_return, 1);
  client.println("°C</span></div>");
  client.print("<div>Delta T: <span class='temp'>");
  client.print(delta_t, 1);
  client.println("°C</span></div>");
  client.print("<div>Buiten: <span class='temp'>");
  client.print(t_outside, 1);
  client.println("°C</span></div>");
  client.print("<div>Kamer: <span class='temp'>");
  client.print(t_kamer, 1);
  client.print("°C</span> → ");
  client.print(t_kamer_gewenst, 1);
  client.println("°C</div>");
  client.println("</div>");
  
  // Compressor info
  client.println("<div class='card'>");
  client.println("<h2>Compressor</h2>");
  client.print("<div>Frequentie: <b>");
  client.print(comp_hz);
  client.println(" Hz</b></div>");
  client.print("<div>Pompsnelheid: <b>");
  client.print(pomp_snelheid_wp);
  client.println("%</b></div>");
  client.print("<div>Defrost: <b>");
  client.print(defrost ? "Actief" : "Uit");
  client.println("</b></div>");
  client.println("</div>");
  
  // Settings card
  client.println("<div class='card'>");
  client.println("<h2>Instellingen</h2>");
  client.println("<form>");
  
  client.print("<div>Setpoint: <input type='number' name='setpoint' value='");
  client.print(setpoint, 1);
  client.println("' step='0.5' min='20' max='45'> °C</div>");
  
  client.print("<div>Modus: <select name='modus'>");
  client.print("<option value='auto'");
  if(modus == "auto") client.print(" selected");
  client.print(">Auto</option>");
  client.print("<option value='handmatig'");
  if(modus == "handmatig") client.print(" selected");
  client.println(">Handmatig</option></select></div>");
  
  client.println("<h3>PID Parameters</h3>");
  
  client.print("<div>Kp: <input type='number' name='kp' value='");
  client.print(Kp, 2);
  client.println("' step='0.1' min='0' max='10'></div>");
  
  client.print("<div>Ki: <input type='number' name='ki' value='");
  client.print(Ki, 3);
  client.println("' step='0.001' min='0' max='1'></div>");
  
  client.print("<div>Kd: <input type='number' name='kd' value='");
  client.print(Kd, 2);
  client.println("' step='0.1' min='0' max='10'></div>");
  
  client.print("<div>PID Output: <b>");
  client.print(pid_output, 1);
  client.println("%</b></div>");
  
  client.println("<br><button type='submit'>💾 Opslaan</button>");
  client.println("</form>");
  client.println("</div>");
  
  client.print("<div class='card'><small>IP: ");
  client.print(WiFi.localIP());
  client.print(" | Uptime: ");
  client.print(millis() / 1000 / 60);
  client.println(" min</small></div>");
  
  client.println("<script>setTimeout(function(){location.reload()},10000);</script>");
  client.println("</body></html>");
  
  delay(10);
  client.stop();
}

// ═══════════════════════════════════════════════════════════════
//  LCD FUNCTIES
// ═══════════════════════════════════════════════════════════════

void update_lcd(){
  if(!USE_LCD || !lcd_enabled) return;
  
  uint32_t nu = millis();
  if(nu - vorige_lcd_ms < 3000) return;
  vorige_lcd_ms = nu;
  
  static uint8_t scherm = 0;
  lcd.clear();
  
  int verm = (werkelijk_vermogen_w > 0) ? (int)werkelijk_vermogen_w : VERMOGEN[stand];
  
  switch(scherm){
    case 0:
      lcd.print("St");lcd.print(stand);
      lcd.print(" ");lcd.print(verm);lcd.print("W");
      lcd.print(wp_aan?" ON":" OFF");
      lcd.setCursor(0,1);
      lcd.print(modus=="auto"?"AUTO":"HAND");
      lcd.print(" Hz:");lcd.print(comp_hz);
      break;
      
    case 1:
      lcd.print("A:");lcd.print(t_supply,1);
      lcd.print(" R:");lcd.print(t_return,1);
      lcd.setCursor(0,1);
      lcd.print("DT:");lcd.print(delta_t,1);
      lcd.print(" Set:");lcd.print(setpoint,0);
      break;
      
    case 2:
      lcd.print("Kamer:");lcd.print(t_kamer,1);lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Doel:");lcd.print(t_kamer_gewenst,1);
      lcd.print(" B:");lcd.print(t_outside,1);
      break;
      
    case 3:
      lcd.print("PID:");lcd.print(pid_output,0);lcd.print("% ");
      lcd.print("P:");lcd.print(pomp_snelheid_wp);
      lcd.setCursor(0,1);
      lcd.print(WiFi.localIP());
      break;
  }
  
  scherm = (scherm + 1) % 4;
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════

void setup(){
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n╔═══════════════════════════════════════════════╗");
  Serial.println("║  Kromhout WP v1.0 - TEST PROTOCOL       ║");
  Serial.println("║  Met protocol, web interface & MQTT          ║");
  Serial.println("╚═══════════════════════════════════════════════╝");
  
  // EEPROM
  eeprom_init();
  
  // SoftwareSerial naar warmtepomp
  chofuSerial.begin(9600);
  Serial.println("Chofu serial OK");
  
  // LCD
  if(USE_LCD){
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print("Kromhout WP");
    lcd.setCursor(0,1);
    lcd.print("v1.0 TEST");
    delay(2000);
  }
  
  // WiFi
  lcd.clear();
  lcd.print("WiFi...");
  WiFi.begin(SSID, PASS);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20){
    Serial.print(".");
    delay(500);
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED){
    while(WiFi.localIP() == IPAddress(0,0,0,0) && attempts < 30){
      delay(1000);
      attempts++;
    }
    Serial.print("WiFi OK! IP: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.print("WiFi OK!");
    lcd.setCursor(0,1);
    lcd.print(WiFi.localIP());
    delay(2000);
  }
  
  // Web server
  webServer.begin();
  Serial.print("Web server: http://");
  Serial.println(WiFi.localIP());
  
  // MQTT
  lcd.clear();
  lcd.print("MQTT...");
  mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
  mqttClient.onMessage(mqtt_ontvang);
  
  if(mqttClient.connect(MQTT_BROKER, MQTT_PORT)){
    Serial.println("MQTT OK!");
    lcd.setCursor(0,1);
    lcd.print("OK!");
    delay(1000);
    
    mqttClient.subscribe("chofu/cmd/#");
    mqttClient.subscribe("anna/setpoint");
    mqttClient.subscribe("anna/temperatuur");
    Serial.println("MQTT subscribed");
    
    mqttClient.beginMessage("chofu/status");
    mqttClient.print("online");
    mqttClient.endMessage();
    
    delay(1000);
    
    lcd.clear();
    lcd.print("Discovery...");
    discovery_fase1();
    vorige_discovery_ms = millis();
    discovery_fase = 1;
  }
  
  Serial.println("╔═══════════════════════════════════════════════╗");
  Serial.println("║           SYSTEEM OPERATIONEEL!              ║");
  Serial.println("╚═══════════════════════════════════════════════╝");
  Serial.print("Web: http://");
  Serial.println(WiFi.localIP());
  Serial.println("MQTT: actief");
  Serial.println("Protocol: luisteren...");
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════

void loop(){
  // MQTT poll
  mqttClient.poll();
  
  // Lees warmtepomp data (0x91 telegrams)
  lees_warmtepomp_data();
  
  // PID regeling (elke 5 sec)
  pas_pid_aan();
  
  // LCD update (elke 3 sec)
  update_lcd();
  
  // MQTT data update (elke 10 sec)
  if(millis() - vorige_data_ms > 10000){
    stuur_data();
  }
  
  // Discovery fases
  if(discovery_fase == 1 && millis() - vorige_discovery_ms > 30000){
    discovery_fase2();
    vorige_discovery_ms = millis();
    discovery_fase = 2;
  }
  
  if(discovery_fase == 2 && millis() - vorige_discovery_ms > 30000){
    discovery_fase3();
    discovery_fase = 3;
  }
  
  // Web client handling
  handle_web_client();
  
  delay(10);
}
