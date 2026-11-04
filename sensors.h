#pragma once
#include "config.h"
#include "globals.h"
#include "display.h"
#include "history.h"

// ============================================================
//   ДАТЧИКИ И ИСТОРИЯ
// ============================================================

static inline void bumpFail(uint8_t &f) { if (f < 250) f++; }

void readSensors() {
  sensors_event_t hum, temp;
  if (ahtOk) {
    if (aht.getEvent(&hum, &temp)) {   // false = ошибка I2C
      ahtFail = 0;
      // калибровочные поправки к сырым показаниям AHT21 (настраиваются с веба)
      float t = temp.temperature + tempOffset;
      float h = constrain(hum.relative_humidity + humOffset, 0.0f, 100.0f);
      curTemp = t;
      curHum  = round(h);
      // ENS160 компенсирует TVOC/AQI по температуре и влажности — кормим его
      // именно ИСПРАВЛЕННЫМИ значениями (реальным воздухом), не сырыми
      if (ensLive()) ens.setTempAndHum(t, h);
    } else bumpFail(ahtFail);
  }
  if (ensOk) {
    uint8_t aqi = ens.getAQI();
    if (aqi >= 1 && aqi <= 5) {   // при мёртвом I2C библиотека отдаёт 0 (валидный AQI 1..5)
      ensFail = 0;
      curAQI  = aqi;
      curTVOC = ens.getTVOC();
      curECO2 = ens.getECO2();
    } else bumpFail(ensFail);
  }
  if (mhzOk) {
    int co2 = mhz.getCO2();
    if (mhz.errorCode == RESULT_OK && co2 > 0) {
      mhzFail = 0;
      curCO2 = co2;
    } else bumpFail(mhzFail);
  }
  drawSensors();
}

// --- минутный буфер (последние 3 часа; час = последние 60 точек) ---
uint16_t histIdx(uint16_t i) {   // i-я точка от самой старой
  return (histW - histCount + i + HIST_LEN) % HIST_LEN;
}

// --- суточный буфер: каждая точка = среднее пяти минутных ---
uint16_t hist5Idx(uint16_t i) {
  return (hist5W - hist5Count + i + HIST5_LEN) % HIST5_LEN;
}

// --- недельный буфер: каждая точка = среднее 12 пятиминутных ---
uint16_t histHIdx(uint16_t i) {
  return (histHW - histHCount + i + HISTH_LEN) % HISTH_LEN;
}

// Стрелка тренда: сравниваем свежую точку с тем, что было TREND_BACK минут назад.
// Мёртвая зона TREND_DEAD не даёт стрелке дёргаться на шуме датчика.
// Нет MH-Z19C — тренд считаем по eCO2, нет и его — стрелки нет.
void updateTrend() {
  curTrend = 0;
  if (histCount < TREND_BACK + 1) return;      // ещё не накопили
  const uint16_t *src;
  if (mhzLive())      src = hC;
  else if (ensLive()) src = hE;
  else return;
  int32_t d = (int32_t)src[histIdx(histCount - 1)] - src[histIdx(histCount - 1 - TREND_BACK)];
  if      (d >=  TREND_DEAD) curTrend =  1;
  else if (d <= -TREND_DEAD) curTrend = -1;
}

void pushHistory() {   // раз в минуту
  hT[histW] = (int16_t)(curTemp * 10);
  hH[histW] = curHum;
  hC[histW] = curCO2;
  hA[histW] = curAQI;
  hV[histW] = curTVOC;
  hE[histW] = curECO2;
  histW = (histW + 1) % HIST_LEN;
  if (histCount < HIST_LEN) histCount++;
  updateTrend();
  drawSensors();   // стрелка могла смениться

  acc5T += curTemp; acc5H += curHum; acc5C += curCO2;
  acc5A += curAQI;  acc5V += curTVOC; acc5E += curECO2;
  if (++acc5N >= 5) {
    float t5 = acc5T / 5;
    uint32_t h5 = acc5H / 5, c5 = acc5C / 5, a5 = acc5A / 5, v5 = acc5V / 5, e5 = acc5E / 5;
    h5T[hist5W] = (int16_t)(t5 * 10);
    h5H[hist5W] = h5;
    h5C[hist5W] = c5;
    h5A[hist5W] = a5;
    h5V[hist5W] = v5;
    h5E[hist5W] = e5;
    hist5W = (hist5W + 1) % HIST5_LEN;
    if (hist5Count < HIST5_LEN) hist5Count++;
    acc5T = 0; acc5H = acc5C = acc5A = acc5V = acc5E = 0; acc5N = 0;

    accHT += t5; accHH += h5; accHC += c5; accHA += a5; accHV += v5; accHE += e5;
    if (++accHN >= 12) {   // накопился час
      hHT[histHW] = (int16_t)(accHT / 12 * 10);
      hHH[histHW] = accHH / 12;
      hHC[histHW] = accHC / 12;
      hHA[histHW] = accHA / 12;
      hHV[histHW] = accHV / 12;
      hHE[histHW] = accHE / 12;
      histHW = (histHW + 1) % HISTH_LEN;
      if (histHCount < HISTH_LEN) histHCount++;
      accHT = 0; accHH = accHC = accHA = accHV = accHE = 0; accHN = 0;
    }
    saveHistory();   // раз в 5 минут скидываем историю на флеш
  }
}
