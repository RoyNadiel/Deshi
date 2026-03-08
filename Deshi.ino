#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include "DHT.h"

// ============================================================
//  PINES  —  Wemos D1 Mini
// ============================================================
const int PIN_SSR = D5;   // GPIO14 — Control Calefacción (SSR)
const int PIN_FAN = D6;   // GPIO12 — PWM Ventilador
const int PIN_DHT = D7;   // GPIO13 — Sensor Temp/Hum
// PIN_POT (A0) ya no se usa — control movido a la interfaz web

// ============================================================
//  CONFIGURACIÓN WI-FI AP
// ============================================================
const char* SSID_AP = "Deshi";
const char* PASS_AP = "Deshiudone";
ESP8266WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// ============================================================
//  SENSOR DHT22
// ============================================================
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ============================================================
//  RANGOS
// ============================================================
const int TEMP_MIN = 30;
const int TEMP_MAX = 70;
const int HORAS_MIN = 1;
const int HORAS_MAX = 24;

// ============================================================
//  VARIABLES DE ESTADO
// ============================================================
float currentTemp      = 0.0f;
float currentHum       = 0.0f;
float targetTemp       = 40.0f;  // Valor por defecto
bool  sistemaEncendido = false;
bool  tiempoBloqueado  = false;  // true = proceso iniciado
bool  finalizado       = false;

unsigned long startTime      = 0;
unsigned long durationMillis = 0;
int  minutosSelec = 240;          // 4 h = 240 min por defecto

// Lectura de temperatura no-bloqueante
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

