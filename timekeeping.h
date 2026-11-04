#pragma once
#include "config.h"
#include "globals.h"
#include "display.h"

// ============================================================
//   ВРЕМЯ: DS3231 (опционально) + NTP + браузерный фолбэк
// ============================================================
// Логика:
// - на загрузке: если DS3231 подключён и держит валидное время — поднимаем мгновенно (без сети)
// - есть сеть: NTP синхронит системные часы и пишет обратно в RTC (onNtpSync)
// - времени нет вообще: /data отдаёт timeok:false, страница шлёт время браузера в /settime
// Физический RTC НЕ обязателен: без модуля rtcOk=false и всё работает от NTP/браузера
// (но время не переживёт ребут без сети).

bool timeValid() { return time(nullptr) > 1600000000; }

void clockTick() {   // каждые 500 мс
  dotFlag = !dotFlag;
  if (!timeValid()) {
    if (lastMin != -2) {   // времени нет ниоткуда — большие "-- : --" (один раз)
      drawNoTime();
      lastMin = -2;
    }
    drawDots(7, 0, dotFlag);   // двоеточие мигает — видно, что станция жива
    return;
  }
  struct tm ti;
  time_t nowT = time(nullptr);
  localtime_r(&nowT, &ti);
  if (ti.tm_min != lastMin) {
    lastMin = ti.tm_min;
    drawClock(ti.tm_hour, ti.tm_min);
    if (ti.tm_mday != lastDay) lastDay = ti.tm_mday;
    drawDate(ti);
  }
  drawDots(7, 0, dotFlag);
}

// NTP синхронизировался -> сохраняем точное время (UTC) в DS3231, если он есть
void onNtpSync(struct timeval *tv) {
  if (rtcOk) rtc.adjust(DateTime((uint32_t)tv->tv_sec));
}

// применить время, пришедшее извне (браузер через /settime), сек UTC
void applyEpoch(time_t e) {
  if (e <= 1600000000) return;
  struct timeval tv = { e, 0 };
  settimeofday(&tv, nullptr);
  if (rtcOk) rtc.adjust(DateTime((uint32_t)e));
}

// на загрузке: поднять системные часы из DS3231, если он есть и не терял питание
void initTimeFromRTC() {
  rtcOk = rtc.begin();
  if (rtcOk && !rtc.lostPower()) {
    struct timeval tv = { (time_t)rtc.now().unixtime(), 0 };
    settimeofday(&tv, nullptr);
  }
}
