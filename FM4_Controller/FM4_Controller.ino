// ============================================================
//  FM4 – MIDI Foot Controller (4-button, 2-stage)
//  Firmware : 2.2.0
// ============================================================

#include <AltSoftSerial.h>
#include <EEPROM.h>

// ============================================================
//  แก้ค่า DEFAULT ที่นี่ที่เดียว
// ============================================================

#define DEFAULT_MIDI_CH     1     // MIDI channel (1-16)
#define DEFAULT_HOLD_MS   750     // กดค้างกี่ ms เพื่อเข้า Free Mode
#define DEFAULT_BOOT_CMD true     // ส่ง PC+CC อัตโนมัติตอนเปิดเครื่อง
#define DEFAULT_BOOT_FS     1     // FS ที่จะ activate ตอนบูท (1-4)
#define DEFAULT_BOOT_STG    1     // Stage ที่จะ activate ตอนบูท (1-2)
#define DEFAULT_COLOR    true     // true = S1 เขียว / S2 แดง  | false = สลับสี

//                                FS1   FS2   FS3   FS4
const byte DEF_PC_S1[4]       = {  0,    1,    2,    6  };  // PC Stage1
const byte DEF_PC_S2[4]       = {  3,    4,    5,    8  };  // PC Stage2

const byte DEF_SNAP_CC_S1[4]  = { 40,   41,   42,   43  };  // Snap CC Stage1
const byte DEF_SNAP_CC_S2[4]  = { 50,   51,   52,   53  };  // Snap CC Stage2

const byte DEF_SNAP_ON_S1[4]  = { 127,  127,  127,  127 };  // Snap ON  Stage1
const byte DEF_SNAP_ON_S2[4]  = { 127,  127,  127,  127 };  // Snap ON  Stage2
const byte DEF_SNAP_OFF_S1[4] = {  0,    0,    0,    0  };  // Snap OFF Stage1
const byte DEF_SNAP_OFF_S2[4] = {  0,    0,    0,    0  };  // Snap OFF Stage2

//                               Slot1  Slot2  Slot3
const byte DEF_FREE_CC_S1[3]  = { 13,   17,   21  };        // Free CC Stage1
const byte DEF_FREE_CC_S2[3]  = { 22,   23,   30  };        // Free CC Stage2

//                                FS1   FS2   FS3   FS4
const byte DEF_FREE_ON_S1[4]  = { 127,  127,  127,  127 };  // Free ON  Stage1
const byte DEF_FREE_ON_S2[4]  = { 127,  127,  127,  127 };  // Free ON  Stage2
const byte DEF_FREE_OFF_S1[4] = {  0,    0,    0,    0  };  // Free OFF Stage1
const byte DEF_FREE_OFF_S2[4] = {  0,    0,    0,    0  };  // Free OFF Stage2

// ============================================================
//  Firmware / Config identity
// ============================================================

#define FW_VERSION   "2.2.0"
#define CFG_SIG      0xBEEF
#define CFG_VER      2         // bump ทุกครั้งที่ struct Config เปลี่ยน layout

// ============================================================
//  Pins
// ============================================================

const byte PIN_BTN[4]  = { A3, A2, A1, A0 };
const byte PIN_LED1[4] = {  2,  3,  4,  5 };   // Stage-1 LEDs
const byte PIN_LED2[4] = {  6,  7, 10, 11 };   // Stage-2 LEDs

// ============================================================
//  Config (เก็บใน EEPROM)
//  ชื่อตัวแปร = ชื่อ key ที่ใช้ใน Serial API
// ============================================================

struct Config {
  uint16_t signature;
  uint16_t version;

  byte midiChannel;       // SET MIDICHANNEL <1-16>
  uint16_t holdMs;        // SET HOLDMS <ms>

  byte pcS1[4];           // SET PC_S1 <fs 1-4> <val>   Program Change Stage1
  byte pcS2[4];           // SET PC_S2 <fs 1-4> <val>   Program Change Stage2

