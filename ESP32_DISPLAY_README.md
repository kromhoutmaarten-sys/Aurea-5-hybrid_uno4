# 📺 ESP32 E-Ink Display voor Kromhout Warmtepomp

**Standalone remote monitor** - Plaats overal in huis!

Ondersteunt **2.9" en 4.2"** Waveshare displays!

![E-Ink Display](../images/eink-display.jpg)

---

## 🎯 Features

- **2.9" of 4.2" E-Ink Display** - Kies wat bij jou past!
- **Ultra Laag Verbruik** - ~0.01W in slaapstand, ~5W tijdens update
- **WiFi Setup Portal** - Geen code aanpassen nodig
- **Live Data** - Real-time kamer temp, aanvoer, stand, vermogen
- **24u Grafiek** - Visuele temperatuur geschiedenis
- **Auto Refresh** - Elke 5 minuten automatische update
- **Instelbare Locatie** - "Woonkamer", "Gang", "Slaapkamer", etc.
- **MQTT Integratie** - Leest data van warmtepomp controller

---

## 🛠️ Hardware Lijst

### Vereist (Kies een display)

#### **Optie A: 2.9" Display (Compact)**
- **ESP32 Development Board** (~€8)
- **Waveshare 2.9" E-Ink Display** (~€15)
  - Model: 2.9inch e-Paper Module
  - Resolutie: 296x128 pixels
  - Voordeel: Compact, goedkoop
  - Nadeel: Minder ruimte voor data
- **Totaal: ~€23**

#### **Optie B: 4.2" Display (Groot)**
- **ESP32 Development Board** (~€8)
- **Waveshare 4.2" E-Ink Display** (~€25)
  - Model: 4.2inch e-Paper Module
  - Resolutie: 400x300 pixels
  - Voordeel: Veel ruimte, betere leesbaarheid
  - Nadeel: Duurder, groter
- **Totaal: ~€33**

### Beide Versies
- **Jumper Wires** (8x female-female)
- **MicroUSB Kabel** - 5V voeding
- **Optioneel: 3D Printed behuizing**

---

## ⚙️ Display Type Selecteren

### In Code (BELANGRIJK!)

Open `esp32_eink_display.ino` en kies je display:

```cpp
// ═══════════════════════════════════════════════════════════════
// DISPLAY TYPE SELECTIE - PAS DIT AAN!
// ═══════════════════════════════════════════════════════════════

// Uncomment je display type:
#define DISPLAY_29   // Waveshare 2.9" (296x128) ← Voor jouw 2.9"
//#define DISPLAY_42   // Waveshare 4.2" (400x300)
```

**Voor 2.9" display:** Zorg dat `DISPLAY_29` niet gecomment is  
**Voor 4.2" display:** Comment `DISPLAY_29` en uncomment `DISPLAY_42`

---

## 🔌 Hardware Aansluiten

### E-Ink Display → ESP32

```
E-Ink Pin    ESP32 Pin
─────────    ─────────
VCC      →   3.3V
GND      →   GND
DIN      →   GPIO 23 (MOSI)
CLK      →   GPIO 18 (SCK)
CS       →   GPIO 5
DC       →   GPIO 17
RST      →   GPIO 16
BUSY     →   GPIO 4
```

### Schema
```
             ┌─────────────┐
             │   ESP32     │
             │             │
        3.3V │●           │
         GND │●           │
    MOSI(23) │●           │
     SCK(18) │●           │ GPIO 5  → CS
             │            │ GPIO 17 → DC
             │            │ GPIO 16 → RST
             │            │ GPIO 4  → BUSY
             │            │
             └─────────────┘
                   ║
              MicroUSB (5V)
```

---

## 💾 Software Installeren

### 1. Arduino IDE Setup

```bash
1. Download Arduino IDE: https://www.arduino.cc/
2. Installeer ESP32 board:
   - File → Preferences
   - Additional Boards Manager URLs:
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   - Tools → Board → Boards Manager
   - Zoek "ESP32" → Install
3. Select board:
   - Tools → Board → ESP32 Arduino → ESP32 Dev Module
```

