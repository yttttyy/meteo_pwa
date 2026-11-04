#pragma once
#include "config.h"
#include "globals.h"
#include "bigdigits.h"
#include "nodes.h"

// ============================================================
//   ОТРИСОВКА: часы, дата, строки датчиков
// ============================================================

void putCell(uint8_t x, uint8_t y, uint8_t code) {
  if (x >= 20 || y >= 4) return;
  lcd.setCursor(x, y);
  lcd.write(code);
}

void drawDig(uint8_t dig, uint8_t x, uint8_t y) {
  if (dig > 9) {   // пустое место
    for (uint8_t i = 0; i < 3; i++) { putCell(x + i, y, ' '); putCell(x + i, y + 1, ' '); }
    return;
  }
  for (uint8_t i = 0; i < 3; i++) putCell(x + i, y,     digSeg[font][dig][i]);
  for (uint8_t i = 0; i < 3; i++) putCell(x + i, y + 1, digSeg[font][dig][i + 3]);
}

// двоеточие как у Гайвера: точка 2x2 по центру знакоместа, в обеих строках
void drawDots(uint8_t x, uint8_t y, bool state) {
  uint8_t code = state ? CH_DOT : 32;
  putCell(x, y,     code);
  putCell(x, y + 1, code);
}

// перерисовываем только те цифры, что изменились — область часов не моргает.
// Часы всегда с ведущим нулём: 02:03, а не 2:03.
void drawClock(uint8_t hours, uint8_t minutes) {
  uint8_t h10 = hours / 10, h1 = hours % 10;
  uint8_t m10 = minutes / 10, m1 = minutes % 10;
  if (h10 != pH10) { drawDig(h10, 0,  0); pH10 = h10; }
  if (h1  != pH1 ) { drawDig(h1,  4,  0); pH1  = h1;  }
  if (m10 != pM10) { drawDig(m10, 8,  0); pM10 = m10; }
  if (m1  != pM1 ) { drawDig(m1,  12, 0); pM1  = m1;  }
}

// дни недели двухбуквенно (tm_wday: 0=Вс). Коды кириллицы из знакогенератора:
// П=0xA8, Ч=0xAB, Б=0xA0; В,С,Т,Р,Н совпадают с латиницей.
static const uint8_t dayNames[7][2] = {
  {'B',  'C' },   // ВС
  {0xA8, 'H' },   // ПН
  {'B',  'T' },   // ВТ
  {'C',  'P' },   // СР
  {0xAB, 'T' },   // ЧТ
  {0xA8, 'T' },   // ПТ
  {'C',  0xA0},   // СБ
};

void drawDate(struct tm &ti) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d.%02d", ti.tm_mday, ti.tm_mon + 1);
  for (uint8_t i = 0; i < 5; i++) putCell(15 + i, 0, buf[i]);
  putCell(16, 1, dayNames[ti.tm_wday][0]);
  putCell(17, 1, dayNames[ti.tm_wday][1]);
  putCell(18, 1, ' ');   // очистить остаток от прежних надписей
  putCell(19, 1, ' ');
}

void printRow(uint8_t row, const char *s) {
  uint8_t i = 0;
  for (; s[i] && i < 20; i++) putCell(i, row, s[i]);
  for (; i < 20; i++) putCell(i, row, ' ');   // добиваем пробелами до 20
}

void printRowCentered(uint8_t row, const char *s) {
  uint8_t len = strlen(s);
  if (len > 20) len = 20;
  uint8_t pad = (20 - len) / 2;
  uint8_t i = 0;
  for (; i < pad; i++) putCell(i, row, ' ');
  for (uint8_t k = 0; k < len; k++, i++) putCell(i, row, s[k]);
  for (; i < 20; i++) putCell(i, row, ' ');
}

