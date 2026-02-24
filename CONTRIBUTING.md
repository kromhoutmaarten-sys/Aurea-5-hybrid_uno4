# 🤝 Contributing to Kromhout Warmtepomp Controller

Dank voor je interesse in bijdragen! **Elk beetje helpt** - van bug reports tot complete features.

---

## 🎯 Manieren om bij te dragen

### 1. 🐛 Bug Reports
Gevonden een bug? Help ons verbeteren!

**Open een Issue met:**
- Duidelijke beschrijving van het probleem
- Stappen om te reproduceren
- Verwacht vs. Werkelijk gedrag  
- Arduino/ESP32 model
- Software versie (v1.0, etc.)
- Serial Monitor output (indien relevant)
- MQTT topics/payloads (indien relevant)

**Template:**
```markdown
**Bug Beschrijving**
Korte duidelijke beschrijving

**Stappen om te reproduceren**
1. Ga naar...
2. Klik op...
3. Zie fout...

**Verwacht Gedrag**
Wat had moeten gebeuren

**Screenshots/Logs**
Serial Monitor output, screenshots, etc.

**Systeem Info**
- Hardware: Arduino UNO R4 WiFi / ESP32
- Versie: v1.0 CLEAN
- MQTT Broker: Mosquitto 2.0.15
- Home Assistant: 2024.1.0
```

---

### 2. 💡 Feature Requests
Idee voor nieuwe functionaliteit?

**Open een Issue met:**
- Duidelijke beschrijving van feature
- Waarom is dit nuttig?
- Hoe zou het moeten werken?
- Voorbeelden van gebruik

**Stem op bestaande requests** met 👍 op Issues!

---

### 3. 📖 Documentatie
Documentatie verbeteren helpt iedereen!

**Mogelijkheden:**
- Typo's fixen
- Duidelijkere uitleg
- Extra voorbeelden toevoegen
- Nieuwe guides schrijven
- Vertalingen (Engels, Duits, Frans, etc.)

**Submit Pull Request** met aanpassingen.

---

### 4. 🔧 Code Bijdragen

#### **Voordat je begint:**
1. Check bestaande Issues voor duplicaten
2. Open een Issue om je plan te bespreken
3. Wacht op feedback voordat je begint
4. Fork de repository
5. Maak een feature branch

#### **Coding Standards:**

**Style Guide:**
```cpp
// ✅ GOED: Duidelijke namen
float calculateTemperatureError(float setpoint, float current){
  return setpoint - current;
}

// ❌ FOUT: Cryptische namen  
float calc(float s, float c){
  return s - c;
}

// ✅ GOED: Comments boven complexe logica
// Check hysteresis timer to prevent rapid cycling
if(nu - vorige_stand_wijz_ms >= hyst){
  stand = nieuwe_stand;
}

// ✅ GOED: Constanten in CAPS
const uint32_t REFRESH_INTERVAL = 300000;

// ✅ GOED: Indentatie = 2 spaties
void mijn_functie(){
  if(conditie){
    doe_iets();
  }
}
```

**Commit Messages:**
```bash
# ✅ GOED: Duidelijk en specifiek
git commit -m "Fix: MQTT reconnect bij WiFi drop"
git commit -m "Add: COP calculation feature"
git commit -m "Docs: Update wiring diagram for ESP32"

# ❌ FOUT: Vaag
git commit -m "fixes"
git commit -m "update"
git commit -m "changes"
```

**Pull Request Checklist:**
- [ ] Code compileert zonder errors
- [ ] Getest op echte hardware
- [ ] Documentatie bijgewerkt (indien nodig)
- [ ] Geen breaking changes (of duidelijk aangegeven)
- [ ] Commit messages zijn duidelijk
- [ ] Code volgt bestaande style

---

## 🔨 Development Setup

### Lokaal Testen

**1. Clone Repository:**
```bash
git clone https://github.com/kromhout/warmtepomp-controller.git
cd warmtepomp-controller
```

**2. Maak Feature Branch:**
```bash
git checkout -b feature/mijn-nieuwe-feature
```

**3. Test je wijzigingen:**
```bash
# Arduino IDE:
File → Open → kromhout_wp_v1_0_CLEAN.ino
Sketch → Verify/Compile
```

**4. Commit & Push:**
```bash
git add .
git commit -m "Add: Duidelijke beschrijving"
git push origin feature/mijn-nieuwe-feature
```

**5. Open Pull Request:**
- Ga naar GitHub repository
- Click "New Pull Request"
- Selecteer je branch
- Vul beschrijving in
- Submit!

---

## 🧪 Testing Richtlijnen

### Minimale Tests
**Voor elke Pull Request:**
- ✅ Code compileert
- ✅ Upload naar hardware
- ✅ Basis functionaliteit werkt
- ✅ Geen Serial Monitor errors
- ✅ MQTT connectie stabiel

### Uitgebreide Tests
**Voor Major Features:**
- ✅ Test 24+ uur uptime
- ✅ Test WiFi reconnect
- ✅ Test MQTT reconnect  
- ✅ Test diverse scenario's
- ✅ Test edge cases
- ✅ Memory leak check

### Test Scenarios
```cpp
// Scenario 1: Normale werking
✓ Start → Connect → Run 1 uur → Check logs

// Scenario 2: WiFi drop
✓ Start → Disconnect WiFi → Wait 2 min → Reconnect → Check recovery

// Scenario 3: MQTT broker restart
✓ Start → Stop broker → Wait 1 min → Start broker → Check reconnect

// Scenario 4: Temperatuur change
✓ Start → Change Anna setpoint → Check WP response → Verify hysteresis

// Scenario 5: Reset
✓ Start → Send reset_setup → Verify setup mode → Reconfigure → Check functionality
```