// ============================================================
//  handleRoot  —  Página principal
// ============================================================
void handleRoot() {
  String html = F(R"rawHTML(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Deshidratadora Control</title>
  <style>
    :root {
      --bg:      #0f1117;
      --surface: #1c1f2e;
      --border:  #2a2d3e;
      --accent:  #f97316;
      --accent2: #3b82f6;
      --red:     #ef4444;
      --green:   #22c55e;
      --blue:    #00aaee;
      --yellow:  #eab308;
      --text:    #e2e8f0;
      --muted:   #94a49b;
      --radius:  14px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: 'Segoe UI', system-ui, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 1rem;
      overscroll-behavior: none;
    }

    /* ---- Card ---- */
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 2rem 1.8rem;
      width: 100%;
      max-width: 440px;
      box-shadow: 0 8px 32px rgba(0,0,0,.45);
    }
    .card-header {
      display: flex;
      align-items: center;
      gap: .75rem;
      margin-bottom: 1.6rem;
      border-bottom: 1px solid var(--border);
      padding-bottom: 1rem;
    }
    .card-header .icon { font-size: 1.8rem; line-height:1; }
    .card-header h1 { font-size: 1.25rem; font-weight: 700; }
    .card-header small { font-size: .75rem; color: var(--muted); display: block; }

    /* ---- Badge ---- */
    .badge {
      display: inline-flex; align-items: center; gap: .4rem;
      padding: .35rem .85rem; border-radius: 999px;
      font-size: .82rem; font-weight: 600; letter-spacing: .03em;
      margin-bottom: 1.2rem;
    }
    .badge.off    { background: rgba(100,116,139,.15); color: var(--muted);   border: 1px solid var(--border); }
    .badge.active { background: rgba(59,130,246,.15);  color: var(--accent2); border: 1px solid rgba(59,130,246,.3); }
    .badge.done   { background: rgba(34,197,94,.15);   color: var(--green);  border: 1px solid rgba(34,197,94,.3); }
    .badge.config { background: rgba(234,179,8,.15);   color: var(--yellow); border: 1px solid rgba(234,179,8,.3); }
    .badge .dot { width:8px; height:8px; border-radius:50%; }
    .badge.off .dot    { background: var(--muted); }
    .badge.active .dot { background: var(--accent2); animation: pulse 1.4s infinite; }
    .badge.done .dot   { background: var(--green); }
    .badge.config .dot { background: var(--yellow); }
    @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }

    /* ---- Data grid ---- */
    .data-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: .5rem; margin-bottom: 1.2rem; }
    .data-item {
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 10px; padding: .85rem 1rem;
    }
    .data-item .label {
      font-size: .7rem; color: var(--muted); text-transform: uppercase;
      letter-spacing: .06em; margin-bottom: .3rem;
    }
    .data-item .value { font-size: 1.45rem; font-weight: 700; }
    .data-item .value.hot  { color: var(--red); }
    .data-item .value.cool { color: var(--blue); }
    .data-item .value.grave { color: var(--red); animation: blink 1s infinite alternate; }
    @keyframes blink { from { opacity: 1; } to { opacity: 0.5; } }

    /* ---- Sliders de configuración ---- */
    .config-block {
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 10px; padding: 1rem 1.1rem; margin-bottom: .85rem;
    }
    .config-block .cfg-header {
      display: flex; justify-content: space-between; align-items: baseline;
      margin-bottom: .65rem;
    }
    .config-block .cfg-label {
      font-size: .75rem; color: var(--muted); text-transform: uppercase; letter-spacing: .06em;
    }
    .config-block .cfg-value {
      font-size: 1.3rem; font-weight: 700; color: var(--text);
    }
    .config-block .cfg-value span { font-size: .85rem; font-weight: 400; color: var(--muted); }
    input[type=range] {
      -webkit-appearance: none; width: 100%; height: 6px;
      background: var(--border); border-radius: 999px; outline: none; cursor: pointer;
      touch-action: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 20px; height: 20px;
      border-radius: 50%; background: var(--accent); cursor: pointer;
      box-shadow: 0 0 0 4px rgba(249,115,22,.2);
      transition: box-shadow .2s;
    }
    input[type=range]::-webkit-slider-thumb:active {
      box-shadow: 0 0 0 7px rgba(249,115,22,.3);
    }

    /* ---- Botones ---- */
    .btn {
      width: 100%; padding: .9rem 1rem; border: none; border-radius: 10px;
      font-size: 1rem; font-weight: 700; cursor: pointer; letter-spacing: .04em;
      transition: opacity .15s, transform .1s; margin-bottom: .6rem;
    }
    .btn:active { transform: scale(.97); }
    .btn-start  { background: var(--accent);  color: #fff; }
    .btn-reset  { background: var(--surface); color: var(--muted); border: 1px solid var(--border); }
    .btn:disabled { opacity: .45; cursor: not-allowed; }

    /* ---- Progreso ---- */
    .progress-wrap {
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 10px; padding: .85rem 1rem; margin-bottom: 1.2rem;
    }
    .progress-header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: .6rem; }
    .progress-header .label  { font-size: .7rem; color: var(--muted); text-transform: uppercase; letter-spacing: .06em; }
    .progress-header .remaining { font-size: 1.05rem; font-weight: 700; color: var(--accent); }
    .bar-bg   { height: 8px; background: var(--border); border-radius: 999px; overflow: hidden; }
    .bar-fill { height: 100%; background: linear-gradient(90deg, var(--accent2), var(--accent)); border-radius: 999px; transition: width .8s ease; }

    /* ---- Resistencia ---- */
    .heat-pill {
      display: flex; align-items: center; gap: .5rem;
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 10px; padding: .75rem 1rem;
      font-size: .9rem; font-weight: 600; margin-bottom: .75rem;
    }
    .heat-pill .heat-icon { font-size: 1.2rem; }
    .heat-on  { color: var(--red); }
    .heat-off { color: var(--green); }

    /* ---- Info ---- */
    .info-msg {
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 10px; padding: .85rem 1rem;
      font-size: .88rem; color: var(--muted); line-height: 1.5; margin-bottom: 1.2rem;
    }

    /* ---- Footer ---- */
    .footer { margin-top: 1.4rem; text-align: center; font-size: .7rem; color: var(--muted); }
    #last-update { opacity: .7; }

    /* ---- Toast de feedback ---- */
    #toast {
      position: fixed; bottom: 1.5rem; left: 50%; transform: translateX(-50%);
      background: #1e293b; color: var(--text); border: 1px solid var(--border);
      border-radius: 10px; padding: .65rem 1.2rem; font-size: .85rem;
      opacity: 0; transition: opacity .3s; pointer-events: none; white-space: nowrap;
    }
    #toast.show { opacity: 1; }
  </style>
