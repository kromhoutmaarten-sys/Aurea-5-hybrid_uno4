# 📡 MQTT Referentie - Kromhout Warmtepomp Controller

Complete overzicht van alle MQTT topics en commando's.

---

## 📋 Topic Structuur

**Prefix:** Instelbaar via setup (default: `kromhout_wp`)

**State Topics:** `sensor/[prefix]_[naam]`  
**Command Topics:** `[prefix]/cmd/[commando]`  
**Log Topics:** `[prefix]/log/[level]`

---

## 📊 State Topics (Arduino → Home Assistant)

### Temperaturen

| Topic | Type | Unit | Update | Beschrijving |
|-------|------|------|--------|--------------|
| `sensor/kromhout_wp_aanvoer` | float | °C | 10s | Aanvoer temperatuur |
| `sensor/kromhout_wp_retour` | float | °C | 10s | Retour temperatuur |
| `sensor/kromhout_wp_kamer` | float | °C | 10s | Kamer temperatuur (van Anna) |
| `sensor/kromhout_wp_setpoint` | float | °C | 10s | Aanvoer setpoint |
| `sensor/kromhout_wp_buiten` | float | °C | 10s | Buiten temperatuur |
| `sensor/kromhout_wp_delta_t` | float | °C | 10s | Delta T (aanvoer - retour) |

**Voorbeeld:**
```json
Topic: sensor/kromhout_wp_aanvoer
Payload: "35.2"
```

### Status & Controle

| Topic | Type | Values | Update | Beschrijving |
|-------|------|--------|--------|--------------|
| `sensor/kromhout_wp_stand` | int | 0-7 | 10s | Huidige compressor stand |
| `sensor/kromhout_wp_vermogen` | int | W | 10s | Geschat vermogen |
| `sensor/kromhout_wp_pid` | int | 0-100 | 10s | PID output (%) |
| `sensor/kromhout_wp_modus` | string | auto/handmatig | 10s | Regelingsmodus |
| `kromhout_wp/aan` | string | 0/1 | 10s | Warmtepomp aan/uit |

**Stand Mapping:**
```
0 = UIT       (0W)
1 = Minimum   (240W)
2 = Laag      (420W)
3 = Medium-   (640W)
4 = Medium    (850W)
5 = Medium+   (1050W)
6 = Hoog      (1250W)
7 = Maximum   (1450W)
```

### Hardware Info

| Topic | Type | Values | Update | Beschrijving |
|-------|------|--------|--------|--------------|
| `sensor/kromhout_wp_compressor_hz` | int | 0-120 | 10s | Compressor frequentie |
| `sensor/kromhout_wp_pomp` | int | 0-100 | 10s | Pomp snelheid (%) |
| `kromhout_wp/defrost` | string | 0/1 | 10s | Defrost actief |
| `kromhout_wp/lcd` | string | 0/1 | 10s | LCD backlight status |

---

## 🎮 Command Topics (Home Assistant → Arduino)

### Basis Controle

#### Power (Handmatig Aan/Uit)
```
Topic: kromhout_wp/cmd/power
Payload: "1" = AAN (handmatige stand 1)
Payload: "0" = UIT (handmatige stand 0)

Effect:
- Schakelt naar handmatige modus
- Forceert warmtepomp aan of uit
- PID regeling gepauzeerd
```

**Home Assistant Switch:**
```yaml
switch:
  - platform: mqtt
    name: "Warmtepomp Power"
    command_topic: "kromhout_wp/cmd/power"
    state_topic: "kromhout_wp/aan"
    payload_on: "1"
    payload_off: "0"
```

#### Modus
```
Topic: kromhout_wp/cmd/modus
Payload: "auto" = Automatische PID regeling
Payload: "handmatig" = Handmatige controle

Effect:
- auto: PID neemt controle over
- handmatig: Gebruik stand commando's
```

