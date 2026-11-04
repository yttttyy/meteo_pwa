#pragma once
#include "config.h"
#include "globals.h"
#include "timekeeping.h"

// ============================================================
//   WIFI: ПРОВИЖИНИНГ ЧЕРЕЗ ТОЧКУ ДОСТУПА + CAPTIVE PORTAL
// ============================================================
// Креды хранятся в Preferences. Нет сохранённой сети ИЛИ не подключилась
// за WIFI_CONNECT_MS -> открытая AP "Meteo-Setup" + DNS-заглушка (все имена
// резолвятся в 192.168.4.1, телефон сам открывает страницу настройки).
// Часы/датчики при этом работают. В рабочем режиме: ретраи каждые 30с,
// офлайн дольше 3 мин -> AP поднимается параллельно STA.

bool     mdnsStarted = false;
bool     netUpDone = false;            // NTP/mDNS уже сконфигурированы
bool     wifiConnectPending = false;   // /wifi/save попросил попытку подключения
uint32_t apStopAtMs = 0;               // STA поднялась при живой AP: гасим AP с задержкой,
                                       // чтобы телефон успел получить ответ страницы

void startAP() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);              // STA оставляем: нужен для сканирования и ретраев
  WiFi.softAP(AP_SSID);                // открытая сеть
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

// сеть поднялась: NTP + mDNS (один раз)
void netUp() {
  netUpDone = true;
  lastOnlineMs = millis();
  sntp_set_time_sync_notification_cb(onNtpSync);   // при NTP-синхронизации запишем время в RTC
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  if (!mdnsStarted && MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
  }
  if (apActive) apStopAtMs = millis() + 60000;   // AP погаснет через минуту
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

void wifiLoadCreds() {
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
}

// каждый проход loop: DNS captive portal + отложенная попытка подключения
void wifiPoll() {
  if (apActive) dnsServer.processNextRequest();
  if (wifiConnectPending) {
    wifiConnectPending = false;
    // не подключились (опечатка в пароле, сети нет) — портал настройки поднимаем
    // сразу, иначе пришлось бы ждать WIFI_AP_FALLBACK_MS
    if (!connectSTA(WIFI_CONNECT_MS)) startAP();
  }
  if (apActive && apStopAtMs && millis() > apStopAtMs && WiFi.status() == WL_CONNECTED)
    stopAP();
}

// каждые WIFI_RETRY_MS: ретраи и AP-фолбэк
void wifiTick() {
  if (WiFi.status() == WL_CONNECTED) {
    lastOnlineMs = millis();
    if (!netUpDone) netUp();   // сеть поднялась фоновым реконнектом (на буте её не было)
    return;
  }
  if (wifiSsid.length()) WiFi.reconnect();
  if (!apActive && millis() - lastOnlineMs > WIFI_AP_FALLBACK_MS)
    startAP();   // долго офлайн — даём возможность перенастроить сеть
}

// --- веб-хендлеры ---

String jsonEscape(const String &s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

void handleWifiScan() {
  int n = WiFi.scanNetworks();   // блокирует ~2с — приемлемо для страницы настройки
  String j; j.reserve(1024);
  j += '[';
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += "{\"ssid\":\""; j += jsonEscape(WiFi.SSID(i));
    j += "\",\"rssi\":"; j += WiFi.RSSI(i);
    j += ",\"open\":";   j += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";
    j += '}';
  }
  j += ']';
  WiFi.scanDelete();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleWifiSave() {
  if (!server.hasArg("ssid") || !server.arg("ssid").length()) {
    server.send(400, "text/plain", "no ssid");
    return;
  }
  wifiSsid = server.arg("ssid");
  wifiPass = server.hasArg("pass") ? server.arg("pass") : "";
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  server.send(200, "text/plain", "OK");
  wifiConnectPending = true;   // подключаемся после отправки ответа (см. wifiPoll)
}

// забыть сеть: стереть креды и уйти в режим настройки (удобно перед переездом)
void handleWifiForget() {
  prefs.remove("ssid");
  prefs.remove("pass");
  wifiSsid = "";
  wifiPass = "";
  server.send(200, "text/plain", "OK");
  delay(300);              // дать странице получить ответ до разрыва связи
  WiFi.disconnect(true);
  netUpDone = false;
  startAP();
}

void handleWifiStatus() {
  bool sta = (WiFi.status() == WL_CONNECTED);
  String j = "{\"sta\":";
  j += sta ? "true" : "false";
  j += ",\"ssid\":\""; j += jsonEscape(wifiSsid);
  j += "\",\"ip\":\""; j += sta ? WiFi.localIP().toString() : "";
  j += "\",\"rssi\":"; j += sta ? WiFi.RSSI() : 0;
  j += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}