</head>
<body>

<div class="card">
  <div class="card-header">
    <span class="icon">🌿</span>
    <div>
      <h1>Deshidratadora</h1>
      <small>Panel de Control</small>
    </div>
  </div>

  <div id="content">
    <div class="info-msg">Conectando...</div>
  </div>

  <div class="footer">
    <span id="last-update"></span>
  </div>
</div>

<div id="toast"></div>

<script>
  // ---------- Utilidades ----------
  function fmtTime(totalSecs) {
    const h = Math.floor(totalSecs / 3600);
    const m = Math.floor((totalSecs % 3600) / 60);
    const s = totalSecs % 60;
    if (h > 0) return h + 'h ' + String(m).padStart(2,'0') + 'm';
    if (m > 0) return m + 'm ' + String(s).padStart(2,'0') + 's';
    return s + 's';
  }

  function showToast(msg) {
    const t = document.getElementById('toast');
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(t._tid);
    t._tid = setTimeout(() => t.classList.remove('show'), 2500);
  }

  // ---------- Estado local de sliders ----------
  let cfgHours = 4;
  let cfgMins  = 0;
  let cfgTemp  = 40;

  function fmtDuration(h, m) {
    if (h === 0) return String(m).padStart(2,'0') + 'm';
    return h + 'h ' + String(m).padStart(2,'0') + 'm';
  }

  // ---------- Render ----------
  function renderContent(d) {
    let html = '';

    if (d.done) {
      // ---- FINALIZADO ----
      html += '<div class="badge done"><span class="dot"></span>FINALIZADO</div>';
      html += '<div class="info-msg">✅ El proceso ha terminado exitosamente.</div>';
      html += '<button class="btn btn-reset" id="btn-reset">🔄 Resetear</button>';

    } else if (d.on) {
      // ---- EN PROCESO ----
      const isHot = d.heating;
      html += '<div class="badge active"><span class="dot"></span>EN PROCESO</div>';
      html += '<div class="data-grid">';
      html +=   '<div class="data-item"><div class="label">Objetivo</div>'
             +  '<div class="value">' + d.target + '°</div></div>';
      html +=   '<div class="data-item"><div class="label">Actual</div>'
             +  '<div class="value ' + (isHot ? 'hot' : 'cool') + '">' + d.current + '°</div></div>';
      
      const humGrave = d.humidity >= 70 ? 'grave' : '';
      html +=   '<div class="data-item"><div class="label">Humedad</div>'
             +  '<div class="value ' + humGrave + '">' + d.humidity + '%</div></div>';
      html += '</div>';

      html += '<div class="heat-pill ' + (isHot ? 'heat-on' : 'heat-off') + '">'
           +  '<span class="heat-icon">' + (isHot ? '🔥' : '✅') + '</span>'
           +  'Resistencia: ' + (isHot ? 'Calentando' : 'Estable')
           +  '</div>';

      const pct = Math.min(100, Math.max(0, d.progress));
      html += '<div class="progress-wrap">'
           +  '<div class="progress-header">'
           +  '<span class="label">Tiempo restante</span>'
           +  '<span class="remaining">' + fmtTime(d.remaining) + '</span>'
           +  '</div>'
           +  '<div class="bar-bg"><div class="bar-fill" style="width:' + pct + '%"></div></div>'
           +  '</div>';

      html += '<button class="btn btn-reset" id="btn-reset">⏹ Detener y Resetear</button>';

    } else {
      // ---- CONFIGURACIÓN ----
      html += '<div class="badge config"><span class="dot"></span>CONFIGURACIÓN</div>';

      html += '<div class="config-block">'
           +  '<div class="cfg-header">'
           +  '<span class="cfg-label">⏱ Duración</span>'
           +  '<span class="cfg-value" id="lbl-hours">' + fmtDuration(cfgHours, cfgMins) + '</span>'
           +  '</div>'
           +  '<div style="display:flex;align-items:center;gap:.6rem;margin-bottom:.7rem">'
           +  '<span style="font-size:.7rem;color:var(--muted);min-width:2rem">Hora</span>'
           +  '<input type="range" id="sl-hours" min="0" max="24" value="' + cfgHours + '" style="flex:1">'
           +  '<span style="font-size:.82rem;font-weight:700;min-width:2.5rem;text-align:right" id="lbl-h-val">' + cfgHours + 'h</span>'
           +  '</div>'
           +  '<div style="display:flex;align-items:center;gap:.6rem">'
           +  '<span style="font-size:.7rem;color:var(--muted);min-width:2rem">Min</span>'
           +  '<input type="range" id="sl-mins" min="0" max="55" step="5" value="' + cfgMins + '" style="flex:1">'
           +  '<span style="font-size:.82rem;font-weight:700;min-width:2.5rem;text-align:right" id="lbl-m-val">' + String(cfgMins).padStart(2,'0') + 'm</span>'
           +  '</div>'
           +  '</div>';

      html += '<div class="config-block">'
           +  '<div class="cfg-header">'
           +  '<span class="cfg-label">🌡️ Temperatura</span>'
           +  '<span class="cfg-value" id="lbl-temp">' + cfgTemp + ' <span>°C</span></span>'
           +  '</div>'
           +  '<input type="range" id="sl-temp" min="30" max="70" value="' + cfgTemp + '">'
           +  '</div>';

      html += '<button class="btn btn-start" id="btn-start">▶ Iniciar Proceso</button>';
    }

    document.getElementById('content').innerHTML = html;

    // Actualizar timestamp
    const now = new Date();
    document.getElementById('last-update').textContent =
      'Actualizado: ' + now.toLocaleTimeString('es', {hour:'2-digit', minute:'2-digit', second:'2-digit'});

    // Vincular eventos tras render
    bindEvents(d);
  }

  function bindEvents(d) {
    // Sliders de configuración
    const slHours = document.getElementById('sl-hours');
    const slMins  = document.getElementById('sl-mins');
    const slTemp  = document.getElementById('sl-temp');
    if (slHours) {
      slHours.addEventListener('input', () => {
        cfgHours = parseInt(slHours.value);
        document.getElementById('lbl-h-val').textContent = cfgHours + 'h';
        document.getElementById('lbl-hours').textContent = fmtDuration(cfgHours, cfgMins);
        sendSet();
      });
    }
    if (slMins) {
      slMins.addEventListener('input', () => {
        cfgMins = parseInt(slMins.value);
        document.getElementById('lbl-m-val').textContent = String(cfgMins).padStart(2,'0') + 'm';
        document.getElementById('lbl-hours').textContent = fmtDuration(cfgHours, cfgMins);
        sendSet();
      });
    }
    if (slTemp) {
      slTemp.addEventListener('input', () => {
        cfgTemp = parseInt(slTemp.value);
        document.getElementById('lbl-temp').innerHTML = cfgTemp + ' <span>°C</span>';
        sendSet();
      });
    }

    // Botón Iniciar
    const btnStart = document.getElementById('btn-start');
    if (btnStart) {
      btnStart.addEventListener('click', () => {
        btnStart.disabled = true;
        fetch('/start', {method:'POST'})
          .then(r => r.ok ? showToast('🚀 Proceso iniciado') : showToast('⚠️ Error al iniciar'))
          .catch(() => showToast('⚠️ Sin conexión'));
      });
    }

    // Botón Reset
    const btnReset = document.getElementById('btn-reset');
    if (btnReset) {
      btnReset.addEventListener('click', () => {
        fetch('/reset', {method:'POST'})
          .then(r => r.ok ? showToast('🔄 Sistema reseteado') : showToast('⚠️ Error al resetear'))
          .catch(() => showToast('⚠️ Sin conexión'));
      });
    }
  }

  // Envía la configuración actual al dispositivo (debounce 300 ms)
  let setTimer = null;
  function sendSet() {
    clearTimeout(setTimer);
    setTimer = setTimeout(() => {
      const totalMins = cfgHours * 60 + cfgMins;
      const params = new URLSearchParams({minutes: totalMins, temp: cfgTemp});
      fetch('/set', {method:'POST', body: params,
             headers:{'Content-Type':'application/x-www-form-urlencoded'}})
        .catch(() => {}); // silencioso; el siguiente poll mostrará el estado real
    }, 300);
  }

  // ---------- Polling ----------
  // Evitar reconstruir el DOM mientras el usuario interactúa con los sliders:
  // en modo CONFIGURACIÓN solo renderizamos la primera vez que entramos a ese estado.
  let prevStateKey = null;

  function fetchStatus() {
    fetch('/status')
      .then(r => r.json())
      .then(d => {
        const key = d.done ? 'done' : d.on ? 'active' : 'config';
        if (key === 'config' && prevStateKey === 'config') {
          // Ya estamos en config y la UI ya existe — no tocar el DOM.
          return;
        }
        prevStateKey = key;
        renderContent(d);
      })
      .catch(() => {
        document.getElementById('content').innerHTML =
          '<div class="info-msg">⚠️ Sin conexión con el dispositivo.</div>';
      });
  }

  fetchStatus();
  setInterval(fetchStatus, 2000);