  byte snapCC_S1[4];      // SET SNAPCC_S1 <fs 1-4> <val>
  byte snapCC_S2[4];      // SET SNAPCC_S2 <fs 1-4> <val>
  byte snapOn_S1[4];      // SET SNAPON_S1 <fs 1-4> <val>
  byte snapOn_S2[4];      // SET SNAPON_S2 <fs 1-4> <val>
  byte snapOff_S1[4];     // SET SNAPOFF_S1 <fs 1-4> <val>
  byte snapOff_S2[4];     // SET SNAPOFF_S2 <fs 1-4> <val>

  byte freeCC_S1[3];      // SET FREECC_S1 <slot 1-3> <val>
  byte freeCC_S2[3];      // SET FREECC_S2 <slot 1-3> <val>
  byte freeOn_S1[4];      // SET FREEON_S1 <fs 1-4> <val>
  byte freeOn_S2[4];      // SET FREEON_S2 <fs 1-4> <val>
  byte freeOff_S1[4];     // SET FREEOFF_S1 <fs 1-4> <val>
  byte freeOff_S2[4];     // SET FREEOFF_S2 <fs 1-4> <val>

  bool bootEnable;        // SET BOOTEN <0/1>
  byte bootFS;            // SET BOOTFS <1-4>
  byte bootStage;         // SET BOOTSTAGE <1-2>

  bool colorSwap;         // SET COLOR <0/1>
};

Config cfg;

// ============================================================
//  Runtime state
// ============================================================

AltSoftSerial midiSerial;        // TX=9  RX=8

byte curStage[4]   = {};         // 0=ดับ  1=Stage1  2=Stage2
byte savedStage[4] = {};         // snapshot ก่อนเข้า Free Mode

bool    freeMode  = false;
int8_t  freeBtn   = -1;
byte*   freeCCPtr = nullptr;

bool     btnLastRaw[4]    = { HIGH, HIGH, HIGH, HIGH };
bool     btnStable[4]     = { HIGH, HIGH, HIGH, HIGH };
uint32_t btnDebounceTs[4] = {};
uint32_t btnPressTs[4]    = {};
bool     btnLongDone[4]   = {};

bool     blinking = false;
bool     blinkOn  = false;
uint32_t blinkTs  = 0;

static uint32_t lastBeat = 0;


// ============================================================
//  Timing constants
// ============================================================

static const uint16_t DEBOUNCE_MS  =  30;
static const uint16_t BLINK_MS     = 250;
static const uint16_t BOOT_CHASE   = 150;
static const uint16_t BOOT_BLINK   = 250;
static const uint16_t BOOT_WAIT_MS = 1000;

// ============================================================
//  EEPROM
// ============================================================

static void defaultConfig()
{
  cfg.signature  = CFG_SIG;
  cfg.version    = CFG_VER;
  cfg.midiChannel = DEFAULT_MIDI_CH;
  cfg.holdMs      = DEFAULT_HOLD_MS;

  memcpy(cfg.pcS1,       DEF_PC_S1,       4);
  memcpy(cfg.pcS2,       DEF_PC_S2,       4);
  memcpy(cfg.snapCC_S1,  DEF_SNAP_CC_S1,  4);
  memcpy(cfg.snapCC_S2,  DEF_SNAP_CC_S2,  4);
  memcpy(cfg.snapOn_S1,  DEF_SNAP_ON_S1,  4);
  memcpy(cfg.snapOn_S2,  DEF_SNAP_ON_S2,  4);
  memcpy(cfg.snapOff_S1, DEF_SNAP_OFF_S1, 4);
  memcpy(cfg.snapOff_S2, DEF_SNAP_OFF_S2, 4);
  memcpy(cfg.freeCC_S1,  DEF_FREE_CC_S1,  3);
  memcpy(cfg.freeCC_S2,  DEF_FREE_CC_S2,  3);
  memcpy(cfg.freeOn_S1,  DEF_FREE_ON_S1,  4);
  memcpy(cfg.freeOn_S2,  DEF_FREE_ON_S2,  4);
  memcpy(cfg.freeOff_S1, DEF_FREE_OFF_S1, 4);
  memcpy(cfg.freeOff_S2, DEF_FREE_OFF_S2, 4);

  cfg.bootEnable = DEFAULT_BOOT_CMD;
  cfg.bootFS     = DEFAULT_BOOT_FS;
  cfg.bootStage  = DEFAULT_BOOT_STG;
  cfg.colorSwap  = DEFAULT_COLOR;
}

