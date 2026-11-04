#pragma once

// ============================================================
//   НАСТРОЙКИ (все #define в одном месте)
// ============================================================

#define FW_VERSION  "2.7.0"

// --- Время / сеть ---
#define TZ_INFO     "EET-2EEST,M3.5.0/3,M10.5.0/4"   // UTC+2, летом UTC+3
#define MDNS_NAME   "meteo"                          // http://meteo.local

// WiFi-креды хранятся в Preferences (настройка через точку доступа),
// в коде их больше нет.
#define AP_SSID             "Meteo-Setup"   // открытая AP для первоначальной настройки
#define WIFI_CONNECT_MS     20000UL         // сколько ждём подключения к сохранённой сети
#define WIFI_RETRY_MS       30000UL         // период ретраев STA в рабочем режиме
#define WIFI_AP_FALLBACK_MS 180000UL        // офлайн дольше 3 мин -> поднимаем AP параллельно STA

// --- Пины ---
// ⚠️ GPIO43/44 = U0TXD/U0RXD (загрузочный UART) — НЕЛЬЗЯ вешать UART-периферию:
// мешает прошивке ("Download timeout!", "CCCCCC"). MH-Z19C поэтому на 15/16.
#define PIN_SDA     17
#define PIN_SCL     18
#define PIN_R       5      // базы BC547 через 1 кОм
#define PIN_G       4
#define PIN_B       6
#define MHZ_RX      16     // RX ESP <- TX датчика  (НЕ 44!)
#define MHZ_TX      15     // TX ESP -> RX датчика  (НЕ 43!)
#define PIN_LDR     7      // фоторезистор: ADC1_CH6 (ADC2 при WiFi не работает).
                           // Делитель: LDR от 3.3V к пину, ~10 кОм от пина на GND.
#define PIN_TOUCH   8      // сенсорная кнопка TTP223: VCC->3.3V, GND, OUT->GPIO8 (активный HIGH)

#define LCD_ADDR    0x27

// --- CO2 / подсветка ---
// Пороги и веса градиента живут в climate.h (балл климата) — подсветка идёт по нему.
#define CO2_ALARM    1500  // ppm: дефолт порога тревоги (настраивается с веба, prefs "co2al")

// режимы цвета подсветки
#define CMODE_AUTO    0    // цвет по общему баллу климата
#define CMODE_MANUAL  1    // свой цвет
#define CMODE_RAINBOW 2    // плавное переливание по кругу HSV

// --- Авто-яркость по фоторезистору ---
#define LDR_PERIOD   200UL   // мс между замерами ADC
#define LDR_SMOOTH   0.05f   // коэффициент EMA (чем меньше, тем плавнее)
#define BRIGHT_MIN   10      // яркость в полной темноте (0 гасил бы подсветку совсем)
#define BRIGHT_MAX   255

// --- Датчики / история ---
// --- тренд CO2 (стрелка на LCD и на плитке) ---
#define TREND_BACK   10        // с чем сравниваем: столько минутных точек назад
#define TREND_DEAD   50        // ppm: мёртвая зона, чтобы стрелка не дёргалась на шуме

#define SENS_PERIOD  15000UL   // опрос датчиков
#define SENS_FAIL_N  3         // столько подряд неудачных чтений = датчик отвалился (~45с)
#define ADDR_SCREEN_S 5        // экран "адрес панели управления" при старте, секунд

// --- внешние узлы-сателлиты (Meteo-Node, /api/push) ---
#define NODE_MAX        4          // максимум узлов
#define NODE_TIMEOUT_MS 90000UL    // нет пуша дольше -> узел офлайн (пуш раз в 20с)
#define VIEW_RETURN_MS  30000UL    // автовозврат LCD с комнаты на домашний экран
#define TOUCH_DEBOUNCE  250UL      // антидребезг сенсорной кнопки, мс
#define HIST_PERIOD  60000UL   // запись минутной истории
#define HIST_LEN     180       // 180 точек по минуте = 3 часа (и последние 60 = час)
#define HIST5_LEN    288       // 288 точек по 5 минут = сутки
#define HISTH_LEN    168       // 168 точек по часу = неделя
// история сохраняется в LittleFS (/hist.bin) раз в 5 минут и перед ребутом/OTA —
// переживает перезагрузку. Разрыв больше суток -> файл считается устаревшим.
#define HIST_MAX_GAP_S (24UL * 3600)

// Калибровка AHT21 (сверено с эталоном). Поправки аддитивные:
// температура -3.9°C, влажность +13 процентных пунктов (40% -> 53%).
// Эти же исправленные значения кормятся в ENS160 для компенсации TVOC/AQI.
// Это ДЕФОЛТЫ: рабочие значения лежат в Preferences и правятся с веб-страницы.
#define TEMP_OFFSET  -3.9f   // °C
#define HUM_OFFSET   13.0f   // процентные пункты

// --- Слоты CGRAM / шрифты больших цифр ---
#define CH_DOT      2     // слот точки двоеточия
#define CH_ARROW    3     // слот стрелки тренда (свободен только в шрифте «палки»)
#define DEGREE_CHAR 5     // слот градуса
#define FONT_RECT   0
#define FONT_ROUND  1