</script>
</body>
</html>
)rawHTML");

  server.send(200, "text/html", html);
}

// ============================================================
//  handleStatus  —  JSON para la UI
// ============================================================
void handleStatus() {
  bool heating = (digitalRead(PIN_SSR) == HIGH);

  unsigned long elapsed   = (startTime > 0) ? (millis() - startTime) : 0;
  unsigned long remaining = (durationMillis > elapsed) ? (durationMillis - elapsed) / 1000 : 0;
  int progress = (durationMillis > 0)
    ? (int)((float)elapsed / (float)durationMillis * 100.0f)
    : 0;

  String json = "{";
  json += "\"on\":"       + String(sistemaEncendido ? "true" : "false") + ",";
  json += "\"locked\":"   + String(tiempoBloqueado  ? "true" : "false") + ",";
  json += "\"done\":"     + String(finalizado        ? "true" : "false") + ",";
  json += "\"heating\":"  + String(heating           ? "true" : "false") + ",";
  json += "\"current\":"  + String(currentTemp, 1)  + ",";
  json += "\"humidity\":" + String(currentHum, 1)   + ",";
  json += "\"target\":"   + String((int)targetTemp)  + ",";
  json += "\"minutes\":"  + String(minutosSelec)     + ",";
  json += "\"remaining\":" + String(remaining)       + ",";
  json += "\"progress\":"  + String(progress);
  json += "}";

  server.send(200, "application/json", json);
}