**Home Assistant Select:**
```yaml
select:
  - platform: mqtt
    name: "WP Modus"
    command_topic: "kromhout_wp/cmd/modus"
    state_topic: "sensor/kromhout_wp_modus"
    options:
      - "auto"
      - "handmatig"
```

### PID Parameters

#### Setpoint (Aanvoer Temperatuur)
```
Topic: kromhout_wp/cmd/setpoint
Payload: "20.0" - "45.0" (float in °C)

Effect:
- Wijzigt doel aanvoer temperatuur
- Opgeslagen in EEPROM (blijft na herstart)
- PID regelt naar deze waarde

Default: 40.0°C
```

**Home Assistant Number:**
```yaml
number:
  - platform: mqtt
    name: "WP Setpoint"
    command_topic: "kromhout_wp/cmd/setpoint"
    state_topic: "sensor/kromhout_wp_setpoint"
    min: 20
    max: 45
    step: 0.5
    unit_of_measurement: "°C"
```

#### Kp (Proportional)
```
Topic: kromhout_wp/cmd/kp
Payload: "0.1" - "5.0" (float)

Effect:
- Wijzigt Proportional gain
- Grotere waarde = snellere reactie
- Te groot = overshoot

Default: 0.6
```

#### Ki (Integral)
```
Topic: kromhout_wp/cmd/ki
Payload: "0.001" - "0.1" (float)

Effect:
- Wijzigt Integral gain
- Elimineert steady-state error
- Te groot = oscillatie

Default: 0.008
```

#### Kd (Derivative)
```
Topic: kromhout_wp/cmd/kd
Payload: "0.0" - "2.0" (float)

Effect:
- Wijzigt Derivative gain
- Dempt snelle veranderingen
- Te groot = nerveus gedrag

Default: 0.4
```

**Home Assistant PID Tuning:**
```yaml
number:
  - platform: mqtt
    name: "PID Kp"
    command_topic: "kromhout_wp/cmd/kp"
    min: 0.1
    max: 5.0
    step: 0.1
    
  - platform: mqtt
    name: "PID Ki"
    command_topic: "kromhout_wp/cmd/ki"
    min: 0.001
    max: 0.1
    step: 0.001
    
  - platform: mqtt
    name: "PID Kd"
    command_topic: "kromhout_wp/cmd/kd"
    min: 0.0
    max: 2.0
    step: 0.1
```

### Stooklijn Parameters

#### Stooklijn Grens
```
Topic: kromhout_wp/cmd/stooklijn_grens
Payload: "0.0" - "15.0" (float in °C)

Effect:
- Onder deze buiten temp wordt setpoint verhoogd
- Compensatie voor kou

Default: 5.0°C
```

#### Stooklijn Factor
```
Topic: kromhout_wp/cmd/stooklijn_factor
Payload: "0.0" - "2.0" (float)

Effect:
- °C setpoint verhoging per °C onder grens
- Grotere waarde = meer compensatie

Default: 0.5 (= +0.5°C per graad onder 5°C)

Voorbeeld:
Buiten = 0°C, Grens = 5°C, Factor = 0.5
→ Setpoint += (5 - 0) × 0.5 = +2.5°C
```

### Advanced Controle

#### Force Start
```
Topic: kromhout_wp/cmd/force_start
Payload: "1"

Effect:
- Reset hysteresis timer
- Warmtepomp start DIRECT (skip 10 min wachttijd)
- Gebruik bij urgente warmtevraag

Eenmalig: Geen blijvende status
```

**Home Assistant Button:**
```yaml
button:
  - platform: mqtt
    name: "WP Force Start"
    command_topic: "kromhout_wp/cmd/force_start"
    payload_press: "1"
```

#### LCD Backlight
```
Topic: kromhout_wp/cmd/lcd
Payload: "1" = Backlight AAN
Payload: "0" = Backlight UIT

Effect:
- Schakelt LCD verlichting
- Spaart energie (klein beetje)
```

