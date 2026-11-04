#pragma once
#include "config.h"
#include "globals.h"

// ============================================================
//   ИСТОРИЯ, ПЕРЕЖИВАЮЩАЯ РЕБУТ (LittleFS, файл /hist.bin)
// ============================================================
// Пишется раз в 5 минут (вместе с 5-минутной точкой) и перед ребутом/OTA.
// Износ флеша: ~105к записей в год по ~6КБ на wear-leveled разделе 1.5МБ —
// хватит на десятилетия. Битый/устаревший (>24ч) файл просто игнорируется.

#define HIST_MAGIC 0x4D483033UL   // "MH03" — версия формата (в MH03 добавлен eCO2)

bool fsOk = false;

// суммарный размер полезной нагрузки (для проверки целостности файла)
size_t histFileSize() {
  return 4 + 4                                  // magic + epoch
       + 6 * 2                                  // счётчики/индексы
       + HIST_LEN  * 10 + HIST5_LEN * 10 + HISTH_LEN * 10   // 6 серий: T2+H1+C2+A1+V2+E2
       + 4 + 5*4 + 1                            // acc5T + acc5H..E + acc5N
       + 4 + 5*4 + 1;                           // accHT + accHH..E + accHN
}

void saveHistory() {
  if (!fsOk) return;
  File f = LittleFS.open("/hist.bin", "w");
  if (!f) return;
  uint32_t magic = HIST_MAGIC;
  uint32_t epoch = (uint32_t)time(nullptr);
  f.write((uint8_t*)&magic, 4);
  f.write((uint8_t*)&epoch, 4);
  f.write((uint8_t*)&histCount,  2); f.write((uint8_t*)&histW,  2);
  f.write((uint8_t*)&hist5Count, 2); f.write((uint8_t*)&hist5W, 2);
  f.write((uint8_t*)&histHCount, 2); f.write((uint8_t*)&histHW, 2);
  f.write((uint8_t*)hT, sizeof(hT)); f.write((uint8_t*)hH, sizeof(hH));
  f.write((uint8_t*)hC, sizeof(hC)); f.write((uint8_t*)hA, sizeof(hA));
  f.write((uint8_t*)hV, sizeof(hV));   f.write((uint8_t*)hE, sizeof(hE));
  f.write((uint8_t*)h5T, sizeof(h5T)); f.write((uint8_t*)h5H, sizeof(h5H));
  f.write((uint8_t*)h5C, sizeof(h5C)); f.write((uint8_t*)h5A, sizeof(h5A));
  f.write((uint8_t*)h5V, sizeof(h5V)); f.write((uint8_t*)h5E, sizeof(h5E));
  f.write((uint8_t*)hHT, sizeof(hHT)); f.write((uint8_t*)hHH, sizeof(hHH));
  f.write((uint8_t*)hHC, sizeof(hHC)); f.write((uint8_t*)hHA, sizeof(hHA));
  f.write((uint8_t*)hHV, sizeof(hHV)); f.write((uint8_t*)hHE, sizeof(hHE));
  f.write((uint8_t*)&acc5T, 4);
  f.write((uint8_t*)&acc5H, 4); f.write((uint8_t*)&acc5C, 4);
  f.write((uint8_t*)&acc5A, 4); f.write((uint8_t*)&acc5V, 4);
  f.write((uint8_t*)&acc5E, 4);
  f.write((uint8_t*)&acc5N, 1);
  f.write((uint8_t*)&accHT, 4);
  f.write((uint8_t*)&accHH, 4); f.write((uint8_t*)&accHC, 4);
  f.write((uint8_t*)&accHA, 4); f.write((uint8_t*)&accHV, 4);
  f.write((uint8_t*)&accHE, 4);
  f.write((uint8_t*)&accHN, 1);
  f.close();
}

void loadHistory() {
  if (!fsOk || !LittleFS.exists("/hist.bin")) return;
  File f = LittleFS.open("/hist.bin", "r");
  if (!f) return;
  if ((size_t)f.size() != histFileSize()) { f.close(); return; }   // другой формат/битый
  uint32_t magic = 0, epoch = 0;
  f.read((uint8_t*)&magic, 4);
  f.read((uint8_t*)&epoch, 4);
  if (magic != HIST_MAGIC) { f.close(); return; }
  time_t now = time(nullptr);
  // если оба времени валидны и станция пролежала выключенной больше суток —
  // данные устарели; если текущего времени ещё нет, восстанавливаем как есть
  if (now > 1600000000 && epoch > 1600000000 && (uint32_t)now - epoch > HIST_MAX_GAP_S) {
    f.close();
    return;
  }
  f.read((uint8_t*)&histCount,  2); f.read((uint8_t*)&histW,  2);
  f.read((uint8_t*)&hist5Count, 2); f.read((uint8_t*)&hist5W, 2);
  f.read((uint8_t*)&histHCount, 2); f.read((uint8_t*)&histHW, 2);
  f.read((uint8_t*)hT, sizeof(hT)); f.read((uint8_t*)hH, sizeof(hH));
  f.read((uint8_t*)hC, sizeof(hC)); f.read((uint8_t*)hA, sizeof(hA));
  f.read((uint8_t*)hV, sizeof(hV));   f.read((uint8_t*)hE, sizeof(hE));
  f.read((uint8_t*)h5T, sizeof(h5T)); f.read((uint8_t*)h5H, sizeof(h5H));
  f.read((uint8_t*)h5C, sizeof(h5C)); f.read((uint8_t*)h5A, sizeof(h5A));
  f.read((uint8_t*)h5V, sizeof(h5V)); f.read((uint8_t*)h5E, sizeof(h5E));
  f.read((uint8_t*)hHT, sizeof(hHT)); f.read((uint8_t*)hHH, sizeof(hHH));
  f.read((uint8_t*)hHC, sizeof(hHC)); f.read((uint8_t*)hHA, sizeof(hHA));
  f.read((uint8_t*)hHV, sizeof(hHV)); f.read((uint8_t*)hHE, sizeof(hHE));
  f.read((uint8_t*)&acc5T, 4);
  f.read((uint8_t*)&acc5H, 4); f.read((uint8_t*)&acc5C, 4);
  f.read((uint8_t*)&acc5A, 4); f.read((uint8_t*)&acc5V, 4);
  f.read((uint8_t*)&acc5E, 4);
  f.read((uint8_t*)&acc5N, 1);
  f.read((uint8_t*)&accHT, 4);
  f.read((uint8_t*)&accHH, 4); f.read((uint8_t*)&accHC, 4);
  f.read((uint8_t*)&accHA, 4); f.read((uint8_t*)&accHV, 4);
  f.read((uint8_t*)&accHE, 4);
  f.read((uint8_t*)&accHN, 1);
  f.close();
  // подстраховка от мусора в индексах
  if (histW  >= HIST_LEN  || histCount  > HIST_LEN ||
      hist5W >= HIST5_LEN || hist5Count > HIST5_LEN ||
      histHW >= HISTH_LEN || histHCount > HISTH_LEN) {
    histCount = histW = hist5Count = hist5W = histHCount = histHW = 0;
    return;
  }
  Serial.printf("[hist] восстановлено: %u мин, %u x5мин, %u час. точек\n",
                histCount, hist5Count, histHCount);
}

void historyInit() {
  fsOk = LittleFS.begin(true);   // true = форматнуть при первом использовании
  if (!fsOk) { Serial.println("[hist] LittleFS недоступна"); return; }
  loadHistory();
}
