/*
  Meteo-Node — узел-сателлит метеостанции (другая комната).
  ESP32-S3 Dev Module + AHT21 + ENS160 (I2C 17/18).

  Логика: WiFi-клиент, раз в 20с шлёт свои показания на основную станцию
  GET http://<станция>/api/push?id&name&t&h&aqi&tvoc&eco2
  Станция сама регистрирует узел по первому пушу.

  Первичная настройка: нет сети / не подключился за 20с -> открытая AP "Meteo-Node"
  с captive portal: выбрать WiFi, задать имя комнаты и адрес станции (meteo.local).
  Своя страничка: http://meteonode.local (показания, настройки, OTA).

  IDE: плата "ESP32S3 Dev Module", USB CDC On Boot: Enabled.
*/

#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include "DFRobot_ENS160.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>

#include "node_config.h"
#include "node_web.h"

Adafruit_AHTX0     aht;
DFRobot_ENS160_I2C ens(&Wire, 0x53);
WebServer          server(80);
DNSServer          dnsServer;
Preferences        prefs;

// --- состояние ---
float   curTemp = 0;
int     curHum = 0, curAQI = 0, curTVOC = 0, curECO2 = 400;
bool    ahtOk = false, ensOk = false;
uint8_t ahtFail = 0, ensFail = 0;

String  wifiSsid, wifiPass, roomName, stationAddr;
bool    apActive = false;
uint32_t lastOnlineMs = 0;
bool    wifiConnectPending = false;
uint32_t apStopAtMs = 0;
bool    mdnsStarted = false;

IPAddress stationIP;          // кэш резолва meteo.local
bool     stationResolved = false;
uint8_t  pushFails = 0;
bool     lastPushOk = false;

uint32_t nodeId = 0;          // стабильный ID из efuse MAC
uint32_t tSens = 0, tPush = 0, tWifi = 0;

bool ahtLive() { return ahtOk && ahtFail < 3; }
bool ensLive() { return ensOk && ensFail < 3; }

// ============================================================
//   WiFi: портал настройки (как у станции, укороченный)
// ============================================================
void startAP() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  dnsServer.start(53, "*", WiFi.softAPIP());
  apActive = true;
  apStopAtMs = 0;
}

void stopAP() {
  if (!apActive) return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
}

void netUp() {
  lastOnlineMs = millis();
  if (!mdnsStarted && MDNS.begin(MDNS_NODE)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
  }
  if (apActive) apStopAtMs = millis() + 60000;
}

bool connectSTA(uint32_t timeoutMs) {
  if (!wifiSsid.length()) return false;
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs && WiFi.status() != WL_CONNECTED) delay(100);
  if (WiFi.status() == WL_CONNECTED) { netUp(); return true; }
  return false;
}

void wifiPoll() {
  if (apActive) dnsServer.processNextRequest();
  if (wifiConnectPending) {
    wifiConnectPending = false;
    connectSTA(WIFI_CONNECT_MS);
  }
  if (apActive && apStopAtMs && millis() > apStopAtMs && WiFi.status() == WL_CONNECTED)
    stopAP();
}

void wifiTick() {
  if (WiFi.status() == WL_CONNECTED) {
    lastOnlineMs = millis();
    if (!mdnsStarted) netUp();
    return;
  }
  if (wifiSsid.length()) WiFi.reconnect();
  if (!apActive && millis() - lastOnlineMs > AP_FALLBACK_MS) startAP();
}

// ============================================================
//   Датчики
// ============================================================
void readSensors() {
  sensors_event_t hum, temp;
  if (ahtOk) {
    if (aht.getEvent(&hum, &temp)) {
      ahtFail = 0;
      float t = temp.temperature + NODE_TEMP_OFFSET;
      float h = constrain(hum.relative_humidity + NODE_HUM_OFFSET, 0.0f, 100.0f);
      curTemp = t;
      curHum  = round(h);
      if (ensLive()) ens.setTempAndHum(t, h);   // компенсация ENS160 реальным воздухом
    } else if (ahtFail < 250) ahtFail++;
  }
  if (ensOk) {
    uint8_t aqi = ens.getAQI();
    if (aqi >= 1 && aqi <= 5) {
      ensFail = 0;
      curAQI  = aqi;
      curTVOC = ens.getTVOC();
      curECO2 = ens.getECO2();
    } else if (ensFail < 250) ensFail++;
  }
}

// ============================================================
//   Пуш на станцию
// ============================================================
String urlEncode(const String &s) {
  String o;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { o += '%'; o += hex[c >> 4]; o += hex[c & 15]; }
  }
  return o;
}

bool resolveStation() {
  // явный IP в настройках — используем как есть
  if (stationIP.fromString(stationAddr)) { stationResolved = true; return true; }
  String host = stationAddr;
  host.replace(".local", "");
  IPAddress ip = MDNS.queryHost(host, 3000);
  if (ip == IPAddress()) return false;
  stationIP = ip;
  stationResolved = true;
  return true;
}