#### Reset Setup
```
Topic: kromhout_wp/cmd/reset_setup
Payload: "1"

Effect:
- Wist WiFi/MQTT configuratie
- Arduino herstart in setup mode
- Gebruik voor herconfiguratie

WAARSCHUWING: Verliest alle instellingen!
```

---

## 📝 Log Topics (Arduino → Home Assistant)

### Levels

| Topic | Level | Gebruik |
|-------|-------|---------|
| `kromhout_wp/log/INFO` | Info | Normale events |
| `kromhout_wp/log/WARNING` | Warning | Waarschuwingen |
| `kromhout_wp/log/ERROR` | Error | Fouten |
| `kromhout_wp/log/DEBUG` | Debug | Debug info |

### Event Voorbeelden

**INFO:**
```
"WP START - Kamer: 20.4°C → 20.5°C"
"Kamer te warm (20.8°C) → WP UIT"
```

**WARNING:**
```
"❄️ VORSTBEVEILIGING! Buiten: 3.2°C → Stand 1 geforceerd"
"⚠️ EQUILIBRIUM! Kamer hangt op 20.4°C Stand: 2→3"
```

**ERROR:**
```
"⛔ MAX! Kamer: 21.2°C (max: 21.0°C) GEFORCEERD UIT!"
"❌ RX: Checksum fout in protocol telegram!"
```

**Home Assistant Automation:**
```yaml
automation:
  - alias: "WP Error Notification"
    trigger:
      - platform: mqtt
        topic: "kromhout_wp/log/ERROR"
    action:
      - service: notify.mobile_app
        data:
          title: "Warmtepomp Error"
          message: "{{ trigger.payload }}"
```

---

## 🔄 Anna Thermostaat Topics

### Input (Anna → Arduino)

#### Setpoint
```
Topic: anna/setpoint
Payload: "14.0" - "30.0" (float in °C)

Effect:
- Stelt gewenste kamer temperatuur in
- Arduino regelt om dit te bereiken
- Basis voor PID beslissingen
```

#### Kamer Temperatuur
```
Topic: anna/temperatuur
Payload: "15.0" - "30.0" (float in °C)

Effect:
- Werkelijke kamer temperatuur
- Gebruikt voor fout berekening
- Update elke 30-60 seconden (van Anna)
```

**Home Assistant Automation (Anna → MQTT):**
```yaml
automation:
  - alias: "Anna Setpoint to MQTT"
    trigger:
      - platform: state
        entity_id: climate.anna
    action:
      - service: mqtt.publish
        data:
          topic: "anna/setpoint"
          payload: "{{ state_attr('climate.anna', 'temperature') }}"
          
  - alias: "Anna Temperature to MQTT"
    trigger:
      - platform: state
        entity_id: sensor.anna_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "anna/temperatuur"
          payload: "{{ states('sensor.anna_temperature') }}"
```

---

## 📊 MQTT Explorer Overzicht

**Structuur in MQTT Explorer:**
```
├── sensor/
│   ├── kromhout_wp_aanvoer        (35.2)
│   ├── kromhout_wp_retour         (30.1)
│   ├── kromhout_wp_kamer          (20.5)
│   ├── kromhout_wp_setpoint       (40.0)
│   ├── kromhout_wp_buiten         (8.5)
│   ├── kromhout_wp_delta_t        (5.1)
│   ├── kromhout_wp_stand          (2)
│   ├── kromhout_wp_vermogen       (420)
│   ├── kromhout_wp_pid            (45)
│   ├── kromhout_wp_modus          (auto)
│   ├── kromhout_wp_compressor_hz  (45)
│   └── kromhout_wp_pomp           (60)
│
├── kromhout_wp/
│   ├── aan                        (1)
│   ├── defrost                    (0)
│   ├── lcd                        (1)
│   ├── cmd/
│   │   ├── power
│   │   ├── modus
│   │   ├── setpoint
│   │   ├── kp
│   │   ├── ki
│   │   ├── kd
│   │   ├── force_start
│   │   └── reset_setup
│   └── log/
│       ├── INFO                   (laatste event)
│       ├── WARNING                (laatste warning)
│       ├── ERROR                  (laatste error)
│       └── DEBUG                  (laatste debug)
│
└── anna/
    ├── setpoint                   (20.5)
    └── temperatuur                (20.3)
```