---

## 📋 Priority Features

**High Priority (Gewenst!):**
- [ ] COP (Coefficient of Performance) berekening
- [ ] Energy dashboard in Home Assistant
- [ ] Multi-language support (Engels, Duits)
- [ ] OTA (Over The Air) updates
- [ ] Backup/restore configuratie

**Medium Priority:**
- [ ] Weersverwachting integratie
- [ ] Scheduler (ECO mode 's nachts)
- [ ] Voice control (Alexa/Google)
- [ ] Solar panel integratie
- [ ] Multi-zone support

**Low Priority (Nice to have):**
- [ ] Mobile app (native)
- [ ] Machine learning temp voorspelling
- [ ] Thermal mass learning
- [ ] Load shifting obv stroomprijs

**Zie ook:** [GitHub Issues](https://github.com/kromhout/warmtepomp-controller/issues)

---

## 🎨 Design Philosophy

### Principes:
1. **Stabiliteit First** - Productie systeem moet 24/7 draaien
2. **KISS** - Keep It Simple, Stupid
3. **User Friendly** - Setup moet makkelijk zijn
4. **Documentatie** - Elke feature goed uitgelegd
5. **Backwards Compatible** - Geen breaking changes zonder goede reden

### Geen Scope Creep:
```
✅ DOEN: Features die 80% van gebruikers helpen
❌ NIET: Niche features voor 1 specifieke use case

✅ DOEN: Optimalisaties met meetbare voordelen
❌ NIET: Premature optimalisatie

✅ DOEN: Clear error messages
❌ NIET: Cryptische debug codes
```

---

## 🐛 Bug Fix Workflow

### 1. Reproduceer Bug
```
- Follow exact steps in bug report
- Verify bug exists
- Note any additional info
```

### 2. Debug
```
- Add Serial.println() debug statements
- Check MQTT Explorer for topics
- Monitor memory usage
- Check for timing issues
```

### 3. Fix
```
- Make minimal changes
- Test thoroughly
- Verify fix doesn't break other features
```

### 4. Document
```
- Update code comments
- Add to CHANGELOG.md
- Close GitHub Issue with reference
```

---

## 📝 Documentation Updates

### README.md
- Feature lists
- Quick start guide
- High-level overview

### Docs Folder
- `INSTALLATION.md` - Stap-voor-stap installatie
- `WIRING.md` - Hardware schemas
- `MQTT_REFERENCE.md` - Alle topics
- `TROUBLESHOOTING.md` - Veel voorkomende problemen
- `PID_TUNING.md` - PID optimalisatie

### Code Comments
```cpp
// ✅ GOED: Uitleg WHY, niet WHAT
// Reset hysteresis timer to prevent 10 minute wait
// when user manually changes setpoint
vorige_stand_wijz_ms = 0;

// ❌ FOUT: Obvious WHAT
// Set variable to zero
vorige_stand_wijz_ms = 0;
```

---

## 🔐 Security

### Gevoelige Data
```
❌ NOOIT committen:
- WiFi wachtwoorden
- MQTT credentials
- API keys
- IP adressen (gebruik voorbeelden)

✅ Gebruik placeholders:
const char* WIFI_SSID = "JouwWiFi";
const char* MQTT_SERVER = "192.168.1.XX";
```

### Code Review
Alle Pull Requests worden gereviewd op:
- Code kwaliteit
- Security issues
- Breaking changes
- Performance impact

---

## 📊 Changelog

Bij elke release update `CHANGELOG.md`:

```markdown
## [1.1.0] - 2025-02-XX

### Added
- COP calculation feature
- Energy dashboard

### Fixed
- MQTT reconnect bij WiFi drop
- Memory leak in display update

### Changed
- Improved PID tuning defaults
- Updated documentation

### Removed
- Deprecated test mode commands
```

---

## 🎓 Learning Resources

**Nieuw bij Arduino/ESP32?**
- [Arduino Basics](https://www.arduino.cc/en/Tutorial/HomePage)
- [ESP32 Guide](https://randomnerdtutorials.com/getting-started-with-esp32/)
- [MQTT Basics](https://www.hivemq.com/mqtt-essentials/)

**Git/GitHub nieuw?**
- [GitHub Hello World](https://guides.github.com/activities/hello-world/)
- [Git Basics](https://git-scm.com/book/en/v2/Getting-Started-Git-Basics)

---

## ❓ Vragen?

**Voor discussie:**
- Open een [GitHub Discussion](https://github.com/kromhout/warmtepomp-controller/discussions)
- Niet zeker of iets een bug/feature is? → Discussion

**Voor bugs/features:**
- Open een [GitHub Issue](https://github.com/kromhout/warmtepomp-controller/issues)
- Duidelijk gedefinieerd probleem → Issue

---

## 🙏 Dank!

Elk bijdrage wordt **enorm gewaardeerd**! Van typo fixes tot major features - allemaal helpen ze het project vooruit.

**Top Contributors worden vermeld in README.md!** ⭐

---

## 📜 Licentie

Door bij te dragen ga je akkoord dat je bijdragen onder de [MIT License](LICENSE) vallen.

---

<div align="center">

**Happy Coding!** 💻✨

*Samen maken we dit project beter!*

</div>
