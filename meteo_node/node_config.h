#pragma once

// ============================================================
//   METEO-NODE: настройки узла-сателлита
// ============================================================
// Плата: ESP32-S3 Dev Module (как основная станция), USB CDC On Boot: Enabled
// Датчики: AHT21 (0x38) + ENS160 (0x53) по I2C

#define NODE_FW      "1.0.0"

#define PIN_SDA      17
#define PIN_SCL      18

#define AP_SSID      "Meteo-Node"    // открытая AP первичной настройки (captive portal)
#define MDNS_NODE    "meteonode"     // http://meteonode.local — своя страничка узла

#define WIFI_CONNECT_MS     20000UL
#define WIFI_RETRY_MS       30000UL
#define AP_FALLBACK_MS      180000UL  // офлайн дольше 3 мин -> поднять AP параллельно

#define SENS_PERIOD  15000UL   // опрос датчиков
#define PUSH_PERIOD  20000UL   // отправка на станцию
#define PUSH_FAILS_RERESOLVE 3 // столько неудачных пушей подряд -> перерезолвить mDNS

// дефолты первичной настройки (меняются через портал/страницу узла)
#define DEFAULT_STATION "meteo.local"
#define DEFAULT_ROOM    "Комната"

// калибровка ЭТОГО датчика (не станции!) — сверить с эталоном отдельно
#define NODE_TEMP_OFFSET 0.0f   // °C
#define NODE_HUM_OFFSET  0.0f   // процентные пункты