// ============================================================
//  handleSet  —  POST /set?hours=X&temp=Y
//  Solo acepta cambios si el proceso NO ha iniciado aún.
// ============================================================
void handleSet() {
  if (tiempoBloqueado || finalizado) {
    server.send(400, "text/plain", "Proceso en curso");
    return;
  }
  if (server.hasArg("minutes")) {
    int m = server.arg("minutes").toInt();
    minutosSelec = constrain(m, 5, HORAS_MAX * 60);
  }
  if (server.hasArg("temp")) {
    int t = server.arg("temp").toInt();
    targetTemp = (float)constrain(t, TEMP_MIN, TEMP_MAX);
  }
  server.send(200, "text/plain", "OK");
}

// ============================================================
//  handleStart  —  POST /start
//  Inicia el proceso con los valores configurados.
// ============================================================
void handleStart() {
  if (finalizado) {
    server.send(400, "text/plain", "Finalizado, resetear primero");
    return;
  }
  if (tiempoBloqueado) {
    server.send(400, "text/plain", "Ya en proceso");
    return;
  }
  durationMillis   = (unsigned long)minutosSelec * 60000UL;
  startTime        = millis();
  sistemaEncendido = true;
  tiempoBloqueado  = true;
  Serial.print(F("Proceso iniciado via web: "));
  Serial.print(minutosSelec / 60);
  Serial.print(F("h "));
  Serial.print(minutosSelec % 60);
  Serial.print(F("m @ "));
  Serial.print((int)targetTemp);
  Serial.println(F(" °C"));
  server.send(200, "text/plain", "OK");
}