---

## 🧪 Testing Met Mosquitto CLI

### Publish (Versturen)

```bash
# Setpoint wijzigen
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/setpoint" -m "42.0"

# Warmtepomp aan
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/power" -m "1"

# Modus naar auto
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/modus" -m "auto"

# Force start
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/force_start" -m "1"

# PID parameters
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/kp" -m "0.8"
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/ki" -m "0.01"
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/kd" -m "0.5"

# Anna setpoint simuleren
mosquitto_pub -h 192.168.1.69 -t "anna/setpoint" -m "21.0"
mosquitto_pub -h 192.168.1.69 -t "anna/temperatuur" -m "20.5"
```

### Subscribe (Ontvangen)

```bash
# Alle topics
mosquitto_sub -h 192.168.1.69 -t "#" -v

# Alleen warmtepomp
mosquitto_sub -h 192.168.1.69 -t "sensor/kromhout_wp_#" -v
mosquitto_sub -h 192.168.1.69 -t "kromhout_wp/#" -v

# Alleen logs
mosquitto_sub -h 192.168.1.69 -t "kromhout_wp/log/#" -v

# Specifieke sensor
mosquitto_sub -h 192.168.1.69 -t "sensor/kromhout_wp_kamer" -v
```

---

## 🔐 Authenticatie

**Met username/password:**
```bash
mosquitto_pub -h 192.168.1.69 -u mqtt -P [wachtwoord] -t "kromhout_wp/cmd/power" -m "1"

mosquitto_sub -h 192.168.1.69 -u mqtt -P [wachtwoord] -t "#" -v
```

**Arduino configuratie:**
```cpp
// In WiFi Setup Portal ingevuld:
MQTT User: mqtt
MQTT Pass: [jouw-wachtwoord]
```

---

## 📈 Update Frequentie

| Data Type | Interval | Reden |
|-----------|----------|-------|
| Temperaturen | 10s | Real-time monitoring |
| Stand/Vermogen | 10s | Status tracking |
| PID Output | 10s | Debug info |
| Compressor Hz | 10s | Hardware monitoring |
| Logs | Event-based | Bij gebeurtenis |
| Anna Setpoint | 2-3 min | Anna update rate |

---

## 🐛 Troubleshooting

### Geen Data Ontvangen

**Check:**
```bash
# Test MQTT broker
mosquitto_sub -h 192.168.1.69 -t "#" -v

Als leeg:
→ MQTT broker draait niet
→ Firewall blokkeert poort 1883
→ Verkeerd IP adres
```

### Commando's Werken Niet

**Check:**
```bash
# Test versturen
mosquitto_pub -h 192.168.1.69 -t "kromhout_wp/cmd/power" -m "1"

# Monitor Serial output Arduino
# Moet zien: "MQTT: kromhout_wp/cmd/power=1"

Als niks:
→ Arduino niet verbonden met MQTT
→ Topics niet gesubscribed
→ Verkeerde topic prefix
```

### "Onbekend" in Home Assistant

**Oorzaken:**
- Discovery nog niet compleet (wacht 3 min)
- MQTT integratie niet actief
- Topics komen niet aan

**Oplossing:**
```
1. Check MQTT Explorer: zie je topics?
2. Developer Tools → MQTT: listen to sensor/#
3. Reset Arduino (discovery opnieuw)
4. Herstart Home Assistant
```

---

## 📚 Zie Ook

- [INSTALLATION.md](INSTALLATION.md) - Setup guide
- [HOME_ASSISTANT.md](HOME_ASSISTANT.md) - HA configuratie
- [PID_TUNING.md](PID_TUNING.md) - Parameter optimalisatie
