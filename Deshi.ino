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
int  currentFanSpeed = 0;         // PWM value 0-255

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
  <title>Deshi - Premium Control</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Audiowide&family=Inconsolata:wght@200..900&family=Inter:wght@400;500;600;700&family=Outfit:wght@400;600;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #030712;
      --surface: rgba(17, 24, 39, 0.7);
      --card-border: rgba(255, 255, 255, 0.08);
      --accent: #f97316;
      --accent-glow: rgba(249, 115, 22, 0.3);
      --blue: #3b82f6;
      --blue-glow: rgba(59, 130, 246, 0.3);
      --red: #ef4444;
      --green: #10b981;
      --purple: #8b5cf6;
      --text: #f9fafb;
      --text-muted: #9ca3af;
      --radius-lg: 24px;
      --radius-md: 16px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
    body {
      background: var(--bg);
      background-image: radial-gradient(at 0% 0%, rgba(249, 115, 22, 0.1) 0px, transparent 50%), radial-gradient(at 100% 100%, rgba(59, 130, 246, 0.1) 0px, transparent 50%);
      color: var(--text);
      font-family: "Inter", sans-serif;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 1.5rem;
      overflow: hidden;
    }
    .ambient-blob { position: fixed; width: 400px; height: 400px; filter: blur(80px); border-radius: 50%; z-index: -1; opacity: 0.4; animation: float 20s infinite alternate ease-in-out; }
    .blob-1 { background: var(--accent); top: -10%; left: -10%; }
    .blob-2 { background: var(--blue); bottom: -10%; right: -10%; animation-delay: -10s; }
    @keyframes float { 0% { transform: translate(0, 0) scale(1); } 100% { transform: translate(50px, 50px) scale(1.1); } }
    .card { background: var(--surface); backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px); border: 1px solid var(--card-border); border-radius: var(--radius-lg); padding: 2.5rem; width: 100%; max-width: 480px; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5); position: relative; overflow: hidden; }
    .card::before { content: ""; position: absolute; top: 0; left: 0; right: 0; height: 1px; background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.2), transparent); }
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 2rem; }
    .brand { display: flex; align-items: center; gap: 0.75rem; }
    .logo-icon { width: 42px; height: 42px; display: flex; align-items: center; justify-content: center; font-size: 1.85rem; }
    .brand-text h1 { font-family: "Audiowide", sans-serif; font-size: 1.6rem; font-weight: 400; letter-spacing: 0.05em; text-transform: uppercase; background: linear-gradient(to bottom, #fff, #9ca3af); -webkit-background-clip: text; background-clip: text; -webkit-text-fill-color: transparent; }
    .brand-text p { font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.1em; }
    .status-badge { padding: 0.5rem 1rem; border-radius: 100px; font-size: 0.72rem; font-weight: 800; display: flex; align-items: center; gap: 0.5rem; border: 1px solid rgba(255, 255, 255, 0.1); text-transform: uppercase; letter-spacing: 0.05em; color: white; }
    .status-config { background: linear-gradient(135deg, var(--accent), #ea580c); box-shadow: 0 4px 15px var(--accent-glow); }
    .status-active { background: linear-gradient(135deg, var(--blue), #1d4ed8); box-shadow: 0 4px 15px var(--blue-glow); }
    .status-done { background: linear-gradient(135deg, var(--green), #059669); box-shadow: 0 4px 15px rgba(16, 185, 129, 0.3); }
    .dot { width: 6px; height: 6px; border-radius: 50%; background: white; }
    .status-active .dot { box-shadow: 0 0 8px white; animation: pulse 2s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.15; } }
    .metrics { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; margin-bottom: 1.5rem; }
    .metric-card { background: rgba(255, 255, 255, 0.03); border: 1px solid var(--card-border); border-radius: var(--radius-md); padding: 1.25rem; display: flex; flex-direction: column; gap: 0.5rem; transition: transform 0.2s; }
    .metric-card:hover { transform: translateY(-2px); background: rgba(255, 255, 255, 0.05); }
    .metric-label { font-size: 0.7rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; display: flex; align-items: center; gap: 0.4rem; }
    .metric-value { font-family: "Outfit", sans-serif; font-size: 2rem; font-weight: 700; }
    .metric-value span { font-size: 1rem; color: var(--text-muted); margin-left: 2px; }
    .text-hot { color: var(--red); text-shadow: 0 0 15px rgba(239, 68, 68, 0.3); }
    .text-cool { color: var(--blue); }
    .text-warning { color: var(--accent); animation: flicker 1s infinite alternate; }
    @keyframes flicker { from { opacity: 1; } to { opacity: 0.7; } }
    .fan-status { grid-column: span 2; display: flex; align-items: center; justify-content: space-between; background: linear-gradient(90deg, rgba(255, 255, 255, 0.02), rgba(255, 255, 255, 0.05)); border: 1px solid var(--card-border); border-radius: var(--radius-md); padding: 1rem 1.25rem; margin-bottom: 1.5rem; }
    .fan-info { display: flex; align-items: center; gap: 0.75rem; }
    .fan-icon { width: 32px; height: 32px; display: flex; align-items: center; justify-content: center; color: var(--blue); animation: spin 3s infinite linear; }
    .fan-icon.stopped { animation: none; }
    @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
    .fan-mode-tag { padding: 0.25rem 0.75rem; border-radius: 6px; font-size: 0.65rem; font-weight: 800; text-transform: uppercase; letter-spacing: 0.05em; }
    .tag-soplar { background: rgba(59, 130, 246, 0.15); color: var(--blue); }
    .progress-section { background: rgba(255, 255, 255, 0.02); border: 1px solid var(--card-border); border-radius: var(--radius-md); padding: 1.25rem; margin-bottom: 1.5rem; }
    .progress-header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 0.75rem; }
    .time-rem { font-family: "Outfit", sans-serif; font-size: 1.25rem; font-weight: 600; color: var(--accent); }
    .bar-bg { height: 8px; background: rgba(255, 255, 255, 0.05); border-radius: 100px; overflow: hidden; }
    .bar-fill { height: 100%; background: linear-gradient(90deg, var(--blue), var(--accent)); border-radius: 100px; transition: width 1s cubic-bezier(0.4, 0, 0.2, 1); position: relative; }
    .bar-fill::after { content: ""; position: absolute; top:0; left:0; right:0; bottom:0; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent); animation: shine 2s infinite; }
    @keyframes shine { 0% { transform: translateX(-100%); } 100% { transform: translateX(100%); } }
    .config-input { display: flex; flex-direction: column; gap: 1.25rem; margin-bottom: 2rem; }
    .slider-group { display: flex; flex-direction: column; gap: 0.75rem; }
    .slider-label { display: flex; justify-content: space-between; font-size: 0.75rem; font-weight: 600; color: var(--text-muted); }
    .slider-label span:last-child { color: var(--text); font-size: 1rem; }
    input[type="range"] { -webkit-appearance: none; appearance: none; width: 100%; height: 6px; background: rgba(255, 255, 255, 0.1); border-radius: 100px; outline: none; }
    input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 22px; height: 22px; background: var(--text); border: 4px solid var(--accent); border-radius: 50%; cursor: pointer; box-shadow: 0 0 15px rgba(0, 0, 0, 0.5); transition: transform 0.1s; }
    .btn { width: 100%; padding: 1.1rem; border-radius: var(--radius-md); border: none; font-family: "Outfit", sans-serif; font-size: 1rem; font-weight: 700; cursor: pointer; transition: all 0.2s; display: flex; align-items: center; justify-content: center; gap: 10px; }
    .btn-primary { background: var(--accent); color: white; box-shadow: 0 10px 20px -5px var(--accent-glow); }
    .btn-secondary { background: rgba(255, 255, 255, 0.05); color: var(--text); border: 1px solid var(--card-border); margin-top: 0.75rem; }
    .footer { margin-top: 2rem; text-align: center; font-size: 0.7rem; color: var(--text-muted); display: flex; flex-direction: column; gap: 0.5rem; }
    #toast { position: fixed; bottom: 2rem; left: 50%; transform: translateX(-50%) translateY(100px); background: rgba(17, 24, 39, 0.9); backdrop-filter: blur(10px); border: 1px solid var(--card-border); padding: 1rem 2rem; border-radius: 100px; font-size: 0.875rem; font-weight: 600; color: var(--text); transition: transform 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275); z-index: 1000; }
    #toast.show { transform: translateX(-50%) translateY(0); }
  </style>
</head>
<body>
  <div class="ambient-blob blob-1"></div>
  <div class="ambient-blob blob-2"></div>
  <div class="card">
    <header class="header">
      <div class="brand"><div class="logo-icon">🌿</div><div class="brand-text"><h1>Deshi</h1><p>Smart Dehydrator</p></div></div>
      <div id="status-badge-container"></div>
    </header>
    <main id="main-content"><div style="text-align: center; padding: 2rem; color: var(--text-muted)">Iniciando...</div></main>
    <footer class="footer"><span id="update-time">Actualizado: --:--:--</span><span style="opacity: 0.4">v2.0 Premium Interface</span></footer>
  </div>
  <div id="toast"></div>
  <script>
    const formatTime = (s) => { const h=Math.floor(s/3600); const m=Math.floor((s%3600)/60); const sec=s%60; return h>0?`${h}h ${m}m`:m>0?`${m}m ${sec}s`:`${sec}s`; };
    const showToast = (msg) => { const t=document.getElementById("toast"); t.textContent=msg; t.classList.add("show"); setTimeout(()=>t.classList.remove("show"),3000); };
    let cfgMinutes = 240, cfgTemp = 45, prevStateKey = null;
    const renderBadge = (key) => {
      const badges = {
        config: '<div class="status-badge status-config"><div class="dot"></div>CONFIGURACIÓN</div>',
        active: '<div class="status-badge status-active"><div class="dot"></div>EN PROCESO</div>',
        done: '<div class="status-badge status-done"><div class="dot"></div>FINALIZADO</div>'
      };
      document.getElementById("status-badge-container").innerHTML = badges[key] || badges.config;
    };
    const renderUI = (data) => {
      const container = document.getElementById("main-content");
      let html = "";
      if (data.done) {
        html = `<div style="text-align:center; padding: 1rem 0 2rem 0;"><div style="font-size: 4rem; margin-bottom: 1rem;">✨</div><h2 style="font-family:'Outfit'; margin-bottom: 0.5rem;">¡Listo!</h2><p style="color:var(--text-muted); margin-bottom: 2rem;">El proceso ha terminado correctamente.</p><button class="btn btn-secondary" onclick="handleAction('reset')">🔄 Volver a Inicio</button></div>`;
        renderBadge("done");
      } else if (data.on) {
        const isGrave = data.humidity >= 70;
        html = `<div class="metrics"><div class="metric-card"><span class="metric-label">🌡️ Objetivo</span><div class="metric-value">${data.target}<span>°C</span></div></div><div class="metric-card"><span class="metric-label">🔥 Actual</span><div class="metric-value ${data.heating?'text-hot':'text-cool'}">${data.current.toFixed(1)}<span>°C</span></div></div><div class="metric-card" style="grid-column: span 2"><span class="metric-label">💧 Humedad Ambiente</span><div class="metric-value ${isGrave?'text-warning':''}">${data.humidity.toFixed(1)}<span>%</span></div></div></div><div class="fan-status"><div class="fan-info"><div class="fan-icon"><svg viewBox="0 0 24.00 24.00" xmlns="http://www.w3.org/2000/svg" fill="#475dff" stroke="#475dff" transform="rotate(270)matrix(-1, 0, 0, -1, 0, 0)" stroke-width="0.00024"><g><rect width="24" height="24" fill="none"></rect> <path d="M12,11a1,1,0,1,0,1,1,1,1,0,0,0-1-1m.5-9C17,2,17.1,5.57,14.73,6.75a3.36,3.36,0,0,0-1.62,2.47,3.17,3.17,0,0,1,1.23.91C18,8.13,22,8.92,22,12.5c0,4.5-3.58,4.6-4.75,2.23a3.44,3.44,0,0,0-2.5-1.62,3.24,3.24,0,0,1-.91,1.23c2,3.69,1.2,7.66-2.38,7.66C7,22,6.89,18.42,9.26,17.24a3.46,3.46,0,0,0,1.62-2.45,3,3,0,0,1-1.25-.92C5.94,15.85,2,15.07,2,11.5,2,7,5.54,6.89,6.72,9.26A3.39,3.39,0,0,0,9.2,10.87a2.91,2.91,0,0,1,.92-1.22C8.13,6,8.92,2,12.48,2Z"></path> </g></svg></div><div><p style="font-size: 1rem; font-weight: 600;">Ventilación</p><p style="font-size: 0.7rem; color: var(--text-muted);">Velocidad adaptativa</p></div></div><span class="fan-mode-tag tag-soplar" style="font-size: 0.85rem;">${data.fanSpeed || 0}%</span></div><div class="progress-section"><div class="progress-header"><span class="metric-label">⏱️ Restante</span><span class="time-rem">${formatTime(data.remaining)}</span></div><div class="bar-bg"><div class="bar-fill" style="width: ${data.progress}%"></div></div></div><button class="btn btn-secondary" onclick="handleAction('reset')">⏹ Detener Sistema</button>`;
        renderBadge("active");
      } else {
        html = `<div class="config-input"><div class="slider-group"><div class="slider-label"><span>⏱️ DURACIÓN TOTAL</span><span id="val-time">${Math.floor(cfgMinutes/60)}h ${cfgMinutes%60}m</span></div><input type="range" id="sl-minutes" min="15" max="1440" step="15" value="${cfgMinutes}" oninput="updateConfig('minutes', this.value)"></div><div class="slider-group"><div class="slider-label"><span>🌡️ TEMPERATURA</span><span id="val-temp">${cfgTemp}°C</span></div><input type="range" id="sl-temp" min="30" max="75" value="${cfgTemp}" oninput="updateConfig('temp', this.value)"></div></div><button class="btn btn-primary" onclick="handleAction('start')"><span>▶</span> INICIAR DESHIDRATACIÓN</button>`;
        renderBadge("config");
      }
      container.innerHTML = html;
      document.getElementById("update-time").textContent = `Actualizado: ${new Date().toLocaleTimeString()}`;
    };
    const updateConfig = (key, val) => {
      val = parseInt(val);
      if (key === 'minutes') { cfgMinutes = val; document.getElementById("val-time").textContent = `${Math.floor(val/60)}h ${val%60}m`; }
      else { cfgTemp = val; document.getElementById("val-temp").textContent = `${val}°C`; }
      const params = new URLSearchParams({minutes: cfgMinutes, temp: cfgTemp});
      fetch('/set', {method:'POST', body: params, headers:{'Content-Type':'application/x-www-form-urlencoded'}});
    };
    const handleAction = (action) => {
      fetch('/' + action, {method:'POST'}).then(r => { if(r.ok) { showToast(action==='start'?'🚀 Iniciado':'⏹ Detenido'); prevStateKey=null; fetchStatus(); } });
    };
    const fetchStatus = () => {
      fetch('/status').then(r => r.json()).then(d => {
        const key = d.done ? 'done' : d.on ? 'active' : 'config';
        if (key === 'config' && prevStateKey === 'config') return;
        if (key === 'config') { cfgMinutes = d.minutes; cfgTemp = d.target; }
        prevStateKey = key;
        renderUI(d);
      }).catch(()=> { document.getElementById("main-content").innerHTML = `<div style="text-align:center; padding:2rem; color:var(--red);">⚠️ Sin conexión</div>`; });
    };
    setInterval(fetchStatus, 2000);
    fetchStatus();
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
  int progress = (durationMillis > 0) ? (int)((float)elapsed / (float)durationMillis * 100.0f) : 0; 
  
  // Calculate fan percentage based on PWM (170-255 range usually, or 0 if off)
  int fanPct = 0;
  if (sistemaEncendido && !finalizado) {
    fanPct = map(currentFanSpeed, 0, 255, 0, 100);
  }

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
  json += "\"progress\":"  + String(progress)        + ",";
  json += "\"fanSpeed\":"  + String(fanPct);
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
  
  currentFanSpeed = velocidad;
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