/*
  Дампер знакогенератора LCD2004 (HD44780/KS0066).
  Показывает все коды 0x00..0xFF страницами по 64 символа.
  Каждая строка: слева HEX старшего полубайта, дальше 16 символов
  (младший полубайт 0..F слева направо).

  Пример чтения: буква в строке "Ax" на 5-й позиции (lo=5) -> код 0xA5.

  Пины I2C и адрес — как в основном проекте. Поправь при необходимости.
*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_SDA  17
#define PIN_SCL  18
#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();        // если ругнётся — замени на lcd.begin()
  lcd.backlight();
}

void loop() {
  for (uint8_t page = 0; page < 4; page++) {   // 4 страницы по 4 строки
    lcd.clear();
    for (uint8_t row = 0; row < 4; row++) {
      uint8_t hi = page * 4 + row;             // старший полубайт 0..F
      lcd.setCursor(0, row);
      lcd.print(hi, HEX);
      lcd.print("x");
      for (uint8_t lo = 0; lo < 16; lo++) {    // младший полубайт 0..F
        lcd.setCursor(3 + lo, row);
        lcd.write((uint8_t)((hi << 4) | lo));  // сам символ с этим кодом
      }
    }
    delay(5000);     // 5 секунд на страницу — успеть сфотать
  }
}