### 2. Bibliotheken Installeren

```bash
Sketch → Include Library → Manage Libraries

Installeer:
✓ PubSubClient (by Nick O'Leary)
✓ GxEPD2 (by Jean-Marc Zingg)
✓ Adafruit GFX Library
```

### 3. Code Uploaden

```bash
1. Open esp32_eink_display.ino
2. Tools → Port → [Selecteer COM port]
3. Tools → Upload Speed → 115200
4. Sketch → Upload
5. Wacht op "Done uploading"
```

---

## 🚀 Eerste Setup

### Stap 1: Upload & Start
```
1. Upload code naar ESP32
2. Display toont: "Setup Mode"
3. Instructies verschijnen op scherm
```

### Stap 2: WiFi Configuratie
```
1. Zoek WiFi netwerk: "WarmtePomp-Display-Setup"
2. Verbind met wachtwoord: "display123"
3. Browser opent automatisch (of surf naar 192.168.4.1)
```

### Stap 3: Instellingen Invullen

```html
╔══════════════════════════════════════╗
║   📺 Display Setup                  ║
╠══════════════════════════════════════╣
║  Display Locatie                    ║
║  [Woonkamer               ]         ║
║                                      ║
║  WiFi Netwerk (SSID)                ║
║  [KromhoutWiFi            ]         ║
║                                      ║
║  WiFi Wachtwoord                    ║
║  [••••••••••••            ]         ║
║                                      ║
║  MQTT Server IP                     ║
║  [192.168.1.69            ]         ║
║                                      ║
║  MQTT Poort                         ║
║  [1883                    ]         ║
║                                      ║
║  MQTT Gebruiker                     ║
║  [mqtt                    ]         ║
║                                      ║
║  MQTT Wachtwoord                    ║
║  [••••••••••••            ]         ║
║                                      ║
║  Warmtepomp Prefix                  ║
║  [kromhout_wp             ]         ║
║                                      ║
║  [💾 Opslaan en Starten]            ║
╚══════════════════════════════════════╝
```

**Belangrijk:** Gebruik dezelfde MQTT prefix als de warmtepomp controller!

### Stap 4: Klaar!
```
Display herstart automatisch
Verbindt met WiFi
Verbindt met MQTT
Toont live data! ✓
```

---

## 📊 Display Layouts

### 2.9" Display (296x128) - Compact
```
╔════════════════════════════════════════╗
║ Woonkamer                           ● ║
╠════════════════════════════════════════╣
║ Kamer: 20.8°C    Aanvoer: 35.2°C    ║
║ Stand: 2         Vermogen: 420 W     ║
║ ┌────────────────────────────────┐   ║
║ │22°╱╲                           │   ║
║ │  ╱  ╲                          │   ║
║ │18°                         24u │   ║
║ └────────────────────────────────┘   ║
╚════════════════════════════════════════╝
```

### 4.2" Display (400x300) - Ruim
```
╔════════════════════════════════════════════╗
║  Woonkamer                              ● ║
╠════════════════════════════════════════════╣
║                                            ║
║  Kamer: 20.8°C                            ║
║                                            ║
║  Aanvoer: 35.2°C        Stand: 2          ║
║  Retour:  30.1°C        Vermogen:         ║
║  Buiten:   8.5°C        420 W             ║
║                                            ║
║  ┌────────────────────────────────────┐   ║
║  │ 22° ╱╲                             │   ║
║  │    ╱  ╲╱╲                          │   ║
║  │   ╱      ╲╱                        │   ║
║  │ 18°                            24u │   ║
║  └────────────────────────────────────┘   ║
╚════════════════════════════════════════════╝
```

**● = Warmtepomp status** (gevuld = AAN, open = UIT)

---

## ⚙️ Instellingen

### Display Locatie
Geeft aan waar het scherm hangt:
- "Woonkamer"
- "Gang"  
- "Slaapkamer"
- "Keuken"
- etc.

Wordt getoond bovenaan display.

### MQTT Prefix
**BELANGRIJK:** Moet exact overeenkomen met warmtepomp controller!

