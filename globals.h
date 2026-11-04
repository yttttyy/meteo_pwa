#pragma once
#include "config.h"

// ============================================================
//   ОБЪЕКТЫ И СОСТОЯНИЕ
// ============================================================

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);
Adafruit_AHTX0    aht;
DFRobot_ENS160_I2C ens(&Wire, 0x53);
HardwareSerial    mhzSerial(1);
MHZ19             mhz;
WebServer         server(80);
DNSServer         dnsServer;
Preferences       prefs;
RTC_DS3231        rtc;

// --- датчики: текущие значения ---
float curTemp = 0;
int   curHum  = 0;
int   curCO2  = 400;
int   curAQI  = 0;
int   curTVOC = 0;
int   curECO2 = 400;
int8_t curTrend = 0;   // тренд CO2: -1 падает, 0 ровно, +1 растёт (считается раз в минуту)
bool  ahtOk = false, ensOk = false, mhzOk = false, rtcOk = false;   // найден при старте
uint8_t ahtFail = 0, ensFail = 0, mhzFail = 0;   // подряд неудачных чтений (детект отвала)
// "жив" = найден при старте И не превышен лимит подряд неудачных чтений (~45с).
// Вернувшийся I2C-датчик оживает сам, повторная инициализация не нужна.
bool ahtLive() { return ahtOk && ahtFail < SENS_FAIL_N; }
bool ensLive() { return ensOk && ensFail < SENS_FAIL_N; }
bool mhzLive() { return mhzOk && mhzFail < SENS_FAIL_N; }

// --- подсветка ---
uint8_t cMode = CMODE_AUTO;          // режим цвета: авто по CO2 / ручной / радуга
uint8_t rgbR = 80, rgbG = 160, rgbB = 255;
uint8_t rainbowSpd = 12;             // скорость радуги 1..50 (градусов hue в секунду)
float   rainbowHue = 0;
int     co2Alarm = CO2_ALARM;        // порог тревоги, настраивается с веба
bool    brightAuto = false;          // авто-яркость по фоторезистору / слайдер
uint8_t bright = 255;                // ручная яркость (слайдер)
uint8_t curBright = 255;             // действующая яркость (в авто ведёт LDR)
float   ldrSm = 2048;                // сглаженное значение ADC (EMA)
int     ldrRaw = 0;                  // последний сырой замер (для веба)
float   smR = 40, smG = 40, smB = 60; // сглаженный цвет авто-режима

// --- экран ---
uint8_t font = FONT_RECT;            // шрифт цифр: 0 - палки, 1 - скруглённый
int8_t   lcdView = -1;               // что в строках 2-3: -1 дом, 0..3 узел-сателлит
uint32_t viewSinceMs = 0;            // когда переключились (для автовозврата)
bool     touchWas = false;           // прошлое состояние сенсорной кнопки
uint32_t touchLastMs = 0;            // антидребезг
bool    dotFlag = false;
int     lastMin = -1, lastDay = -1;
uint8_t pH10 = 0xFF, pH1 = 0xFF, pM10 = 0xFF, pM1 = 0xFF;  // прошлые цифры (частичная перерисовка)

// --- история: минутный буфер (3 часа) ---
int16_t  hT[HIST_LEN];               // температура x10
uint8_t  hH[HIST_LEN];
uint16_t hC[HIST_LEN];
uint8_t  hA[HIST_LEN];
uint16_t hV[HIST_LEN];
uint16_t hE[HIST_LEN];               // eCO2 от ENS160 — пишем, чтобы сверять с MH-Z19C
uint16_t histCount = 0, histW = 0;

// --- история: буфер 5-минутных средних (сутки) ---
int16_t  h5T[HIST5_LEN];
uint8_t  h5H[HIST5_LEN];
uint16_t h5C[HIST5_LEN];
uint8_t  h5A[HIST5_LEN];
uint16_t h5V[HIST5_LEN];
uint16_t h5E[HIST5_LEN];
uint16_t hist5Count = 0, hist5W = 0;
// аккумулятор для усреднения пяти минутных точек
float    acc5T = 0; uint32_t acc5H = 0, acc5C = 0, acc5A = 0, acc5V = 0, acc5E = 0;
uint8_t  acc5N = 0;

// --- история: буфер часовых средних (неделя) ---
int16_t  hHT[HISTH_LEN];
uint8_t  hHH[HISTH_LEN];
uint16_t hHC[HISTH_LEN];
uint8_t  hHA[HISTH_LEN];
uint16_t hHV[HISTH_LEN];
uint16_t hHE[HISTH_LEN];
uint16_t histHCount = 0, histHW = 0;
// аккумулятор часа: 12 пятиминутных точек
float    accHT = 0; uint32_t accHH = 0, accHC = 0, accHA = 0, accHV = 0, accHE = 0;
uint8_t  accHN = 0;

// --- калибровка (рабочие значения; дефолты из config.h) ---
float tempOffset = TEMP_OFFSET;
float humOffset  = HUM_OFFSET;

// --- WiFi ---
String   wifiSsid, wifiPass;         // сохранённая сеть (Preferences)
bool     apActive = false;           // поднята ли точка доступа Meteo-Setup
uint32_t lastOnlineMs = 0;           // millis последнего момента онлайна (для AP-фолбэка)
