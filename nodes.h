#pragma once
#include "config.h"
#include "globals.h"

// ============================================================
//   ВНЕШНИЕ УЗЛЫ-САТЕЛЛИТЫ (Meteo-Node)
// ============================================================
// Узел сам шлёт GET /api/push?id&name&t&h&aqi&tvoc&eco2 раз в ~20с.
// Регистрация автоматическая при первом пуше. Пуша нет 90с -> офлайн.

struct Node {
  uint32_t id = 0;          // чип-ID узла
  char     nameUtf[24] = ""; // имя комнаты (UTF-8, для веба)
  char     nameLcd[13] = ""; // имя кодами знакогенератора (для LCD, до 12 симв.)
  float    t = 0;
  int16_t  h = 0, aqi = 0, tvoc = 0, eco2 = 0;
  uint32_t lastMs = 0;      // millis последнего пуша
};

Node    nodes[NODE_MAX];
uint8_t nodeCount = 0;

bool nodeOnline(uint8_t i) {
  return i < nodeCount && millis() - nodes[i].lastMs < NODE_TIMEOUT_MS;
}

int findOrAddNode(uint32_t id) {
  for (uint8_t i = 0; i < nodeCount; i++)
    if (nodes[i].id == id) return i;
  if (nodeCount >= NODE_MAX) return -1;
  nodes[nodeCount].id = id;
  return nodeCount++;
}

// UTF-8 кириллица -> коды знакогенератора KS0066 RUS (для LCD).
// Строчные приводятся к заглавным (у ROM нет нижнего регистра кириллицы),
// совпадающие с латиницей буквы пишутся ASCII-символами, ASCII проходит как есть.
uint8_t cyrToLcd(uint16_t u) {   // u = юникод-код буквы
  if (u >= 0x0430 && u <= 0x044F) u -= 0x20;   // а-я -> А-Я
  if (u == 0x0451) u = 0x0401;                 // ё -> Ё
  switch (u) {
    case 0x0410: return 'A';   // А
    case 0x0411: return 0xA0;  // Б
    case 0x0412: return 'B';   // В
    case 0x0413: return 0xA1;  // Г
    case 0x0414: return 0xE0;  // Д
    case 0x0415: return 'E';   // Е
    case 0x0401: return 0xA2;  // Ё
    case 0x0416: return 0xA3;  // Ж
    case 0x0417: return 0xA4;  // З
    case 0x0418: return 0xA5;  // И
    case 0x0419: return 0xA6;  // Й
    case 0x041A: return 'K';   // К
    case 0x041B: return 0xA7;  // Л
    case 0x041C: return 'M';   // М
    case 0x041D: return 'H';   // Н
    case 0x041E: return 'O';   // О
    case 0x041F: return 0xA8;  // П
    case 0x0420: return 'P';   // Р
    case 0x0421: return 'C';   // С
    case 0x0422: return 'T';   // Т
    case 0x0423: return 0xA9;  // У
    case 0x0424: return 0xAA;  // Ф
    case 0x0425: return 'X';   // Х
    case 0x0426: return 0xE1;  // Ц
    case 0x0427: return 0xAB;  // Ч
    case 0x0428: return 0xAC;  // Ш
    case 0x0429: return 0xE2;  // Щ
    case 0x042A: return 0xE3;  // Ъ
    case 0x042B: return 0xAE;  // Ы
    case 0x042C: return 0xB4;  // Ь
    case 0x042D: return 0xAF;  // Э
    case 0x042E: return 0xB0;  // Ю
    case 0x042F: return 0xB1;  // Я
    default:     return '?';
  }
}

// src — UTF-8 строка, dst — буфер под коды LCD (maxLen символов + ноль)
void utf8ToLcd(const char *src, char *dst, uint8_t maxLen) {
  uint8_t o = 0;
  for (const uint8_t *p = (const uint8_t*)src; *p && o < maxLen;) {
    if (*p < 0x80) {                        // ASCII
      dst[o++] = toupper(*p);
      p++;
    } else if ((*p & 0xE0) == 0xC0 && p[1]) {   // двухбайтовый UTF-8 (вся кириллица)
      uint16_t u = ((uint16_t)(*p & 0x1F) << 6) | (p[1] & 0x3F);
      dst[o++] = cyrToLcd(u);
      p += 2;
    } else p++;                             // прочий юникод пропускаем
  }
  dst[o] = 0;
}