**Voorbeelden:**
```
Warmtepomp heet: "Kromhout WP"
→ Prefix: "kromhout_wp"

Warmtepomp heet: "Woonkamer WP"  
→ Prefix: "woonkamer_wp"
```

Display leest data van topics zoals:
- `sensor/[prefix]_kamer`
- `sensor/[prefix]_aanvoer`
- `[prefix]/aan`

---

## 🔋 Stroomverbruik

### Normaal Gebruik
```
Update (5 sec):     ~1000 mW
Slaapstand (4m55s): ~10 mW
Gemiddeld:          ~50 mW (0.05W)

Per dag: ~1.2 Wh
Per jaar: ~440 Wh (~€0.15 aan stroom)
```

### Battery Powered (Optioneel)
Met 3x 18650 batterijen (3.7V, 3000mAh):
```
Capaciteit: ~33 Wh
Runtime: ~27 dagen op batterij!
```

**LiPo/Li-ion Setup:**
- 3x 18650 in serie (11.1V)
- Step-down naar 5V (voor ESP32)
- USB batterij pack werkt ook!

---

## 🐛 Troubleshooting

### Display blijft wit/zwart
```
✓ Check: Jumper wires correct aangesloten?
✓ Check: E-Ink display model = 4.2" (400x300)?
✓ Check: GxEPD2_420 in code (niet 213 of 290)?
✓ Test: Upload voorbeeld sketch van GxEPD2 library
```

### Display toont geen setup instructies
```
✓ Check: Code correct geüpload?
✓ Check: Serial Monitor (115200): "DISPLAY SETUP MODE"?
✓ Fix: Reset ESP32 (knop op board)
✓ Fix: Upload code opnieuw
```

### Kan niet verbinden met "WarmtePomp-Display-Setup"
```
✓ Check: ESP32 in setup mode? (eerste keer opstarten)
✓ Check: Wachtwoord = "display123"
✓ Check: 2.4GHz WiFi (geen 5GHz)
✓ Fix: Reset ESP32, probeer opnieuw
```

### Display toont geen live data
```
✓ Check: WiFi verbonden? (Serial Monitor)
✓ Check: MQTT broker bereikbaar?
✓ Check: MQTT prefix correct ingevuld?
✓ Check: Warmtepomp controller draait?
✓ Test: MQTT Explorer - zie je topics?
```

### Display update te langzaam
```
✓ Change: REFRESH_INTERVAL = 300000 (5 min default)
✓ Sneller: REFRESH_INTERVAL = 60000 (1 min)
✓ Let op: Vaker updaten = meer stroomverbruik
✓ E-Ink displays zijn NIET bedoeld voor real-time updates
```

---

## 🎨 3D Behuizing

### STL Bestanden (coming soon)
```
enclosure_front.stl    - Voorkant met display opening
enclosure_back.stl     - Achterkant met USB opening
wall_mount.stl         - Wandbevestiging
stand.stl              - Tafelstandaard (optioneel)
```

### Print Settings
```
Material:        PLA / PETG
Layer Height:    0.2mm
Infill:          20%
Supports:        Ja (voor USB opening)
Print Time:      ~8 uur totaal
```

### Montage
1. Print alle delen
2. Plaats ESP32 + E-Ink in front
3. Route USB kabel door opening
4. Schroef back vast (4x M3 schroef)
5. Monteer wall_mount aan muur
6. Klik behuizing op mount

---

## 🔄 Reset naar Setup Mode

### Methode 1: Via Serial Monitor
```
1. Open Serial Monitor (115200 baud)
2. Type: RESET
3. ESP32 herstart in setup mode
```

### Methode 2: EEPROM Wissen
```cpp
// Voeg toe aan setup():
EEPROM.write(ADDR_SETUP_DONE, 0x00);
EEPROM.commit();

Upload → ESP32 herstart in setup mode
```

### Methode 3: Code Wijzigen
```cpp
// In setup(), verander:
setup_done = check_setup_done();

// Naar:
setup_done = false;  // Force setup mode

Upload → ESP32 gaat in setup mode
```

