#pragma once
#include "config.h"
#include "globals.h"

// ============================================================
//   БАЛЛ КЛИМАТА 0..100
// ============================================================
// Порт расчёта с веб-страницы (index.html, renderClim): те же веса, пороги
// и опорные точки, чтобы цвет подсветки и плитка «Климат в комнате»
// показывали одно и то же.
//
// Пороги живут здесь и переживают ребут (Preferences, ключи "thr_<имя>").
// Страница их читает из /data и правит через /cfg — источник правды один,
// так что подсветка LCD и плитка климата всегда считаются по одному набору.

// трёхпороговые параметры: чем больше, тем хуже
float thrCO2[3]  = {800, 1000, 1500};
float thrECO2[3] = {800, 1000, 1500};
float thrTVOC[3] = {220, 660, 2200};
float thrAQI[3]  = {2, 3, 4};
// шестипороговые: три нижние границы, три верхние, норма между 3-й и 4-й
float thrTemp[6] = {16, 18, 20, 24, 26, 28};
float thrHum[6]  = {20, 30, 40, 60, 70, 80};

// одна таблица на всё: хранение, JSON и разбор запроса ходят по ней
struct ThrDef { const char *key; float *v; uint8_t n; };
ThrDef THR[] = {
  {"co2",  thrCO2,  3}, {"temp", thrTemp, 6}, {"hum",  thrHum,  6},
  {"tvoc", thrTVOC, 3}, {"aqi",  thrAQI,  3}, {"eco2", thrECO2, 3},
};
const uint8_t THR_N = sizeof(THR) / sizeof(THR[0]);

void thrLoad() {
  char key[16];
  for (uint8_t i = 0; i < THR_N; i++) {
    snprintf(key, sizeof(key), "thr_%s", THR[i].key);
    size_t need = THR[i].n * sizeof(float);
    if (prefs.getBytesLength(key) == need) prefs.getBytes(key, THR[i].v, need);
  }
}

void thrSave() {
  char key[16];
  for (uint8_t i = 0; i < THR_N; i++) {
    snprintf(key, sizeof(key), "thr_%s", THR[i].key);
    prefs.putBytes(key, THR[i].v, THR[i].n * sizeof(float));
  }
}

// "800,1000,1500" -> n чисел. Набор обязан строго возрастать: равные соседи
// дали бы деление на ноль в segIdx. Кривой ввод просто отвергаем.
bool parseThr(const String &s, float *out, uint8_t n) {
  float tmp[6];
  uint8_t i = 0;
  int start = 0;
  while (true) {
    if (i >= n) return false;               // значений больше, чем ждём
    int c = s.indexOf(',', start);
    tmp[i++] = ((c < 0) ? s.substring(start) : s.substring(start, c)).toFloat();
    if (c < 0) break;
    start = c + 1;
  }
  if (i != n) return false;
  for (uint8_t k = 1; k < n; k++) if (tmp[k] <= tmp[k - 1]) return false;
  memcpy(out, tmp, n * sizeof(float));
  return true;
}

static const float SB[5] = {100, 75, 45, 15, 0};   // подбалл по индексу плохости
static const float HU[4] = {145, 55, 30, 5};       // оттенок HSV: зелёный -> красный

// индекс «плохости» 0..4: 0 — лучшая зона, между порогами линейно
float segIdx(float v, const float *a, uint8_t n) {
  if (v <= a[0]) return 0;
  for (uint8_t k = 1; k < n; k++)
    if (v <= a[k]) return k - 1 + (v - a[k - 1]) / (a[k] - a[k - 1]);
  return fminf(4.0f, (n - 1) + (v - a[n - 1]) / (a[n - 1] - a[n - 2]));
}

// двусторонний параметр (температура, влажность): ниже центра — зеркалим
float segTwoSided(float v, const float *t) {
  if (v < t[2]) {
    const float mirrored[3] = {-t[2], -t[1], -t[0]};
    return segIdx(-v, mirrored, 3);
  }
  return segIdx(v, t + 3, 3);
}

float subScore(float i) {            // индекс плохости -> подбалл
  if (i >= 4) return 0;
  uint8_t k = (uint8_t)i;
  return SB[k] + (SB[k + 1] - SB[k]) * (i - k);
}

float scoreToIdx(float s) {          // обратно: балл -> индекс (нужен для цвета)
  for (uint8_t k = 0; k < 4; k++)
    if (s >= SB[k + 1]) return k + (SB[k] - s) / (SB[k] - SB[k + 1]);
  return 4;
}

float climateHue(float i) {          // оттенок HSV по индексу плохости
  if (i >= 3) return HU[3];
  uint8_t k = (uint8_t)i;
  return HU[k] + (HU[k + 1] - HU[k]) * (i - k);
}

// Балл климата 0..100, или -1 если ни одного живого датчика.
// Взвешенное среднее подбаллов, но не выше «худший + 30»: один провальный
// параметр не должен тонуть в хороших соседях.
// Отвалившиеся датчики просто выпадают из суммы весов.
//
// Значения передаются явно, чтобы тем же расчётом считать точки истории
// (/hist отдаёт серию климата, не храня её). Состав участвующих параметров
// при этом берётся по живости датчиков СЕЙЧАС: что мёртво сегодня, то и в
// прошлом писало мусор.
float climateScoreOf(float t, float h, float co2, float eco2, float tvoc, float aqi) {
  float sw = 0, ss = 0, worst = -1;
  auto add = [&](float w, float idx) {
    sw += w;
    ss += w * subScore(idx);
    if (idx > worst) worst = idx;
  };
  if (mhzLive())      add(0.40f, segIdx(co2,  thrCO2,  3));
  else if (ensLive()) add(0.40f, segIdx(eco2, thrECO2, 3));   // нет MH-Z19C — по eCO2
  if (ahtLive()) {
    add(0.20f, segTwoSided(t, thrTemp));
    add(0.20f, segTwoSided(h, thrHum));
  }
  if (ensLive()) {
    add(0.15f, segIdx(tvoc, thrTVOC, 3));
    add(0.05f, segIdx(aqi,  thrAQI,  3));
  }
  if (sw == 0) return -1;
  return fminf(ss / sw, subScore(worst) + 30);
}

float climateScore() {   // по текущим показаниям
  return climateScoreOf(curTemp, curHum, curCO2, curECO2, curTVOC, curAQI);
}
