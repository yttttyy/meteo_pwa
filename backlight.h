#pragma once
#include "config.h"
#include "globals.h"
#include "climate.h"

// ============================================================
//   RGB ПОДСВЕТКА + АВТО-ЯРКОСТЬ ПО ФОТОРЕЗИСТОРУ
// ============================================================

void setRGB(uint8_t r, uint8_t g, uint8_t b) {   // curBright масштабирует все три канала
  ledcWrite(PIN_R, (uint16_t)r * curBright / 255);
  ledcWrite(PIN_G, (uint16_t)g * curBright / 255);
  ledcWrite(PIN_B, (uint16_t)b * curBright / 255);
}

void applyManual() { if (cMode == CMODE_MANUAL) setRGB(rgbR, rgbG, rgbB); }  // в остальных режимах цвет ведёт backlightTick

// HSV (h 0..360, s/v 0..1) -> RGB 0..255, для режима радуги
void hsv2rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  float c = v * s;
  float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
  float m = v - c, rr = 0, gg = 0, bb = 0;
  if      (h <  60) { rr = c; gg = x; }
  else if (h < 120) { rr = x; gg = c; }
  else if (h < 180) { gg = c; bb = x; }
  else if (h < 240) { gg = x; bb = c; }
  else if (h < 300) { rr = x; bb = c; }
  else              { rr = c; bb = x; }
  r = (uint8_t)((rr + m) * 255);
  g = (uint8_t)((gg + m) * 255);
  b = (uint8_t)((bb + m) * 255);
}

// целевой цвет по баллу климата: зелёный -> жёлтый -> оранжевый -> красный.
// Оттенок берём тот же, которым страница красит плитку «Климат в комнате»,
// так что станция и веб показывают одно состояние одним цветом.
// false = ни одного живого датчика, цвет трогать нечем.
bool climateTarget(float &r, float &g, float &b) {
  float sc = climateScore();
  if (sc < 0) return false;
  uint8_t rr, gg, bb;
  hsv2rgb(climateHue(scoreToIdx(sc)), 1.0f, 1.0f, rr, gg, bb);
  r = rr; g = gg; b = bb;
  return true;
}

void backlightTick() {   // вызывается каждые 50 мс
  if (cMode == CMODE_MANUAL) return;
  // тревога CO2 перебивает авто и радугу (ручной цвет не трогаем — раз человек
  // выставил сам, пусть держит; но проветрить всё равно стоит)
  if (curCO2 >= co2Alarm) {      // пульсация тускло-красный <-> ярко-красный
    float k = 0.7f + 0.3f * sinf(millis() / 120.0f);   // ~1.3 Гц, диапазон 0.4..1.0 (не до нуля)
    setRGB((uint8_t)(255 * k), 0, 0);
    return;
  }
  if (cMode == CMODE_RAINBOW) {  // плавное переливание по кругу оттенков
    rainbowHue += rainbowSpd * 0.05f;   // spd градусов в секунду (тик = 50 мс)
    if (rainbowHue >= 360) rainbowHue -= 360;
    uint8_t r, g, b;
    hsv2rgb(rainbowHue, 1.0f, 1.0f, r, g, b);
    setRGB(r, g, b);
    return;
  }
  float tr, tg, tb;              // CMODE_AUTO: цвет по общему баллу климата
  if (!climateTarget(tr, tg, tb)) return;
  smR += (tr - smR) * 0.02f;   // плавно тянем цвет к целевому (~2-3 c)
  smG += (tg - smG) * 0.02f;
  smB += (tb - smB) * 0.02f;
  setRGB((uint8_t)smR, (uint8_t)smG, (uint8_t)smB);
}

// замер фоторезистора каждые LDR_PERIOD мс.
// Делитель LDR->3.3V / 10к->GND: светло = ADC высокий, темно = низкий.
void ldrTick() {
  ldrRaw = analogRead(PIN_LDR);                     // 0..4095
  ldrSm += (ldrRaw - ldrSm) * LDR_SMOOTH;           // EMA, чтобы яркость не дёргалась
  if (!brightAuto) { curBright = bright; return; }
  int b = map((int)ldrSm, 0, 4095, BRIGHT_MIN, BRIGHT_MAX);
  curBright = constrain(b, BRIGHT_MIN, BRIGHT_MAX);
  applyManual();                                    // в ручном цвете применяем яркость сразу
}