---

## ⚡ Deep Sleep Mode (Optioneel)

Voor **ultra laag verbruik** op batterij:

```cpp
// Toevoegen aan einde van loop():
if(millis() - last_refresh > REFRESH_INTERVAL){
  draw_display();
  
  // Sleep voor 5 minuten
  esp_sleep_enable_timer_wakeup(300 * 1000000);  // 300 sec
  esp_deep_sleep_start();
}
```

**Voordeel:** ~0.01W in slaap (was ~0.01W)  
**Nadeel:** Geen MQTT verbinding tijdens slaap

**Ideaal voor:** Battery powered displays

---

## 📊 Customization

### Display Rotation
```cpp
// In draw_display():
display.setRotation(1);  // Landscape (default)

// Of:
display.setRotation(0);  // Portrait
display.setRotation(2);  // Landscape 180°
display.setRotation(3);  // Portrait 180°
```

### Refresh Interval
```cpp
const uint32_t REFRESH_INTERVAL = 300000;  // 5 min (default)

// Sneller:
const uint32_t REFRESH_INTERVAL = 60000;   // 1 min

// Langzamer (batterij):
const uint32_t REFRESH_INTERVAL = 600000;  // 10 min
```

### Display Fonts
```cpp
// Beschikbare fonts:
&FreeSans9pt7b   // Klein
&FreeSans12pt7b  // Medium (gebruikt voor data)
&FreeSans18pt7b  // Groot (gebruikt voor titel en kamer temp)
&FreeSans24pt7b  // Extra groot
```

---

## 🎯 Tips & Tricks

### Optimale Plaatsing
```
✓ Binnen WiFi bereik (signaal >-70 dBm)
✓ Niet in direct zonlicht (vervaagt E-Ink)
✓ Op ooghoogte voor makkelijk aflezen
✓ Bij USB stopcontact (of gebruik battery)
```

### E-Ink Display Care
```
✓ Update niet vaker dan om de minuut (slijtage)
✓ Vermijd static images >24u (ghosting)
✓ Periodiek "full refresh" voorkomt ghosting
✓ Temperatuur 0-40°C (display spec)
```

### WiFi Signaal Verbeteren
```
✓ Plaats dichter bij router
✓ Gebruik WiFi repeater
✓ Check 2.4GHz kanaal (1, 6, 11 beste keuze)
✓ Externe antenne op ESP32 (sommige boards)
```

---

## 📝 Code Aanpassingen

### Meer Grafiek Historie
```cpp
// Wijzig array size (regel ~70):
float temp_history[48];  // Was 24, nu 48u

// Pas grafiek aan (regel ~295):
for(int i = 0; i < 47; i++){  // Was 23
  // ... grafiek code
}

// Update interval (regel ~320):
if(millis() - last_history_update > 1800000){  // 30 min (was 1u)
```

### Extra Data Tonen
```cpp
// Voeg variabelen toe (regel ~73):
float t_setpoint = 0.0;
String modus = "auto";

// Subscribe MQTT (regel ~138):
mqttClient.subscribe(("sensor/" + prefix + "_setpoint").c_str());
mqttClient.subscribe(("sensor/" + prefix + "_modus").c_str());

// Parse in callback (regel ~112):
else if(topic_str == "sensor/" + prefix + "_setpoint") t_setpoint = val;
else if(topic_str == "sensor/" + prefix + "_modus") modus = payload_str;

// Toon op display (in draw_display):
display.print("Setpoint: ");
display.print(t_setpoint, 1);
```

---

## 🆘 Support

**Issues?** Check:
1. [Troubleshooting](#-troubleshooting) sectie hierboven
2. [GitHub Issues](https://github.com/kromhout/warmtepomp-controller/issues)
3. Serial Monitor output (115200 baud)
4. MQTT Explorer (zie welke topics beschikbaar zijn)

---

## 📜 Licentie

MIT License - Vrij te gebruiken en aan te passen!

---

**Veel plezier met je E-Ink display!** 📺✨
