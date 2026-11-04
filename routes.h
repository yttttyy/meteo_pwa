#pragma once
#include "config.h"
#include "globals.h"
#include "display.h"
#include "backlight.h"
#include "sensors.h"
#include "timekeeping.h"
#include "wifi_portal.h"
#include "web_assets.h"

// ============================================================
//   ВЕБ-СЕРВЕР: страница, данные, управление
// ============================================================

void handleRoot() {     // страница лежит во флеше сжатой (build_web.py), браузер распакует сам
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}
void handleManifest() { server.send_P(200, "application/manifest+json", MANIFEST_JSON); }
void handleIcon()     { server.send_P(200, "image/svg+xml", ICON_SVG); }

// captive portal: в режиме AP любой незнакомый URL (generate_204,
// hotspot-detect.html и т.п.) редиректим на страницу настройки
void handleNotFound() {
  if (apActive) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "not found");
  }
}

void handleData() {
  bool sta = (WiFi.status() == WL_CONNECTED);
  String j;
  j.reserve(1400);   // +пороги зон (24 числа)
  j += "{\"temp\":";   j += String(curTemp, 1);
  j += ",\"hum\":";    j += curHum;
  j += ",\"co2\":";    j += curCO2;
  j += ",\"aqi\":";    j += curAQI;
  j += ",\"tvoc\":";   j += curTVOC;
  j += ",\"eco2\":";   j += curECO2;
  j += ",\"cmode\":";  j += cMode;
  j += ",\"r\":";      j += rgbR;
  j += ",\"g\":";      j += rgbG;
  j += ",\"b\":";      j += rgbB;
  j += ",\"rspd\":";   j += rainbowSpd;
  j += ",\"bright\":"; j += bright;
  j += ",\"bauto\":";  j += brightAuto ? "true" : "false";
  j += ",\"ldr\":";    j += ldrRaw;
  j += ",\"font\":";   j += font;
  j += ",\"toff\":";   j += String(tempOffset, 1);
  j += ",\"hoff\":";   j += String(humOffset, 1);
  j += ",\"co2al\":";  j += co2Alarm;
  // балл климата считает станция — страница его только показывает, чтобы
  // формула жила в одном месте и LCD с вебом не могли разойтись
  float clim = climateScore();
  j += ",\"clim\":"; j += (clim < 0) ? String("null") : String((int)lroundf(clim));
  j += ",\"trend\":"; j += curTrend;
  // пороги зон: страница рисует по ним плитки и редактор, источник правды — станция
  j += ",\"thr\":{";
  for (uint8_t i = 0; i < THR_N; i++) {
    if (i) j += ',';
    j += '"'; j += THR[i].key; j += "\":[";
    for (uint8_t k = 0; k < THR[i].n; k++) {
      if (k) j += ',';
      float v = THR[i].v[k];
      j += (v == (long)v) ? String((long)v) : String(v, 1);
    }
    j += ']';
  }
  j += '}';
  j += ",\"fw\":\"" FW_VERSION "\"";
  // состояние датчиков: 0 = ОК, 1 = отвалился на ходу, 2 = не найден при старте
  auto sensSt = [](bool boot, bool live) -> uint8_t { return !boot ? 2 : live ? 0 : 1; };
  j += ",\"aht\":";    j += sensSt(ahtOk, ahtLive());
  j += ",\"ens\":";    j += sensSt(ensOk, ensLive());
  j += ",\"mhz\":";    j += sensSt(mhzOk, mhzLive());
  j += ",\"rtc\":";    j += rtcOk ? "true" : "false";
  j += ",\"up\":";     j += (uint32_t)(millis() / 1000);
  j += ",\"heap\":";   j += (uint32_t)ESP.getFreeHeap();
  j += ",\"timeok\":"; j += timeValid() ? "true" : "false";
  // --- внешние узлы ---
  j += ",\"nodes\":[";
  for (uint8_t i = 0; i < nodeCount; i++) {
    if (i) j += ',';
    j += "{\"id\":";     j += nodes[i].id;
    j += ",\"name\":\""; j += jsonEscape(nodes[i].nameUtf);
    j += "\",\"t\":";    j += String(nodes[i].t, 1);
    j += ",\"h\":";      j += nodes[i].h;
    j += ",\"aqi\":";    j += nodes[i].aqi;
    j += ",\"tvoc\":";   j += nodes[i].tvoc;
    j += ",\"eco2\":";   j += nodes[i].eco2;
    j += ",\"age\":";    j += (uint32_t)((millis() - nodes[i].lastMs) / 1000);
    j += ",\"on\":";     j += nodeOnline(i) ? "true" : "false";
    j += '}';
  }
  j += ']';
  j += ",\"sta\":";    j += sta ? "true" : "false";
  j += ",\"ssid\":\""; j += jsonEscape(sta ? WiFi.SSID() : wifiSsid);
  j += "\",\"ip\":\""; j += sta ? WiFi.localIP().toString() : "";
  j += "\",\"rssi\":"; j += sta ? WiFi.RSSI() : 0;
  j += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// история за выбранный период: /hist?range=h|3h|day|week
// час = последние 60 минутных точек, 3h = 180 минутных,
// day = 288 пятиминутных средних, week = 168 часовых средних
void handleHist() {
  String rg = server.hasArg("range") ? server.arg("range") : "3h";
  uint8_t src = (rg == "day") ? 1 : (rg == "week") ? 2 : 0;   // 0=минутный, 1=5мин, 2=часовой
  uint16_t n = (src == 1) ? hist5Count
             : (src == 2) ? histHCount
             : (rg == "h") ? (uint16_t)min((int)histCount, 60)
             : histCount;
  uint32_t step  = (src == 1) ? 300 : (src == 2) ? 3600 : 60;
  uint16_t first = ((src == 1) ? hist5Count : (src == 2) ? histHCount : histCount) - n;

  // значение i-й точки серии s (0=t x10, 1=h, 2=c, 3=a, 4=v, 5=e) с учётом источника
  auto pt = [&](uint8_t s, uint16_t i) -> int32_t {
    uint16_t k = (src == 1) ? hist5Idx(first + i)
               : (src == 2) ? histHIdx(first + i)
               : histIdx(first + i);
    switch (s) {
      case 0:  return (src == 1) ? h5T[k] : (src == 2) ? hHT[k] : hT[k];
      case 1:  return (src == 1) ? h5H[k] : (src == 2) ? hHH[k] : hH[k];
      case 2:  return (src == 1) ? h5C[k] : (src == 2) ? hHC[k] : hC[k];
      case 3:  return (src == 1) ? h5A[k] : (src == 2) ? hHA[k] : hA[k];
      case 4:  return (src == 1) ? h5V[k] : (src == 2) ? hHV[k] : hV[k];
      default: return (src == 1) ? h5E[k] : (src == 2) ? hHE[k] : hE[k];
    }
  };

  String j;
  j.reserve(13000);   // 7 серий
  j += "{\"step\":"; j += step;
  j += ",\"now\":";  j += (uint32_t)time(nullptr);
  j += ",\"t\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += String(pt(0, i) / 10.0, 1); }
  j += "],\"h\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += pt(1, i); }
  j += "],\"c\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += pt(2, i); }
  j += "],\"a\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += pt(3, i); }
  j += "],\"v\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += pt(4, i); }
  j += "],\"e\":[";
  for (uint16_t i = 0; i < n; i++) { if (i) j += ','; j += pt(5, i); }
  // балл климата не храним — пересчитываем из сохранённых точек тем же кодом.
  // Бонусом: график появляется сразу для всей старой истории.
  j += "],\"k\":[";
  for (uint16_t i = 0; i < n; i++) {
    if (i) j += ',';
    float sc = climateScoreOf(pt(0, i) / 10.0f, pt(1, i), pt(2, i), pt(5, i), pt(4, i), pt(3, i));
    j += (sc < 0) ? 0 : (int)lroundf(sc);
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// CSV-выгрузка истории для внешнего анализа: /export?range=h|3h|day|week
// Главное назначение — сверить CO2 (MH-Z19C) с eCO2 (ENS160) и решить,
// нужен ли дорогой NDIR-датчик. Отдаём чанками: весь файл в heap не собираем.
void handleExport() {
  String rg = server.hasArg("range") ? server.arg("range") : "day";
  uint8_t src = (rg == "day") ? 1 : (rg == "week") ? 2 : 0;
  uint16_t total = (src == 1) ? hist5Count : (src == 2) ? histHCount : histCount;
  uint16_t n = (src == 0 && rg == "h") ? (uint16_t)min((int)total, 60) : total;
  uint32_t step  = (src == 1) ? 300 : (src == 2) ? 3600 : 60;
  uint16_t first = total - n;
  time_t now = time(nullptr);
  bool tok = timeValid();

  server.sendHeader("Content-Disposition", "attachment; filename=meteo-" + rg + ".csv");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent("ts,iso,temp,hum,co2,eco2,aqi,tvoc\n");

  String buf;
  buf.reserve(900);
  char iso[20];
  for (uint16_t i = 0; i < n; i++) {
    uint16_t k = (src == 1) ? hist5Idx(first + i)
               : (src == 2) ? histHIdx(first + i)
               : histIdx(first + i);
    // время точки: отсчитываем назад от «сейчас» с шагом буфера
    time_t ts = now - (time_t)(n - 1 - i) * step;
    iso[0] = 0;
    if (tok) { struct tm ti; localtime_r(&ts, &ti); strftime(iso, sizeof(iso), "%Y-%m-%d %H:%M", &ti); }
    buf += (uint32_t)ts;                        buf += ',';
    buf += iso;                                 buf += ',';
    buf += String(((src == 1) ? h5T[k] : (src == 2) ? hHT[k] : hT[k]) / 10.0, 1);
    buf += ',';
    buf += (src == 1) ? h5H[k] : (src == 2) ? hHH[k] : hH[k];   buf += ',';
    buf += (src == 1) ? h5C[k] : (src == 2) ? hHC[k] : hC[k];   buf += ',';
    buf += (src == 1) ? h5E[k] : (src == 2) ? hHE[k] : hE[k];   buf += ',';
    buf += (src == 1) ? h5A[k] : (src == 2) ? hHA[k] : hA[k];   buf += ',';
    buf += (src == 1) ? h5V[k] : (src == 2) ? hHV[k] : hV[k];   buf += '\n';
    if (buf.length() > 700) { server.sendContent(buf); buf = ""; }
  }
  if (buf.length()) server.sendContent(buf);
  server.sendContent("");   // пустой чанк = конец ответа
}

void handleRgb() {
  uint8_t wasMode = cMode;
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    cMode = (m == "auto") ? CMODE_AUTO : (m == "rainbow") ? CMODE_RAINBOW : CMODE_MANUAL;
  }
  if (cMode == CMODE_AUTO && wasMode != CMODE_AUTO) {   // в авто стартуем градиент с текущего цвета
    smR = rgbR; smG = rgbG; smB = rgbB;
  }
  if (server.hasArg("r")) rgbR = constrain(server.arg("r").toInt(), 0, 255);
  if (server.hasArg("g")) rgbG = constrain(server.arg("g").toInt(), 0, 255);
  if (server.hasArg("b")) rgbB = constrain(server.arg("b").toInt(), 0, 255);
  if (server.hasArg("spd")) rainbowSpd = constrain(server.arg("spd").toInt(), 1, 50);
  applyManual();
  if (server.hasArg("save")) {   // пишем во флеш только по отпусканию пикера, бережём ресурс
    prefs.putUChar("cmode", cMode);
    prefs.putUChar("r", rgbR);
    prefs.putUChar("g", rgbG);
    prefs.putUChar("b", rgbB);
    prefs.putUChar("rspd", rainbowSpd);
  }
  server.send(200, "text/plain", "OK");
}

// калибровка датчиков и порог тревоги CO2 (значения в Preferences)
void handleCfg() {
  if (server.hasArg("toff"))  tempOffset = constrain(server.arg("toff").toFloat(), -15.0f, 15.0f);
  if (server.hasArg("hoff"))  humOffset  = constrain(server.arg("hoff").toFloat(), -30.0f, 30.0f);
  if (server.hasArg("co2al")) co2Alarm   = constrain((int)server.arg("co2al").toInt(), 600, 5000);
  // пороги зон: thr_co2=800,1000,1500 и т.д.; кривой набор молча игнорируем
  bool thrChanged = false;
  for (uint8_t i = 0; i < THR_N; i++) {
    char arg[16];
    snprintf(arg, sizeof(arg), "thr_%s", THR[i].key);
    if (server.hasArg(arg) && parseThr(server.arg(arg), THR[i].v, THR[i].n)) thrChanged = true;
  }
  if (server.hasArg("save")) {
    prefs.putFloat("toff", tempOffset);
    prefs.putFloat("hoff", humOffset);
    prefs.putInt("co2al", co2Alarm);
    if (thrChanged) thrSave();   // не трогаем флеш, когда двигали только ползунки
    readSensors();   // применить поправки сразу, не ждать 15с
  }
  server.send(200, "text/plain", "OK");
}

// пуш от узла-сателлита: /api/push?id&name&t&h&aqi&tvoc&eco2 (раз в ~20с)
void handleNodePush() {
  if (!server.hasArg("id")) { server.send(400, "text/plain", "no id"); return; }
  uint32_t id = strtoul(server.arg("id").c_str(), nullptr, 10);
  int i = findOrAddNode(id);
  if (i < 0) { server.send(507, "text/plain", "node table full"); return; }
  Node &n = nodes[i];
  if (server.hasArg("name") && server.arg("name").length()) {
    strlcpy(n.nameUtf, server.arg("name").c_str(), sizeof(n.nameUtf));
    utf8ToLcd(n.nameUtf, n.nameLcd, 12);
  }
  if (server.hasArg("t"))    n.t    = server.arg("t").toFloat();
  if (server.hasArg("h"))    n.h    = server.arg("h").toInt();
  if (server.hasArg("aqi"))  n.aqi  = server.arg("aqi").toInt();
  if (server.hasArg("tvoc")) n.tvoc = server.arg("tvoc").toInt();
  if (server.hasArg("eco2")) n.eco2 = server.arg("eco2").toInt();
  n.lastMs = millis();
  if (lcdView == i) drawSensors();   // смотрим эту комнату — обновить экран сразу
  server.send(200, "text/plain", "OK");
}

// OTA: страница шлёт .bin POST'ом на /update
void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  saveHistory();   // не терять графики при перепрошивке
  delay(500);
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleReboot() {
  server.send(200, "text/plain", "OK");
  saveHistory();   // не терять графики при ребуте
  delay(300);
  ESP.restart();
}

void handleBright() {
  if (server.hasArg("mode")) brightAuto = (server.arg("mode") == "auto");
  if (server.hasArg("v")) bright = constrain(server.arg("v").toInt(), 0, 255);
  if (!brightAuto) curBright = bright;   // в авто яркость подхватит ldrTick
  applyManual();
  if (server.hasArg("save")) {
    prefs.putUChar("bright", bright);
    prefs.putBool("bauto", brightAuto);
  }
  server.send(200, "text/plain", "OK");
}

void handleFont() {
  if (server.hasArg("f")) {
    uint8_t nf = server.arg("f").toInt() ? FONT_ROUND : FONT_RECT;
    if (nf != font) { font = nf; redrawMain(); }
  }
  if (server.hasArg("save")) prefs.putUChar("font", font);
  server.send(200, "text/plain", "OK");
}

void handleCalibrate() {   // ручная калибровка нуля MH-Z19C (текущее = 400 ppm)
  if (!mhzOk) { server.send(503, "text/plain", "no sensor"); return; }
  mhz.calibrate();         // команда zero-point (0x87)
  server.send(200, "text/plain", "OK");
}

// фолбэк: страница отдаёт время браузера (UTC, сек), если у устройства его нет
void handleSetTime() {
  if (server.hasArg("e"))
    applyEpoch((time_t)strtoul(server.arg("e").c_str(), nullptr, 10));
  server.send(200, "text/plain", "OK");
}

void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/hist", handleHist);
  server.on("/export", handleExport);
  server.on("/rgb", handleRgb);
  server.on("/bright", handleBright);
  server.on("/font", handleFont);
  server.on("/cfg", handleCfg);
  server.on("/api/push", handleNodePush);
  server.on("/calibrate", handleCalibrate);
  server.on("/settime", handleSetTime);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/reboot", handleReboot);
  server.on("/wifi/scan", handleWifiScan);
  server.on("/wifi/save", handleWifiSave);
  server.on("/wifi/forget", handleWifiForget);
  server.on("/wifi/status", handleWifiStatus);
  server.on("/manifest.json", handleManifest);
  server.on("/icon.svg", handleIcon);
  server.onNotFound(handleNotFound);
  server.begin();
}
