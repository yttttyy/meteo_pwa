// ===== ТОЛЬКО ДЛЯ РАЗРАБОТКИ В VS CODE / LIVE SERVER =====
// Подменяет ответы станции фейковыми данными, чтобы страницу можно было
// верстать без железа. В прошивку НЕ попадает: build_web.py вырезает блок
// между маркерами <!--DEV--> и <!--/DEV--> в index.html.
//
// Сценарии — добавь к адресу в браузере:
//   ?mock=offline  — MH-Z19 и ENS160 «отвалились» (проверить черточки)
//   ?mock=nodes    — два узла-сателлита, один офлайн (чипсы комнат)
//   ?mock=ap       — режим точки доступа (сеть не настроена)
//   ?mock=alarm    — высокий CO2 (тревога)
(() => {
  const scen = new URLSearchParams(location.search).get('mock') || '';
  const t0 = Date.now() / 1000;
  const wave = (i, n, base, amp, per) => +(base + amp * Math.sin(i / n * Math.PI * per)).toFixed(1);

  const nodes = scen === 'nodes' ? [
    { id: 111, name: 'Спальня', t: 21.3, h: 52, aqi: 2, tvoc: 95,  eco2: 620, age: 12,  on: true  },
    { id: 222, name: 'Кухня',   t: 24.8, h: 47, aqi: 3, tvoc: 240, eco2: 910, age: 340, on: false }
  ] : [];

  const data = () => ({
    temp: 24.5, hum: 45,
    co2: scen === 'alarm' ? 1720 : 780,
    aqi: 2, tvoc: 120, eco2: 690,
    cmode: 0, r: 80, g: 160, b: 255, rspd: 12,
    bright: 200, bauto: false, ldr: 2480,
    font: 0, toff: -3.9, hoff: 13, co2al: 1500,
    clim: scen === 'alarm' ? 62 : 99, trend: scen === 'alarm' ? 1 : -1,
    thr: {
      co2:  [800, 1000, 1500], temp: [16, 18, 20, 24, 26, 28],
      hum:  [20, 30, 40, 60, 70, 80], tvoc: [220, 660, 2200],
      aqi:  [2, 3, 4], eco2: [800, 1000, 1500]
    },
    fw: '2.7.0-dev',
    aht: 0,
    ens: scen === 'offline' ? 1 : 0,
    mhz: scen === 'offline' ? 1 : 0,
    rtc: true, up: 93720, heap: 214000,
    timeok: true,
    nodes,
    sta: scen !== 'ap',
    ssid: scen === 'ap' ? '' : 'Pofig',
    ip: scen === 'ap' ? '' : '192.168.1.42',
    rssi: -58
  });

  const hist = (range) => {
    const n = { h: 60, '3h': 180, day: 288, week: 168 }[range] || 180;
    const step = { h: 60, '3h': 60, day: 300, week: 3600 }[range] || 60;
    const s = (base, amp, per, dec) => Array.from({ length: n },
      (_, i) => +(wave(i, n, base, amp, per)).toFixed(dec));
    return {
      step, now: Math.floor(t0),
      t: s(23.5, 1.8, 3, 1),
      h: s(46, 7, 2, 0).map(Math.round),
      c: s(800, 320, 4, 0).map(Math.round),
      a: s(2.4, 1.2, 5, 0).map(v => Math.max(1, Math.min(5, Math.round(v)))),
      v: s(150, 90, 6, 0).map(v => Math.max(0, Math.round(v))),
      e: s(760, 280, 4, 0).map(v => Math.max(400, Math.round(v))),
      k: s(78, 18, 4, 0).map(v => Math.max(0, Math.min(100, Math.round(v))))
    };
  };

  const scan = [
    { ssid: 'Pofig', rssi: -52, open: false },
    { ssid: 'TP-Link_2G', rssi: -71, open: false },
    { ssid: 'FreeWiFi', rssi: -83, open: true }
  ];

  const realFetch = window.fetch;
  window.fetch = (url, opt) => {
    const u = String(url);
    const json = o => Promise.resolve(new Response(JSON.stringify(o),
      { status: 200, headers: { 'Content-Type': 'application/json' } }));
    if (u.startsWith('/data')) return json(data());
    if (u.startsWith('/hist')) return json(hist(new URL(u, location.href).searchParams.get('range')));
    if (u.startsWith('/wifi/scan')) return json(scan);
    if (u.startsWith('/')) {                       // управляющие роуты — просто OK
      console.log('[mock] ' + u);
      return Promise.resolve(new Response('OK', { status: 200 }));
    }
    return realFetch(url, opt);
  };

  addEventListener('DOMContentLoaded', () => {
    const b = document.createElement('div');
    b.textContent = 'DEV-режим: фейковые данные' + (scen ? ' (' + scen + ')' : '');
    b.style.cssText = 'position:fixed;right:8px;bottom:8px;z-index:99;background:#e5c07b;' +
      'color:#241a00;font:600 11px system-ui;padding:4px 9px;border-radius:20px;opacity:.9';
    document.body.appendChild(b);
  });
})();
