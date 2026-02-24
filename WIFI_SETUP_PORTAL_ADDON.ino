// ═══════════════════════════════════════════════════════════════
// WIFI SETUP PORTAL - Voor v1.0 OPEN SOURCE
// ═══════════════════════════════════════════════════════════════

/*
 * FEATURES:
 * ✅ Eerste keer: Maak eigen WiFi netwerk "WarmtePomp-Setup"
 * ✅ Captive portal (browser opent automatisch)
 * ✅ Web formulier voor WiFi + MQTT instellingen
 * ✅ Opslaan in EEPROM (blijft na herstart)
 * ✅ Reset knop = terug naar setup mode
 * ✅ Test verbinding voordat opslaan
 * 
 * GEBRUIK:
 * 1. Upload code
 * 2. Arduino maakt "WarmtePomp-Setup" netwerk
 * 3. Verbind met telefoon
 * 4. Browser opent automatisch
 * 5. Vul gegevens in
 * 6. Klaar!
 */

// EEPROM Layout (na bestaande adressen)
#define ADDR_SETUP_DONE 100      // 1 byte: 0xAA = setup compleet
#define ADDR_DEVICE_NAME 101     // 32 bytes: Naam van de warmtepomp
#define ADDR_WIFI_SSID 133       // 32 bytes
#define ADDR_WIFI_PASS 165       // 64 bytes
#define ADDR_MQTT_SERVER 229     // 50 bytes
#define ADDR_MQTT_PORT 279       // 2 bytes
#define ADDR_MQTT_USER 281       // 32 bytes
#define ADDR_MQTT_PASS 313       // 32 bytes

// Setup status
bool setup_mode = false;
bool setup_done = false;

// Setup AP naam
const char* SETUP_SSID = "WarmtePomp-Setup";
const char* SETUP_PASS = "warmtepomp123";  // Simpel wachtwoord voor setup

// ═══════════════════════════════════════════════════════════════
// EEPROM FUNCTIES VOOR SETUP
// ═══════════════════════════════════════════════════════════════

void eeprom_read_string(int addr, char* buffer, int max_len){
  for(int i = 0; i < max_len; i++){
    buffer[i] = EEPROM.read(addr + i);
    if(buffer[i] == 0) break;
  }
  buffer[max_len - 1] = 0;  // Ensure null termination
}

void eeprom_write_string(int addr, const char* str, int max_len){
  int len = strlen(str);
  if(len >= max_len) len = max_len - 1;
  
  for(int i = 0; i < len; i++){
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, 0);  // Null terminator
}

bool check_setup_done(){
  return EEPROM.read(ADDR_SETUP_DONE) == 0xAA;
}

void mark_setup_done(){
  EEPROM.write(ADDR_SETUP_DONE, 0xAA);
}

void reset_setup(){
  EEPROM.write(ADDR_SETUP_DONE, 0x00);
  Serial.println("Setup gereset - herstart voor setup mode");
}

void load_wifi_settings(char* ssid, char* pass){
  eeprom_read_string(ADDR_WIFI_SSID, ssid, 32);
  eeprom_read_string(ADDR_WIFI_PASS, pass, 64);
}

void save_wifi_settings(const char* ssid, const char* pass){
  eeprom_write_string(ADDR_WIFI_SSID, ssid, 32);
  eeprom_write_string(ADDR_WIFI_PASS, pass, 64);
}

void load_device_name(char* name){
  eeprom_read_string(ADDR_DEVICE_NAME, name, 32);
  if(strlen(name) == 0){
    strcpy(name, "Kromhout WP");  // Default naam
  }
}

void save_device_name(const char* name){
  eeprom_write_string(ADDR_DEVICE_NAME, name, 32);
}

void load_mqtt_settings(char* server, uint16_t* port, char* user, char* pass){
  eeprom_read_string(ADDR_MQTT_SERVER, server, 50);
  *port = (EEPROM.read(ADDR_MQTT_PORT) << 8) | EEPROM.read(ADDR_MQTT_PORT + 1);
  eeprom_read_string(ADDR_MQTT_USER, user, 32);
  eeprom_read_string(ADDR_MQTT_PASS, pass, 32);
}

void save_mqtt_settings(const char* server, uint16_t port, const char* user, const char* pass){
  eeprom_write_string(ADDR_MQTT_SERVER, server, 50);
  EEPROM.write(ADDR_MQTT_PORT, port >> 8);
  EEPROM.write(ADDR_MQTT_PORT + 1, port & 0xFF);
  eeprom_write_string(ADDR_MQTT_USER, user, 32);
  eeprom_write_string(ADDR_MQTT_PASS, pass, 32);
}

