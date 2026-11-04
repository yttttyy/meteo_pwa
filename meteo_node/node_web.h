#pragma once
#include <Arduino.h>

// Страница узла: свои показания + настройка WiFi/имени/станции + OTA.
// Открывается и через captive portal (AP Meteo-Node), и по http://meteonode.local
const char NODE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="ru"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Meteo-Node</title>
<style>
:root{--bg:#0f1216;--card:#1a1f26;--txt:#e8ecf1;--mut:#8a94a3;--acc:#4da3ff;--inp:#11151a;--grid:#262d36}
@media(prefers-color-scheme:light){:root{--bg:#eef1f5;--card:#fff;--txt:#1b2430;--mut:#66707d;--acc:#2b7de9;--inp:#e9edf2;--grid:#dde3ea}}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:system-ui,sans-serif;padding:16px;max-width:560px;margin:0 auto}
h1{font-size:20px;margin-bottom:4px}
#st{color:var(--mut);font-size:13px;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px}
.card{background:var(--card);border-radius:12px;padding:12px;text-align:center}
.card .v{font-size:22px;font-weight:600}
.card .l{font-size:12px;color:var(--mut);margin-top:2px}
section{background:var(--card);border-radius:12px;padding:14px;margin-bottom:16px}
section h2{font-size:14px;color:var(--mut);margin-bottom:10px;font-weight:600}
.row{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:8px}
input{background:var(--inp);border:1px solid var(--grid);border-radius:8px;color:var(--txt);padding:9px 10px;font-size:14px;flex:1;min-width:140px}
button{background:var(--acc);border:none;border-radius:8px;color:#fff;padding:9px 14px;font-size:14px;font-weight:600;cursor:pointer}
button.sec{background:var(--inp);color:var(--txt)}
.hint{color:var(--mut);font-size:12px;margin-top:6px}
#nets{display:flex;flex-direction:column;gap:6px;margin-top:8px}
#nets button{display:flex;justify-content:space-between;background:var(--inp);color:var(--txt);font-weight:400}
#nets button span{color:var(--mut);font-size:12px}
</style></head><body>
<h1 id="room">Meteo-Node</h1>
<div id="st">…</div>
<div class="grid">
<div class="card"><div class="v" id="vT">—</div><div class="l">Температура</div></div>
<div class="card"><div class="v" id="vH">—</div><div class="l">Влажность</div></div>
<div class="card"><div class="v" id="vA">—</div><div class="l">AQI (1–5)</div></div>
<div class="card"><div class="v" id="vV">—</div><div class="l">TVOC, ppb</div></div>
<div class="card"><div class="v" id="vE">—</div><div class="l">eCO₂, ppm</div></div>
<div class="card"><div class="v" id="vP">—</div><div class="l">Пуш на станцию</div></div>
</div>
<section><h2>Настройки</h2>
<div class="row"><button class="sec" id="scan">Сканировать сети</button></div>
<div id="nets"></div>
<div class="row"><input id="ssid" placeholder="Имя сети (SSID)"><input id="pass" type="password" placeholder="Пароль"></div>
<div class="row"><input id="rm" placeholder="Имя комнаты (напр. Спальня)"><input id="stn" placeholder="Адрес станции (meteo.local)"></div>
<div class="row"><button id="save">Сохранить и перезапустить</button></div>
<div class="hint">После сохранения узел перезагрузится и начнёт слать данные на станцию. Имя комнаты появится на станции автоматически.</div>
</section>
<section><h2>Система</h2>
<div class="row"><span class="hint" style="margin:0">Прошивка узла <span id="fw">—</span></span></div>
<div class="row"><input type="file" id="otaF" accept=".bin" style="color:var(--mut)"><button class="sec" id="otaB">Прошить OTA</button>
<button class="sec" id="rb">Перезагрузить</button></div>
<div class="hint" id="otaSt"></div>
</section>
<script>
const $=i=>document.getElementById(i);
async function load(){try{
const d=await(await fetch('/info',{cache:'no-store'})).json();
$('room').textContent=d.room;
$('vT').textContent=d.ok?d.t.toFixed(1)+'°':'--';
$('vH').textContent=d.ok?d.h+'%':'--';
$('vA').textContent=d.eok?d.aqi:'--';
$('vV').textContent=d.eok?d.tvoc:'--';
$('vE').textContent=d.eok?d.eco2:'--';
$('vP').textContent=d.sta?(d.push?'OK':'ошибка'):'нет WiFi';
$('st').textContent=(d.sta?('WiFi '+d.rssi+' dBm'):'режим настройки (AP)')+' · станция: '+d.station;
$('fw').textContent='v'+d.fw;
if(!$('rm').value)$('rm').value=d.room;
if(!$('stn').value)$('stn').value=d.station;
}catch(e){$('st').textContent='нет связи с узлом'}}
$('scan').onclick=async()=>{$('scan').disabled=true;$('scan').textContent='Сканирую…';
try{const ns=await(await fetch('/scan')).json();ns.sort((a,b)=>b.rssi-a.rssi);
$('nets').innerHTML='';const seen={};
ns.forEach(n=>{if(!n.ssid||seen[n.ssid])return;seen[n.ssid]=1;
const b=document.createElement('button');
b.innerHTML=n.ssid.replace(/</g,'&lt;')+'<span>'+n.rssi+' dBm</span>';
b.onclick=()=>{$('ssid').value=n.ssid;$('pass').focus()};
$('nets').appendChild(b)})}catch(e){}
$('scan').disabled=false;$('scan').textContent='Сканировать сети'};
$('save').onclick=async()=>{
const q='ssid='+encodeURIComponent($('ssid').value)+'&pass='+encodeURIComponent($('pass').value)+
'&room='+encodeURIComponent($('rm').value)+'&station='+encodeURIComponent($('stn').value);
await fetch('/save?'+q);alert('Сохранено, узел перезагружается');};
$('rb').onclick=()=>{if(confirm('Перезагрузить узел?'))fetch('/reboot')};
$('otaB').onclick=async()=>{const f=$('otaF').files[0];if(!f){alert('Выбери .bin');return}
if(!confirm('Прошить «'+f.name+'»?'))return;
const fd=new FormData();fd.append('u',f,f.name);
$('otaSt').textContent='Заливаю…';
try{const r=await fetch('/update',{method:'POST',body:fd});
$('otaSt').textContent=r.ok?'Готово, перезагружаюсь…':'Ошибка прошивки';}
catch(e){$('otaSt').textContent='Связь оборвалась — узел перезагружается'}};
load();setInterval(load,5000);
</script></body></html>)HTML";
