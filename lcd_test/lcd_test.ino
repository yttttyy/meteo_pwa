/*
  Тест LCD отдельно от прошивки.

  Инициализирует экран в цикле раз в 3 секунды и сразу поднимает RGB-подсветку
  на полную, чтобы негативный экран было видно без телефона.

  Как читать результат:
    - текст появился сразу          -> с экраном всё хорошо, дело было в прошивке
    - появился с 2-3-й попытки      -> тайминги: контроллеру мало времени после
                                       подъёма питания, в основной код нужен delay
    - не появляется никогда         -> разводка переходника PCF8574 <-> гребёнка LCD
                                       (RS/E/R-W или D4..D7), либо другая раскладка битов
    - текст есть, подсветка темная  -> цоколёвка BC547 (плоской стороной к себе: C B E)

  Настройки IDE: плата "ESP32S3 Dev Module", USB CDC On Boot: Enabled.
*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_SDA  17
#define PIN_SCL  18
#define PIN_R    5
#define PIN_G    4
#define PIN_B    6
#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

void setup() {
  Serial.begin(115200);

  ledcAttach(PIN_R, 5000, 8);
  ledcAttach(PIN_G, 5000, 8);
  ledcAttach(PIN_B, 5000, 8);
  ledcWrite(PIN_R, 255);   // белый на полную: заодно проверка транзисторов
  ledcWrite(PIN_G, 255);
  ledcWrite(PIN_B, 255);

  delay(1000);             // даём контроллеру LCD спокойно выйти из внутреннего сброса
  Wire.begin(PIN_SDA, PIN_SCL);
}

uint32_t n = 0;

void loop() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("STROKA 1 ABCDEFGHIJ");
  lcd.setCursor(0, 1); lcd.print("STROKA 2 0123456789");
  lcd.setCursor(0, 2); lcd.print("STROKA 3 ##########");
  lcd.setCursor(0, 3); lcd.print("init #"); lcd.print(n);
  Serial.printf("init #%lu\n", n++);
  delay(3000);
}