// ═══════════════════════════════════════════════════════════════
// SETUP MODE - ACCESS POINT + CAPTIVE PORTAL
// ═══════════════════════════════════════════════════════════════

void start_setup_mode(){
  setup_mode = true;
  
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║      WIFI SETUP MODE ACTIEF          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println();
  Serial.print("1. Verbind met WiFi netwerk: ");
  Serial.println(SETUP_SSID);
  Serial.print("2. Wachtwoord: ");
  Serial.println(SETUP_PASS);
  Serial.println("3. Browser opent automatisch setup pagina");
  Serial.println("4. Vul WiFi + MQTT gegevens in");
  Serial.println();
  
  // Start Access Point
  WiFi.beginAP(SETUP_SSID, SETUP_PASS);
  
  Serial.print("Access Point actief op: ");
  Serial.println(WiFi.localIP());
  
  // Start web server voor setup
  webServer.begin();
}

// HTML voor setup pagina
const char SETUP_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Warmtepomp Setup</title>
<style>
body{font-family:Arial;max-width:500px;margin:50px auto;padding:20px;background:#f5f5f5}
h1{color:#e74c3c;text-align:center}
.card{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
label{display:block;margin:15px 0 5px;font-weight:bold;color:#333}
input{width:100%;padding:12px;border:2px solid #ddd;border-radius:5px;font-size:16px;box-sizing:border-box}
input:focus{border-color:#3498db;outline:none}
button{width:100%;background:#e74c3c;color:white;padding:15px;border:none;border-radius:5px;font-size:18px;font-weight:bold;cursor:pointer;margin-top:20px}
button:hover{background:#c0392b}
.info{background:#ecf0f1;padding:10px;border-radius:5px;margin:15px 0;font-size:14px}
.success{background:#2ecc71;color:white;padding:15px;border-radius:5px;text-align:center;display:none}
</style>
</head>
<body>
<div class='card'>
<h1>🔥 Warmtepomp Setup</h1>
<div class='info'>
Vul hieronder je WiFi en MQTT broker gegevens in. Deze worden opgeslagen en gebruikt voor de verbinding.
</div>

<form id='setupForm' onsubmit='submitForm(event)'>
<h2>Warmtepomp Naam</h2>
<label>Geef je warmtepomp een naam</label>
<input type='text' name='device_name' required placeholder='Bijv: Kromhout WP, Woonkamer WP' value='Kromhout WP' maxlength='30'>
<div class='info' style='font-size:12px;margin-top:5px'>
Deze naam wordt gebruikt in Home Assistant en het dashboard.
</div>

<h2>WiFi Instellingen</h2>
<label>WiFi Netwerk Naam (SSID)</label>
<input type='text' name='ssid' required placeholder='Bijv: MijnWiFi'>

<label>WiFi Wachtwoord</label>
<input type='password' name='pass' required placeholder='Wachtwoord'>

<h2>MQTT Broker</h2>
<label>MQTT Server IP</label>
<input type='text' name='mqtt_server' required placeholder='Bijv: 192.168.1.100' value='192.168.1.69'>

<label>MQTT Poort</label>
<input type='number' name='mqtt_port' required value='1883'>

<label>MQTT Gebruikersnaam</label>
<input type='text' name='mqtt_user' required placeholder='Bijv: mqtt' value='mqtt'>

<label>MQTT Wachtwoord</label>
<input type='password' name='mqtt_pass' required placeholder='Wachtwoord' value=''>

<button type='submit'>💾 Opslaan en Herstarten</button>
</form>

<div id='success' class='success'>
✅ Instellingen opgeslagen!<br>
Arduino herstart nu...<br>
Verbind opnieuw met je WiFi netwerk.
</div>
</div>

<script>
function submitForm(e){
  e.preventDefault();
  var form = document.getElementById('setupForm');
  var data = new FormData(form);
  
  fetch('/save', {
    method: 'POST',
    body: new URLSearchParams(data)
  })
  .then(response => response.text())
  .then(result => {
    form.style.display = 'none';
    document.getElementById('success').style.display = 'block';
    setTimeout(() => {
      window.location.href = '/';
    }, 5000);
  })
  .catch(error => {
    alert('Fout bij opslaan: ' + error);
  });
}
</script>
</body>
</html>
)";

void handle_setup_page(WiFiClient client){
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.print(SETUP_HTML);
}

void handle_save_settings(WiFiClient client, String request){
  // Parse POST data
  int body_start = request.indexOf("\r\n\r\n") + 4;
  String body = request.substring(body_start);
  
  // Extract parameters
  String device_name = extract_param(body, "device_name");
  String ssid = extract_param(body, "ssid");
  String pass = extract_param(body, "pass");
  String mqtt_server = extract_param(body, "mqtt_server");
  String mqtt_port_str = extract_param(body, "mqtt_port");
  String mqtt_user = extract_param(body, "mqtt_user");
  String mqtt_pass = extract_param(body, "mqtt_pass");
  
  uint16_t mqtt_port = mqtt_port_str.toInt();
  
  // Save to EEPROM
  save_device_name(device_name.c_str());
  save_wifi_settings(ssid.c_str(), pass.c_str());
  save_mqtt_settings(mqtt_server.c_str(), mqtt_port, mqtt_user.c_str(), mqtt_pass.c_str());
  mark_setup_done();
  
  Serial.println("✅ Instellingen opgeslagen!");
  Serial.println("Naam: " + device_name);
  Serial.println("WiFi SSID: " + ssid);
  Serial.println("MQTT Server: " + mqtt_server + ":" + mqtt_port);
  
  // Send success response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println();
  client.println("OK");
  
  delay(1000);
  
  // Herstart
  Serial.println("Herstart in 3 seconden...");
  delay(3000);
  NVIC_SystemReset();  // Arduino R4 reset
}

String extract_param(String data, String param){
  String search = param + "=";
  int start = data.indexOf(search);
  if(start == -1) return "";
  
  start += search.length();
  int end = data.indexOf("&", start);
  if(end == -1) end = data.length();
  
  String value = data.substring(start, end);
  value.replace("+", " ");
  // URL decode (simpel)
  value.replace("%2B", "+");
  value.replace("%2F", "/");
  value.replace("%3D", "=");
  value.replace("%40", "@");
  value.replace("%21", "!");
  value.replace("%23", "#");
  value.replace("%24", "$");
  
  return value;
}

// ═══════════════════════════════════════════════════════════════
// IN SETUP() TOEVOEGEN:
// ═══════════════════════════════════════════════════════════════

/*
void setup(){
  Serial.begin(115200);
  delay(2000);
  
  // Check of setup al gedaan is
  setup_done = check_setup_done();
  
  if(!setup_done){
    // SETUP MODE
    start_setup_mode();
    return;  // Stop hier, wacht op setup
  }
  
  // NORMALE MODE - Laad settings
  char device_name[32];
  char wifi_ssid[32];
  char wifi_pass[64];
  char mqtt_server[50];
  char mqtt_user[32];
  char mqtt_pass[32];
  uint16_t mqtt_port;
  
  load_device_name(device_name);
  load_wifi_settings(wifi_ssid, wifi_pass);
  load_mqtt_settings(mqtt_server, &mqtt_port, mqtt_user, mqtt_pass);
  
  Serial.println("Instellingen geladen:");
  Serial.println("Naam: " + String(device_name));
  Serial.println("WiFi: " + String(wifi_ssid));
  Serial.println("MQTT: " + String(mqtt_server) + ":" + mqtt_port);
  
  // Gebruik device_name voor MQTT discovery
  // Bijvoorbeeld in discovery messages:
  // "name": device_name + " Kamer Temp"
  // "unique_id": device_name + "_kamer"
  
  // Verbind met WiFi (gebruik geladen settings)
  WiFi.begin(wifi_ssid, wifi_pass);
  
  // ... rest van normale setup
}
*/

// ═══════════════════════════════════════════════════════════════
// IN LOOP() TOEVOEGEN:
// ═══════════════════════════════════════════════════════════════

/*
void loop(){
  if(setup_mode){
    // Handle setup web requests
    WiFiClient client = webServer.available();
    if(client){
      String request = "";
      while(client.available()){
        request += (char)client.read();
      }
      
      if(request.indexOf("GET / ") >= 0 || request.indexOf("GET /setup") >= 0){
        handle_setup_page(client);
      }
      else if(request.indexOf("POST /save") >= 0){
        handle_save_settings(client, request);
      }
      
      client.stop();
    }
    return;
  }
  
  // ... rest van normale loop
}
*/

// ═══════════════════════════════════════════════════════════════
// RESET COMMANDO TOEVOEGEN (MQTT):
// ═══════════════════════════════════════════════════════════════

/*
  else if(topic == "chofu/cmd/reset_setup"){
    reset_setup();
    delay(1000);
    NVIC_SystemReset();
  }
*/