static void saveConfig() { EEPROM.put(0, cfg); }

static void loadConfig()
{
  EEPROM.get(0, cfg);
  if (cfg.signature != CFG_SIG || cfg.version != CFG_VER) {
    defaultConfig();
    saveConfig();
  }
}

// ============================================================
//  MIDI
// ============================================================

static byte midiStatus(byte base)
{
  return base | (constrain(cfg.midiChannel, 1, 16) - 1);
}

static void sendPC(byte prog)
{
  byte st = midiStatus(0xC0);
  midiSerial.write(st);  midiSerial.write(prog);
  Serial.write(st);      Serial.write(prog);
  delay(5);
}

static void sendCC(byte cc, byte val)
{
  byte st = midiStatus(0xB0);
  midiSerial.write(st);  midiSerial.write(cc);  midiSerial.write(val);
  Serial.write(st);      Serial.write(cc);       Serial.write(val);
  delay(5);
}

// ============================================================
//  LED
// ============================================================

static void setLED(byte idx, byte stage)
{
  bool s1 = cfg.colorSwap ? (stage == 1) : (stage == 2);
  bool s2 = cfg.colorSwap ? (stage == 2) : (stage == 1);
  digitalWrite(PIN_LED1[idx], s1);
  digitalWrite(PIN_LED2[idx], s2);
}

static void refreshLEDs()
{
  for (byte i = 0; i < 4; i++) {
    if (freeMode && i == (byte)freeBtn) continue;
    if (freeMode) {
      byte otherStage = (curStage[freeBtn] == 1) ? 2 : 1;
      setLED(i, curStage[i] ? otherStage : 0);
    } else {
      setLED(i, curStage[i]);
    }
  }
}

// ============================================================
//  Serial API
//
//  GET                       → ส่งค่า config ทั้งหมดกลับ (key:value)
//  SET <KEY> <index> <val>   → ตั้งค่า array (index = 1-based)
//  SET <KEY> <val>           → ตั้งค่า scalar
//  SAVE                      → บันทึกลง EEPROM  → "OK"
//  FACTORY                   → reset default (ยังไม่ save) → "OK"
//  PING                      → "PONG"
//
//  ตอบกลับ: "OK" หรือ "ERR"
// ============================================================

static void handleSerial();  // forward declare

static void pollWait(unsigned long ms)
{
  uint32_t t = millis();
  while (millis() - t < ms) handleSerial();
}