// ============================================================
//  handleReset  —  POST /reset
//  Detiene todo y vuelve al estado inicial de configuración.
// ============================================================
void handleReset() {
  digitalWrite(PIN_SSR, LOW);
  analogWrite(PIN_FAN, 0);
  sistemaEncendido = false;
  tiempoBloqueado  = false;
  finalizado       = false;
  startTime        = 0;
  durationMillis   = 0;
  Serial.println(F("Sistema reseteado via web."));
  server.send(200, "text/plain", "OK");
}

// ============================================================
//  VENTILADOR  —  Velocidad proporcional según ΔT
// ============================================================
void actualizarVentilador(float actual, float meta) {
  const int VEL_MIN = 170;
  const int VEL_MAX = 255;
  float diferencia = meta - actual;
  int velocidad;
  if      (diferencia <= 0.0f) velocidad = VEL_MIN;
  else if (diferencia >  5.0f) velocidad = VEL_MAX;
  else velocidad = (int)map((long)(diferencia * 10), 0, 50, VEL_MIN, VEL_MAX);
  analogWrite(PIN_FAN, velocidad);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SSR, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  digitalWrite(PIN_SSR, LOW);
  analogWrite(PIN_FAN, 0);

  analogWriteRange(255);
  dht.begin();

  WiFi.softAP(SSID_AP, PASS_AP);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print(F("IP del servidor: "));
  Serial.println(apIP);

  dnsServer.start(DNS_PORT, "*", apIP);

  if (MDNS.begin("deshi")) {
    Serial.println(F("mDNS iniciado: http://deshi.local"));
  }

  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  server.on("/set",    HTTP_POST, handleSet);
  server.on("/start",  HTTP_POST, handleStart);
  server.on("/reset",  HTTP_POST, handleReset);

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println(F("Servidor HTTP iniciado."));
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  dnsServer.processNextRequest();
  MDNS.update();
  server.handleClient();

  // Control de temperatura — solo cuando el proceso está activo
  if (sistemaEncendido && tiempoBloqueado && !finalizado) {

    // Lectura DHT no-bloqueante
    if (millis() - lastDHTRead > DHT_INTERVAL) {
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      if (!isnan(t)) currentTemp = t;
      if (!isnan(h)) currentHum = h;
      lastDHTRead = millis();
    }

    // Histéresis ±0.5 °C
    if      (currentTemp < (targetTemp - 0.5f)) digitalWrite(PIN_SSR, HIGH);
    else if (currentTemp >= targetTemp)          digitalWrite(PIN_SSR, LOW);

    actualizarVentilador(currentTemp, targetTemp);

    // Verificar fin de ciclo
    if ((millis() - startTime) >= durationMillis) {
      finalizado = true;
      digitalWrite(PIN_SSR, LOW);
      analogWrite(PIN_FAN, 0);
      Serial.println(F("Proceso finalizado."));
    }
  }

  delay(20);
}