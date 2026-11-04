/*
  Метеостанция на ESP32-S3 Dev Module
  LCD2004 RGB (PCF8574, 0x27) + большие часы в стиле AlexGyver MeteoClock
  Датчики: AHT21 + ENS160 (I2C), MH-Z19C (UART), фоторезистор (авто-яркость)
  Веб-сервер (PWA): графики за час/3ч/день/неделю, темы, цвет подсветки, OTA
  WiFi настраивается через точку доступа Meteo-Setup (captive portal)
  Дисплей с кириллическим знакогенератором (KS0066 RUS: И=0xA5 и т.д.)

  Структура:
    config.h      — все настройки (#define)
    globals.h     — объекты и состояние
    bigdigits.h   — большие цифры (порт AlexGyver), CGRAM
    display.h     — отрисовка: часы, дата, датчики
    climate.h     — общий балл климата 0..100 (порт расчёта с веб-страницы)
    backlight.h   — RGB-подсветка (авто по климату/ручной/радуга) + авто-яркость по фоторезистору
    sensors.h     — опрос датчиков, история (3 часа + сутки + неделя)
    timekeeping.h — время: DS3231 (опционально) + NTP + браузерный фолбэк
    wifi_portal.h — WiFi-провижининг через AP + captive portal
    routes.h      — HTTP-роуты
    web_assets.h  — HTML/manifest/иконка (PROGMEM)

  Настройки IDE: плата "ESP32S3 Dev Module", USB CDC On Boot: Enabled.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AHTX0.h>
#include "DFRobot_ENS160.h"
#include "MHZ19.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <LittleFS.h>
#include <time.h>
#include <sys/time.h>
#include <RTClib.h>
#include "esp_sntp.h"

#include "config.h"
#include "globals.h"
#include "bigdigits.h"
#include "nodes.h"
#include "display.h"
#include "climate.h"
#include "backlight.h"
#include "history.h"
#include "sensors.h"
#include "timekeeping.h"
#include "wifi_portal.h"
#include "routes.h"

uint32_t tBl = 0, tLdr = 0, tHalf = 0, tSens = 0, tHist = 0, tWifi = 0, tTouch = 0;

// сенсорная кнопка: цикл экранов дом -> узел0 -> узел1 ... -> дом
void touchTick() {
  bool s = digitalRead(PIN_TOUCH);
  if (s && !touchWas && millis() - touchLastMs > TOUCH_DEBOUNCE) {
    touchLastMs = millis();
    if (nodeCount) {
      lcdView++;
      if (lcdView >= (int8_t)nodeCount) lcdView = -1;
      viewSinceMs = millis();
      drawSensors();
    }
  }
  touchWas = s;
}

// ожидание на старте: веб/подсветка живут, касание кнопки прерывает (вернёт true).
// Прижатый палец помечаем как уже обработанное нажатие, иначе touchTick в loop
// примет его за новое и сразу перелистнёт экран на комнату.
bool waitOrTouch(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (digitalRead(PIN_TOUCH)) {
      touchWas = true;
      touchLastMs = millis();
      return true;
    }
    server.handleClient();
    wifiPoll();
    if (millis() - tBl  >= 50)         { tBl  = millis(); backlightTick(); }
    if (millis() - tLdr >= LDR_PERIOD) { tLdr = millis(); ldrTick(); }
    delay(5);
  }
  return false;
}

// --- стартовый лог: строки уезжают вверх по мере загрузки ---
// bootBegin печатает "ENS160...... ", bootEnd дописывает статус, когда он известен
// (WiFi, например, думает до 20 секунд). Пятая строка сдвигает лог вверх.
char    bootLines[4][21] = {{0}};
uint8_t bootN = 0;             // сколько строк занято

void bootDraw() { for (uint8_t r = 0; r < 4; r++) printRow(r, bootLines[r]); }

void bootBegin(const char *name) {
  bool scrolled = false;
  if (bootN == 4) {            // экран полон — уезжаем на строку вверх
    for (uint8_t r = 0; r < 3; r++) memcpy(bootLines[r], bootLines[r + 1], 21);
    bootN = 3;
    scrolled = true;
  }
  char *l = bootLines[bootN++];
  memset(l, '.', 12);          // имя добиваем точками до 12 знаков
  size_t n = strlen(name);
  if (n > 12) n = 12;
  memcpy(l, name, n);
  l[12] = ' ';
  l[13] = '\0';
  if (scrolled) bootDraw(); else printRow(bootN - 1, l);
}

void bootEnd(const char *status) {
  char *l = bootLines[bootN - 1];
  strlcpy(l + 13, status, sizeof(bootLines[0]) - 13);
  printRow(bootN - 1, l);
}

// стартовый экран: куда заходить за панелью управления + обратный отсчёт.
// В режиме настройки вместо адреса панели показываем, к какой сети подключаться.
void showAddrScreen() {
  char buf[24];
  bool sta = (WiFi.status() == WL_CONNECTED);
  lcd.clear();
  if (sta) {
    utf8ToLcd("АДРЕС ПАНЕЛИ", buf, 20); printRowCentered(0, buf);
    printRowCentered(1, WiFi.localIP().toString().c_str());
    // mDNS мог не подняться — тогда не обещаем имя, которое не отрезолвится
    printRowCentered(2, mdnsStarted ? MDNS_NAME ".local" : "");
  } else {
    utf8ToLcd("ПОДКЛЮЧИСЬ К СЕТИ", buf, 20); printRowCentered(0, buf);
    printRowCentered(1, AP_SSID);   // ASCII — без конвертации
    printRowCentered(2, WiFi.softAPIP().toString().c_str());
  }
  int last = -1;
  uint32_t t0 = millis();
  while (true) {
    int remain = ADDR_SCREEN_S - (int)((millis() - t0) / 1000);
    if (remain <= 0) break;
    if (remain != last) {   // тик раз в секунду, справа внизу
      last = remain;
      char b[4];
      snprintf(b, sizeof(b), "%2d", remain);
      putCell(18, 3, b[0]);
      putCell(19, 3, b[1]);
    }
    if (waitOrTouch(200)) break;
  }
}

void setup() {
  Serial.begin(115200);

  ledcAttach(PIN_R, 5000, 8);
  ledcAttach(PIN_G, 5000, 8);
  ledcAttach(PIN_B, 5000, 8);
  setRGB(40, 40, 60);   // тусклый на время загрузки

  pinMode(PIN_TOUCH, INPUT_PULLDOWN);   // pulldown: без кнопки пин не болтается в воздухе

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // --- датчики со статусом в бегущем логе ---
  bootBegin("AHT21");
  ahtOk = aht.begin(&Wire);
  bootEnd(ahtOk ? "OK" : "ERR");

  bootBegin("ENS160");
  for (uint8_t i = 0; i < 3 && !ensOk; i++) {
    ensOk = (ens.begin() == NO_ERR);
    if (!ensOk) delay(500);
  }
  if (ensOk) ens.setPWRMode(ENS160_STANDARD_MODE);
  bootEnd(ensOk ? "OK" : "ERR");

  bootBegin("MH-Z19C");
  mhzSerial.begin(9600, SERIAL_8N1, MHZ_RX, MHZ_TX);
  mhz.begin(mhzSerial);
  mhz.autoCalibration(false);   // автокалибровку выключаем: дом — не улица
  mhz.getCO2();
  mhzOk = (mhz.errorCode == RESULT_OK);
  bootEnd(mhzOk ? "OK" : "ERR");

  // --- настройки из памяти (до WiFi: там лежат креды) ---
  prefs.begin("meteo", false);
  // миграция со старого ключа "auto" (bool) на "cmode" (uint8)
  cMode      = prefs.getUChar("cmode", prefs.getBool("auto", true) ? CMODE_AUTO : CMODE_MANUAL);
  rgbR       = prefs.getUChar("r", 80);
  rgbG       = prefs.getUChar("g", 160);
  rgbB       = prefs.getUChar("b", 255);
  rainbowSpd = prefs.getUChar("rspd", 12);
  bright     = prefs.getUChar("bright", 255);
  brightAuto = prefs.getBool("bauto", false);
  font       = prefs.getUChar("font", FONT_RECT);
  tempOffset = prefs.getFloat("toff", TEMP_OFFSET);
  humOffset  = prefs.getFloat("hoff", HUM_OFFSET);
  co2Alarm   = prefs.getInt("co2al", CO2_ALARM);
  thrLoad();                           // пороги зон климата (дефолты — в climate.h)
  wifiLoadCreds();
  if (!brightAuto) curBright = bright;

  // --- WiFi: сохранённая сеть или точка доступа настройки ---
  bootBegin("WIFI");
  bool online = connectSTA(WIFI_CONNECT_MS);
  if (!online) startAP();
  bootEnd(online ? "OK" : "AP");   // адрес покажет следующий экран

  setenv("TZ", TZ_INFO, 1); tzset();   // часовой пояс — всегда (нужно и офлайн)

  bootBegin("DS3231");
  initTimeFromRTC();                   // DS3231 опционален: нет модуля — время от NTP/браузера
  bootEnd(rtcOk ? "OK" : "--");

  bootBegin("FS/HISTORY");
  historyInit();                       // LittleFS + восстановление графиков после ребута
  bootEnd(fsOk ? "OK" : "ERR");

  bootBegin("MDNS");                   // поднялся внутри connectSTA -> netUp
  bootEnd(mdnsStarted ? "OK" : "--");

  setupRoutes();

  applyManual();
  // статусы датчиков и экран адреса пропускаются одним касанием кнопки
  if (!waitOrTouch(2500)) showAddrScreen();

  // --- главный экран ---
  readSensors();
  redrawMain();
}

void loop() {
  server.handleClient();
  wifiPoll();   // DNS captive portal + отложенное подключение после /wifi/save

  uint32_t now = millis();
  if (now - tBl >= 50) {
    tBl = now;
    backlightTick();
  }
  if (now - tLdr >= LDR_PERIOD) {
    tLdr = now;
    ldrTick();
  }
  if (now - tTouch >= 30) {
    tTouch = now;
    touchTick();
  }
  if (lcdView >= 0 && now - viewSinceMs >= VIEW_RETURN_MS) {
    lcdView = -1;                  // автовозврат на домашний экран
    drawSensors();
  }
  if (now - tHalf >= 500) {
    tHalf = now;
    clockTick();
  }
  if (now - tSens >= SENS_PERIOD) {
    tSens = now;
    readSensors();
  }
  if (now - tHist >= HIST_PERIOD) {
    tHist = now;
    pushHistory();
  }
  if (now - tWifi >= WIFI_RETRY_MS) {
    tWifi = now;
    wifiTick();
  }
}