static void printAll()
{
  Serial.print(F("FW:"));         Serial.println(F(FW_VERSION));
  Serial.print(F("MIDICHANNEL:")); Serial.println(cfg.midiChannel);
  Serial.print(F("HOLDMS:"));     Serial.println(cfg.holdMs);
  Serial.print(F("BOOTEN:"));     Serial.println(cfg.bootEnable);
  Serial.print(F("BOOTFS:"));     Serial.println(cfg.bootFS);
  Serial.print(F("BOOTSTAGE:"));  Serial.println(cfg.bootStage);
  Serial.print(F("COLOR:"));      Serial.println(cfg.colorSwap);

  // ส่งเป็น KEY:i:val  (i = 1-based)
  for (byte i = 0; i < 4; i++) {
    byte n = i + 1;
    Serial.print(F("PC_S1:"));      Serial.print(n); Serial.print(':'); Serial.println(cfg.pcS1[i]);
    Serial.print(F("PC_S2:"));      Serial.print(n); Serial.print(':'); Serial.println(cfg.pcS2[i]);
    Serial.print(F("SNAPCC_S1:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.snapCC_S1[i]);
    Serial.print(F("SNAPCC_S2:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.snapCC_S2[i]);
    Serial.print(F("SNAPON_S1:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.snapOn_S1[i]);
    Serial.print(F("SNAPON_S2:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.snapOn_S2[i]);
    Serial.print(F("SNAPOFF_S1:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.snapOff_S1[i]);
    Serial.print(F("SNAPOFF_S2:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.snapOff_S2[i]);
    Serial.print(F("FREEON_S1:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.freeOn_S1[i]);
    Serial.print(F("FREEON_S2:"));  Serial.print(n); Serial.print(':'); Serial.println(cfg.freeOn_S2[i]);
    Serial.print(F("FREEOFF_S1:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.freeOff_S1[i]);
    Serial.print(F("FREEOFF_S2:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.freeOff_S2[i]);
  }
  for (byte i = 0; i < 3; i++) {
    byte n = i + 1;
    Serial.print(F("FREECC_S1:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.freeCC_S1[i]);
    Serial.print(F("FREECC_S2:")); Serial.print(n); Serial.print(':'); Serial.println(cfg.freeCC_S2[i]);
  }
  Serial.println(F("END"));
}

// ตาราง mapping key → (pointer array, max index)
// ใช้สำหรับ SET command แบบ array
struct ArrayEntry {
  const char* key;
  byte*       arr;
  byte        maxIdx;  // ขนาด array
};

static bool handleSet(const String& cmd)
{
  // รูปแบบ: "SET KEY val"  หรือ  "SET KEY index val"
  // แยก token ด้วย space
  int s1 = cmd.indexOf(' ');        // หลัง "SET"
  int s2 = cmd.indexOf(' ', s1+1);  // หลัง KEY
  if (s1 < 0 || s2 < 0) return false;

  String key = cmd.substring(s1+1, s2);
  key.toUpperCase();
  String rest = cmd.substring(s2+1);

  // ---- Scalar ----
  if (key == F("MIDICHANNEL")) { cfg.midiChannel = constrain(rest.toInt(), 1, 16);   return true; }
  if (key == F("HOLDMS"))      { cfg.holdMs      = constrain(rest.toInt(), 50, 5000); return true; }
  if (key == F("BOOTEN"))      { cfg.bootEnable  = rest.toInt() != 0;                return true; }
  if (key == F("BOOTFS"))      { cfg.bootFS      = constrain(rest.toInt(), 1, 4);    return true; }
  if (key == F("BOOTSTAGE"))   { cfg.bootStage   = constrain(rest.toInt(), 1, 2);    return true; }
  if (key == F("COLOR"))       { cfg.colorSwap   = rest.toInt() != 0;                return true; }

  // ---- Array: "SET KEY index val" ----
  int s3 = rest.indexOf(' ');
  if (s3 < 0) return false;

  byte idx = rest.substring(0, s3).toInt() - 1;   // 1-based → 0-based
  byte val = constrain(rest.substring(s3+1).toInt(), 0, 127);

  if (key == F("PC_S1")      && idx < 4) { cfg.pcS1[idx]      = val; return true; }
  if (key == F("PC_S2")      && idx < 4) { cfg.pcS2[idx]      = val; return true; }
  if (key == F("SNAPCC_S1")  && idx < 4) { cfg.snapCC_S1[idx] = val; return true; }
  if (key == F("SNAPCC_S2")  && idx < 4) { cfg.snapCC_S2[idx] = val; return true; }
  if (key == F("SNAPON_S1")  && idx < 4) { cfg.snapOn_S1[idx] = val; return true; }
  if (key == F("SNAPON_S2")  && idx < 4) { cfg.snapOn_S2[idx] = val; return true; }
  if (key == F("SNAPOFF_S1") && idx < 4) { cfg.snapOff_S1[idx]= val; return true; }
  if (key == F("SNAPOFF_S2") && idx < 4) { cfg.snapOff_S2[idx]= val; return true; }
  if (key == F("FREECC_S1")  && idx < 3) { cfg.freeCC_S1[idx] = val; return true; }
  if (key == F("FREECC_S2")  && idx < 3) { cfg.freeCC_S2[idx] = val; return true; }
  if (key == F("FREEON_S1")  && idx < 4) { cfg.freeOn_S1[idx] = val; return true; }
  if (key == F("FREEON_S2")  && idx < 4) { cfg.freeOn_S2[idx] = val; return true; }
  if (key == F("FREEOFF_S1") && idx < 4) { cfg.freeOff_S1[idx]= val; return true; }
  if (key == F("FREEOFF_S2") && idx < 4) { cfg.freeOff_S2[idx]= val; return true; }

  return false;
}

static void handleSerial()
{
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if      (cmd == F("GET"))            { printAll(); }
  else if (cmd.startsWith(F("SET ")))  { Serial.println(handleSet(cmd) ? F("OK") : F("ERR")); }
  else if (cmd == F("SAVE"))           { saveConfig();    Serial.println(F("OK")); }
  else if (cmd == F("FACTORY"))        { defaultConfig(); Serial.println(F("OK")); }
  else if (cmd == F("PING_FM4"))           { Serial.println(F("PONG_FM4")); }
}

// ============================================================
//  Boot animation
// ============================================================

static void bootAnimation()
{
  for (byte i = 0; i < 4; i++) { digitalWrite(PIN_LED1[i], HIGH); pollWait(BOOT_CHASE); }
  for (byte i = 0; i < 4; i++) { digitalWrite(PIN_LED2[i], HIGH); pollWait(BOOT_CHASE); }
  pollWait(150);
  for (byte i = 0; i < 4; i++) setLED(i, 0);
  pollWait(300);

  for (byte b = 0; b < 2; b++) {
    for (byte i = 0; i < 4; i++) { digitalWrite(PIN_LED1[i], HIGH); digitalWrite(PIN_LED2[i], LOW); }
    pollWait(BOOT_BLINK);
    for (byte i = 0; i < 4; i++) { digitalWrite(PIN_LED1[i], LOW);  digitalWrite(PIN_LED2[i], HIGH); }
    pollWait(BOOT_BLINK);
  }
  for (byte i = 0; i < 4; i++) setLED(i, 0);
}

static void sendBootCommand()
{
  if (!cfg.bootEnable) return;

  byte btn   = constrain(cfg.bootFS,    1, 4) - 1;
  byte stage = constrain(cfg.bootStage, 1, 2);

  for (byte i = 0; i < 4; i++) curStage[i] = (i == btn) ? stage : 0;

  pollWait(BOOT_WAIT_MS);
  refreshLEDs();

  sendPC (stage == 1 ? cfg.pcS1[btn]      : cfg.pcS2[btn]);
  sendCC (stage == 1 ? cfg.snapCC_S1[btn] : cfg.snapCC_S2[btn],
          stage == 1 ? cfg.snapOn_S1[btn]  : cfg.snapOn_S2[btn]);
}

// ============================================================
//  Button debounce
// ============================================================

static bool buttonPressed(byte i)
{
  bool raw = digitalRead(PIN_BTN[i]);
  if (raw != btnLastRaw[i]) { btnDebounceTs[i] = millis(); btnLastRaw[i] = raw; }
  if (millis() - btnDebounceTs[i] > DEBOUNCE_MS) btnStable[i] = raw;
  return btnStable[i] == LOW;
}

// ============================================================
//  Toggle stage (กดสั้น โหมดปกติ)
// ============================================================

static void toggleStage(byte idx)
{
  for (byte i = 0; i < 4; i++)
    if (i != idx) { curStage[i] = 0; setLED(i, 0); }

  curStage[idx] = (curStage[idx] == 1) ? 2 : 1;
  refreshLEDs();

  if (curStage[idx] == 1) {
    sendPC(cfg.pcS1[idx]);
    sendCC(cfg.snapCC_S1[idx], cfg.snapOn_S1[idx]);
  } else {
    sendPC(cfg.pcS2[idx]);
    sendCC(cfg.snapCC_S2[idx], cfg.snapOn_S2[idx]);
  }
}

// ============================================================
//  Setup
// ============================================================

void setup()
{
  Serial.begin(115200);
  midiSerial.begin(31250);
  Serial.setTimeout(200);

  for (byte i = 0; i < 4; i++) {
    pinMode(PIN_BTN[i],  INPUT_PULLUP);
    pinMode(PIN_LED1[i], OUTPUT);
    pinMode(PIN_LED2[i], OUTPUT);
    setLED(i, 0);
    btnLastRaw[i] = btnStable[i] = digitalRead(PIN_BTN[i]);
  }

  loadConfig();
  bootAnimation();

  // delay(1000);
  Serial.println("FM4_READY");
  pollWait(800);
  sendBootCommand();
}

// ============================================================
//  Main loop
// ============================================================

void loop()
{
  handleSerial();

  if (millis() - lastBeat > 1000) {
    Serial.println("ALIVE");
    lastBeat = millis();
  }

  uint32_t now = millis();

  for (byte i = 0; i < 4; i++) {
    bool pressed = buttonPressed(i);

    // ---- ปุ่มที่ถือค้างใน Free Mode ----
    if (freeMode && i == (byte)freeBtn) {
      if (pressed) {
        if (!btnPressTs[i]) btnPressTs[i] = now;

        if (!btnLongDone[i] && now - btnPressTs[i] >= cfg.holdMs) {
          btnLongDone[i] = true;

          // PC toggle แล้วกลับ เพื่อ flush state DAW
          if (curStage[i] == 1) { sendPC(cfg.pcS2[i]); delay(20); sendPC(cfg.pcS1[i]); }
          else                  { sendPC(cfg.pcS1[i]); delay(20); sendPC(cfg.pcS2[i]); }

          freeMode = false;  blinking = false;  blinkOn = false;  freeBtn = -1;
          for (byte j = 0; j < 4; j++) if (j != i) curStage[j] = savedStage[j];
          refreshLEDs();
        }
      } else {
        btnPressTs[i] = 0;  btnLongDone[i] = false;
      }
      continue;
    }

    // ---- ปุ่มปกติ ----
    if (pressed) {
      if (!btnPressTs[i]) btnPressTs[i] = now;

      // กดค้าง → เข้า Free Mode
      if (!btnLongDone[i] && !freeMode && curStage[i] && now - btnPressTs[i] >= cfg.holdMs) {
        btnLongDone[i] = true;
        freeMode  = true;
        freeBtn   = i;
        blinking  = true;
        freeCCPtr = (curStage[i] == 1) ? cfg.freeCC_S1 : cfg.freeCC_S2;
        memcpy(savedStage, curStage, sizeof(curStage));
      }
    } else {
      // ปล่อยปุ่ม (short press)
      if (btnPressTs[i] && !btnLongDone[i]) {

        if (freeMode && i != (byte)freeBtn) {
          // หา slot (0-2) สำหรับปุ่มนี้ ข้ามตำแหน่ง freeBtn
          byte slot = 0, count = 0;
          for (byte k = 0; k < 4; k++) {
            if (k == (byte)freeBtn) continue;
            if (k == i) { slot = count; break; }
            count++;
          }

          bool turnOn = (curStage[i] == 0);
          byte ccNum  = freeCCPtr[slot];
          byte ccVal  = (curStage[freeBtn] == 1)
                        ? (turnOn ? cfg.freeOn_S1[i] : cfg.freeOff_S1[i])
                        : (turnOn ? cfg.freeOn_S2[i] : cfg.freeOff_S2[i]);

          curStage[i] = turnOn ? 1 : 0;
          sendCC(ccNum, ccVal);
          refreshLEDs();

        } else if (!freeMode) {
          toggleStage(i);
        }
      }
      btnPressTs[i] = 0;  btnLongDone[i] = false;
    }
  }

  // Blink LED ปุ่ม Free Mode
  if (blinking && freeBtn >= 0 && now - blinkTs >= BLINK_MS) {
    blinkTs = now;
    blinkOn = !blinkOn;
    setLED(freeBtn, blinkOn ? curStage[freeBtn] : 0);
  }
}
