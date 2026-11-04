/*
  Сканер шины I2C — диагностика при сборке новой станции.
  Выводит в монитор порта (115200) все адреса, которые отвечают.

  Настройки IDE: плата "ESP32S3 Dev Module", USB CDC On Boot: Enabled.

  Чего ждать:
    0x27 — LCD2004 (PCF8574)   | 0x3F если на переходнике PCF8574A
    0x53 — ENS160              | 0x52 если перемычка ADDR в другом положении
    0x38 — AHT21
    0x68 — DS3231
  Пусто на всех адресах = шина не поднята: питание, земля или перепутаны SDA/SCL.
*/
#include <Wire.h>

#define PIN_SDA 17
#define PIN_SCL 18

void setup() {
  Serial.begin(115200);
  delay(2000);            // дать USB CDC подняться, иначе первые строки теряются
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.printf("\nI2C scan: SDA=%d SCL=%d\n", PIN_SDA, PIN_SCL);
}

void loop() {
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {   // 0 = устройство ответило ACK
      Serial.printf("  найдено: 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  ничего не найдено — проверь питание, землю и SDA/SCL");
  else        Serial.printf("  всего устройств: %u\n", found);
  Serial.println("---");
  delay(3000);
}