void pushToStation() {
  if (WiFi.status() != WL_CONNECTED) { lastPushOk = false; return; }
  if (!stationResolved && !resolveStation()) { lastPushOk = false; return; }
  HTTPClient http;
  String url = "http://" + stationIP.toString() + "/api/push?id=" + String(nodeId) +
               "&name=" + urlEncode(roomName) +
               "&t=" + String(curTemp, 1) + "&h=" + String(curHum) +
               "&aqi=" + String(curAQI) + "&tvoc=" + String(curTVOC) +
               "&eco2=" + String(curECO2);
  http.setTimeout(4000);
  bool ok = false;
  if (http.begin(url)) {
    ok = (http.GET() == 200);
    http.end();
  }
  lastPushOk = ok;
  if (ok) pushFails = 0;
  else if (++pushFails >= PUSH_FAILS_RERESOLVE) {
    stationResolved = false;   // станция могла сменить IP — перерезолвим mDNS
    pushFails = 0;
  }
}

// ============================================================
//   Веб узла
// ============================================================
void handleRoot() { server.send_P(200, "text/html", NODE_HTML); }

void handleInfo() {
  String j = "{\"room\":\"";
  String r = roomName; r.replace("\\", "\\\\"); r.replace("\"", "\\\"");
  j += r;
  j += "\",\"station\":\""; j += stationAddr;
  j += "\",\"t\":";    j += String(curTemp, 1);
  j += ",\"h\":";      j += curHum;
  j += ",\"aqi\":";    j += curAQI;
  j += ",\"tvoc\":";   j += curTVOC;
  j += ",\"eco2\":";   j += curECO2;
  j += ",\"ok\":";     j += ahtLive() ? "true" : "false";
  j += ",\"eok\":";    j += ensLive() ? "true" : "false";
  j += ",\"push\":";   j += lastPushOk ? "true" : "false";
  j += ",\"sta\":";    j += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  j += ",\"rssi\":";   j += (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  j += ",\"fw\":\"" NODE_FW "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    String s = WiFi.SSID(i); s.replace("\\", "\\\\"); s.replace("\"", "\\\"");
    j += "{\"ssid\":\""; j += s;
    j += "\",\"rssi\":"; j += WiFi.RSSI(i);
    j += '}';
  }
  j += ']';
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleSave() {
  if (server.hasArg("ssid") && server.arg("ssid").length()) {
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.hasArg("pass") ? server.arg("pass") : "");
  }
  if (server.hasArg("room") && server.arg("room").length())
    prefs.putString("room", server.arg("room"));
  if (server.hasArg("station") && server.arg("station").length())
    prefs.putString("station", server.arg("station"));
  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();   // проще перезапуститься с новыми настройками
}

void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  delay(500);
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START)        Update.begin(UPDATE_SIZE_UNKNOWN);
  else if (up.status == UPLOAD_FILE_WRITE)   Update.write(up.buf, up.currentSize);
  else if (up.status == UPLOAD_FILE_END)     Update.end(true);
  else if (up.status == UPLOAD_FILE_ABORTED) Update.abort();
}

void handleReboot() {
  server.send(200, "text/plain", "OK");
  delay(300);
  ESP.restart();
}

void handleNotFound() {   // captive portal
  if (apActive) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "not found");
  }
}

// ============================================================
void setup() {
  Serial.begin(115200);
  nodeId = (uint32_t)(ESP.getEfuseMac() >> 16);

  Wire.begin(PIN_SDA, PIN_SCL);
  ahtOk = aht.begin(&Wire);
  for (uint8_t i = 0; i < 3 && !ensOk; i++) {
    ensOk = (ens.begin() == NO_ERR);
    if (!ensOk) delay(500);
  }
  if (ensOk) ens.setPWRMode(ENS160_STANDARD_MODE);
  Serial.printf("[node %u] AHT:%d ENS:%d\n", nodeId, ahtOk, ensOk);

  prefs.begin("node", false);
  wifiSsid    = prefs.getString("ssid", "");
  wifiPass    = prefs.getString("pass", "");
  roomName    = prefs.getString("room", DEFAULT_ROOM);
  stationAddr = prefs.getString("station", DEFAULT_STATION);

  if (!connectSTA(WIFI_CONNECT_MS)) startAP();

  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  server.begin();

  readSensors();
}

void loop() {
  server.handleClient();
  wifiPoll();

  uint32_t now = millis();
  if (now - tSens >= SENS_PERIOD) {
    tSens = now;
    readSensors();
  }
  if (now - tPush >= PUSH_PERIOD) {
    tPush = now;
    pushToStation();
  }
  if (now - tWifi >= WIFI_RETRY_MS) {
    tWifi = now;
    wifiTick();
  }
}
