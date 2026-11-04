#!/usr/bin/env python3
"""
Собирает web/index.html обратно в прошивку (web_assets.h -> INDEX_HTML_GZ).

Запуск из папки проекта:   python build_web.py

Что делает:
  * вырезает dev-блоки <!--DEV--> ... <!--/DEV--> (заглушка с фейковыми данными)
  * сжимает страницу gzip'ом и кладёт байтами в web_assets.h вместо старого INDEX_HTML_GZ
    (handleRoot отдаёт её с Content-Encoding: gzip, браузер распаковывает сам)
  * трогает ТОЛЬКО INDEX_HTML_GZ; MANIFEST_JSON и ICON_SVG остаются как есть
После этого — обычная сборка в Arduino IDE и заливка по OTA.
"""
import gzip
import re
import sys
import pathlib

sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # консоль Windows по умолчанию cp1252

ROOT = pathlib.Path(__file__).parent
SRC = ROOT / "web" / "index.html"
DST = ROOT / "web_assets.h"

html = SRC.read_text(encoding="utf-8")

# 1. вырезаем всё, что только для разработки
html = re.sub(r"[ \t]*<!--DEV-->.*?<!--/DEV-->[ \t]*\n?", "", html, flags=re.S)
raw = html.rstrip("\n").encode("utf-8")

# 2. gzip с mtime=0 — одинаковый вход даёт одинаковые байты, без лишних диффов
gz = gzip.compress(raw, compresslevel=9, mtime=0)
rows = ",\n".join(",".join(f"0x{b:02x}" for b in gz[i:i + 16]) for i in range(0, len(gz), 16))
block = (f"// {len(raw)} байт HTML -> {len(gz)} байт gzip\n"
         f"const uint8_t INDEX_HTML_GZ[] PROGMEM = {{\n{rows}\n}};\n"
         f"const size_t INDEX_HTML_GZ_LEN = sizeof(INDEX_HTML_GZ);")

# 3. заменяем прошлый блок: gzip-массив или (один раз, при переходе) старую raw-строку
src = DST.read_text(encoding="utf-8")
new, n = re.subn(
    r'// \d+ байт HTML -> \d+ байт gzip\n.*?const size_t INDEX_HTML_GZ_LEN = sizeof\(INDEX_HTML_GZ\);'
    r'|const char INDEX_HTML\[\] PROGMEM = R"HTML\(.*?\)HTML";',
    lambda m: block, src, count=1, flags=re.S)
if n != 1:
    sys.exit("ОШИБКА: не нашёл блок INDEX_HTML_GZ в web_assets.h")

if new == src:
    print("Без изменений: web_assets.h уже содержит эту версию страницы.")
else:
    DST.write_text(new, encoding="utf-8")
    print(f"OK: страница {len(raw)} байт, в gzip {len(gz)} байт — записана в web_assets.h")
    print("Дальше: собрать скетч в Arduino IDE и залить по OTA.")