// отвалившийся датчик -> черточки вместо значений, подписи остаются.
// lcdView >= 0 -> в строках 2-3 удалённая комната: temp/hum/eCO2 + ИМЯ и AQI
// (часы и дата в строках 0-1 не трогаются)
void drawSensors() {
  if (lcdView >= 0 && lcdView < (int8_t)nodeCount) {
    Node &n = nodes[lcdView];
    bool on = nodeOnline(lcdView);
    char line[32];
    if (on) snprintf(line, sizeof(line), "%.1f%c %d%%  %d ppm", n.t, (char)DEGREE_CHAR, (int)n.h, (int)n.eco2);
    else    snprintf(line, sizeof(line), "--.-%c --%%  --- ppm", (char)DEGREE_CHAR);
    printRow(2, line);
    char aqiS[8], row[21];
    if (on) snprintf(aqiS, sizeof(aqiS), "AQI:%d", (int)n.aqi);
    else    strcpy(aqiS, "AQI:-");
    memset(row, ' ', 20); row[20] = 0;          // имя слева, AQI прижат вправо
    uint8_t al = strlen(aqiS);
    uint8_t nl = strlen(n.nameLcd);
    if (nl > 20 - al - 1) nl = 20 - al - 1;
    memcpy(row, n.nameLcd, nl);
    memcpy(row + 20 - al, aqiS, al);
    printRow(3, row);
    return;
  }
  char line[32], tS[8], hS[6], cS[8], aS[4], vS[8];
  if (ahtLive()) {
    snprintf(tS, sizeof(tS), "%.1f", curTemp);
    snprintf(hS, sizeof(hS), "%d", curHum);
  } else { strcpy(tS, "--.-"); strcpy(hS, "--"); }
  if (mhzLive()) snprintf(cS, sizeof(cS), "%d", curCO2); else strcpy(cS, "---");
  if (ensLive()) {
    snprintf(aS, sizeof(aS), "%d", curAQI);
    snprintf(vS, sizeof(vS), "%d", curTVOC);
  } else { strcpy(aS, "-"); strcpy(vS, "---"); }
  // стрелка тренда CO2 в хвосте строки: своя графика в CGRAM, где слот свободен,
  // иначе ASCII (в скруглённом шрифте слот 3 занят под CB)
  char tr = ' ';
  if (curTrend) {
    loadArrow(curTrend);
    tr = (font == FONT_RECT) ? (char)CH_ARROW : (curTrend > 0 ? '^' : 'v');
  }
  snprintf(line, sizeof(line), "%s%c %s%%  %s ppm%c", tS, (char)DEGREE_CHAR, hS, cS, tr);
  printRow(2, line);
  snprintf(line, sizeof(line), "AQI:%s  TVOC:%s ppb", aS, vS);
  printRow(3, line);
}

// заглушка часов, пока времени нет ниоткуда: большие "-- : --"
void drawNoTime() {
  // LB (нижняя палка) в верхней клетке цифры ≈ черта по центру строки часов
  static const uint8_t xs[4] = {0, 4, 8, 12};
  for (uint8_t d = 0; d < 4; d++)
    for (uint8_t i = 0; i < 3; i++) {
      putCell(xs[d] + i, 0, 4);
      putCell(xs[d] + i, 1, ' ');
    }
  pH10 = pH1 = pM10 = pM1 = 0xFF;   // появится время — цифры перерисуются целиком
}

// полная перерисовка главного экрана (после смены шрифта / выхода из статусов)
void redrawMain() {
  loadFont(font);
  lcd.clear();
  pH10 = pH1 = pM10 = pM1 = 0xFF;   // заставить часы перерисоваться целиком
  time_t nowT = time(nullptr);
  if (nowT > 1600000000) {
    struct tm ti;
    localtime_r(&nowT, &ti);
    drawClock(ti.tm_hour, ti.tm_min);
    drawDate(ti);
    lastMin = ti.tm_min;
    lastDay = ti.tm_mday;
    drawDots(7, 0, dotFlag);
  } else {
    drawNoTime();
    lastMin = -2;   // флаг "нарисована заглушка"
  }
  drawSensors();
}
