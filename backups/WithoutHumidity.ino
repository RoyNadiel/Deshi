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
const int PIN_POT = A0;   // ADC0   — Potenciómetro

// ============================================================
//  CONFIGURACIÓN WI-FI AP
// ============================================================
const char* SSID_AP = "Deshi";
const char* PASS_AP = "12345678";
ESP8266WebServer server(80);
DNSServer dnsServer; // Servidor DNS para el portal cautivo
const byte DNS_PORT = 53;

// ============================================================
//  SENSOR DHT22
// ============================================================
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ============================================================
//  RANGOS DE TEMPERATURA
// ============================================================
const int TEMP_MIN = 30;
const int TEMP_MAX = 70;

// ============================================================
//  HISTÉRESIS DEL POTENCIÓMETRO (ADC 0-1023)
// ============================================================
const int UMBRAL_APAGADO   = 30;   // Por debajo → apagar
const int UMBRAL_ENCENDIDO = 60;   // Por encima → encender

// ============================================================
//  VARIABLES DE ESTADO
// ============================================================
float currentTemp     = 0.0f;
float targetTemp      = 0.0f;
bool  sistemaEncendido = false;

unsigned long startTime = 0;
unsigned long durationMillis = 0;
int horasSelec = 0;
bool tiempoBloqueado = false;
bool finalizado = false;
unsigned long lastPotMove = 0;
int lastPotValue = 0;

// Lectura de temperatura no-bloqueante
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000; // ms entre lecturas

// ============================================================
//  handleRoot  —  Página principal (shell estático)
//  La UI se actualiza vía fetch() contra /status, sin
//  meta-refresh, por lo que la respuesta es más fluida y
//  no interrumpe la interacción del usuario.
// ============================================================
void handleRoot() {
  // El HTML es PROGMEM-friendly en tamaño pero se sirve desde RAM en D1 Mini.
  // Para boards con poca RAM se puede usar PROGMEM / LittleFS.
  String html = F(R"rawHTML(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Deshidratadora Control</title>
  <style>
    :root {
      --bg:        #0f1117;
      --surface:   #1c1f2e;
      --border:    #2a2d3e;
      --accent:    #f97316;
      --accent2:   #3b82f6;
      --green:     #22c55e;
      --red:       #ef4444;
      --yellow:    #eab308;
      --text:      #e2e8f0;
      --muted:     #64748b;
      --radius:    14px;
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
    }

    /* ---- Card principal ---- */
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 2rem 1.8rem;
      width: 100%;
      max-width: 420px;
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
    .card-header h1 {
      font-size: 1.25rem;
      font-weight: 700;
      color: var(--text);
    }
    .card-header small {
      font-size: .75rem;
      color: var(--muted);
      display: block;
    }

    /* ---- Badge de estado ---- */
    .badge {
      display: inline-flex;
      align-items: center;
      gap: .4rem;
      padding: .35rem .85rem;
      border-radius: 999px;
      font-size: .82rem;
      font-weight: 600;
      letter-spacing: .03em;
      margin-bottom: 1.2rem;
    }
    .badge.off     { background: rgba(100,116,139,.15); color: var(--muted);  border: 1px solid var(--border); }
    .badge.active  { background: rgba(59,130,246,.15);  color: var(--accent2); border: 1px solid rgba(59,130,246,.3); }
    .badge.done    { background: rgba(34,197,94,.15);   color: var(--green);  border: 1px solid rgba(34,197,94,.3); }
    .badge.adjust  { background: rgba(234,179,8,.15);   color: var(--yellow); border: 1px solid rgba(234,179,8,.3); }
    .badge .dot { width:8px; height:8px; border-radius:50%; }
    .badge.off .dot    { background: var(--muted); }
    .badge.active .dot { background: var(--accent2); animation: pulse 1.4s infinite; }
    .badge.done .dot   { background: var(--green); }
    .badge.adjust .dot { background: var(--yellow); animation: pulse 1.4s infinite; }
    @keyframes pulse {
      0%,100% { opacity:1; } 50% { opacity:.3; }
    }

    /* ---- Filas de datos ---- */
    .data-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: .75rem;
      margin-bottom: 1.2rem;
    }
    .data-item {
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: .85rem 1rem;
    }
    .data-item .label {
      font-size: .7rem;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: .06em;
      margin-bottom: .3rem;
    }
    .data-item .value {
      font-size: 1.45rem;
      font-weight: 700;
      color: var(--text);
    }
    .data-item .value.hot  { color: var(--red); }
    .data-item .value.cool { color: var(--green); }

    /* ---- Barra de progreso de tiempo ---- */
    .progress-wrap {
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: .85rem 1rem;
      margin-bottom: 1.2rem;
    }
    .progress-header {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      margin-bottom: .6rem;
    }
    .progress-header .label { font-size: .7rem; color: var(--muted); text-transform: uppercase; letter-spacing: .06em; }
    .progress-header .remaining { font-size: 1.05rem; font-weight: 700; color: var(--accent); }
    .bar-bg {
      height: 8px;
      background: var(--border);
      border-radius: 999px;
      overflow: hidden;
    }
    .bar-fill {
      height: 100%;
      background: linear-gradient(90deg, var(--accent2), var(--accent));
      border-radius: 999px;
      transition: width .8s ease;
    }

    /* ---- Mensaje informativo ---- */
    .info-msg {
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: .85rem 1rem;
      font-size: .88rem;
      color: var(--muted);
      line-height: 1.5;
      margin-bottom: 1.2rem;
    }

    /* ---- Resistencia pill ---- */
    .heat-pill {
      display: flex;
      align-items: center;
      gap: .5rem;
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: .75rem 1rem;
      font-size: .9rem;
      font-weight: 600;
      margin-bottom: .75rem;
    }
    .heat-pill .heat-icon { font-size: 1.2rem; }
    .heat-on  { color: var(--red); }
    .heat-off { color: var(--green); }

    /* ---- Footer ---- */
    .footer {
      margin-top: 1.4rem;
      text-align: center;
      font-size: .7rem;
      color: var(--muted);
    }
    #last-update { opacity: .7; }
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

  <!-- Contenido dinámico -->
  <div id="content">
    <div class="info-msg">Conectando...</div>
  </div>

  <div class="footer">
    <span id="last-update"></span>
  </div>
</div>

<script>
  function fmtTime(totalSecs) {
    const h = Math.floor(totalSecs / 3600);
    const m = Math.floor((totalSecs % 3600) / 60);
    const s = totalSecs % 60;
    if (h > 0) return h + 'h ' + String(m).padStart(2,'0') + 'm';
    if (m > 0) return m + 'm ' + String(s).padStart(2,'0') + 's';
    return s + 's';
  }

  function renderContent(d) {
    let html = '';

    if (!d.on && !d.done) {
      // ---- APAGADO ----
      html += '<div class="badge off"><span class="dot"></span>APAGADO</div>';
      html += '<div class="info-msg">Gira la perilla hacia la derecha para iniciar el proceso de deshidratación.</div>';

    } else if (d.done) {
      // ---- FINALIZADO ----
      html += '<div class="badge done"><span class="dot"></span>FINALIZADO</div>';
      html += '<div class="info-msg">✅ El proceso ha terminado exitosamente.<br>Regresa la perilla al inicio para resetear.</div>';

    } else if (!d.locked) {
      // ---- AJUSTANDO TIEMPO ----
      html += '<div class="badge adjust"><span class="dot"></span>AJUSTANDO TIEMPO</div>';
      const countdown = Math.max(0, d.countdown);
      html += '<div class="data-grid">';
      html +=   '<div class="data-item">';
      html +=     '<div class="label">Tiempo selec.</div>';
      html +=     '<div class="value">' + d.hours + ' h</div>';
      html +=   '</div>';
      html +=   '<div class="data-item">';
      html +=     '<div class="label">Iniciando en</div>';
      html +=     '<div class="value">' + countdown + ' s</div>';
      html +=   '</div>';
      html += '</div>';
      html += '<div class="info-msg">Deja de mover la perilla para confirmar el tiempo.</div>';

    } else {
      // ---- EN PROCESO ----
      html += '<div class="badge active"><span class="dot"></span>EN PROCESO</div>';

      const isHot = d.heating;
      html += '<div class="data-grid">';
      html +=   '<div class="data-item">';
      html +=     '<div class="label">Temp. Objetivo</div>';
      html +=     '<div class="value">' + d.target + ' °C</div>';
      html +=   '</div>';
      html +=   '<div class="data-item">';
      html +=     '<div class="label">Temp. Actual</div>';
      html +=     '<div class="value ' + (isHot ? 'hot' : 'cool') + '">' + d.current + ' °C</div>';
      html +=   '</div>';
      html += '</div>';

      html += '<div class="heat-pill ' + (isHot ? 'heat-on' : 'heat-off') + '">';
      html +=   '<span class="heat-icon">' + (isHot ? '🔥' : '✅') + '</span>';
      html +=   'Resistencia: ' + (isHot ? 'Calentando' : 'Estable');
      html += '</div>';

      const pct = Math.min(100, Math.max(0, d.progress));
      html += '<div class="progress-wrap">';
      html +=   '<div class="progress-header">';
      html +=     '<span class="label">Tiempo restante</span>';
      html +=     '<span class="remaining">' + fmtTime(d.remaining) + '</span>';
      html +=   '</div>';
      html +=   '<div class="bar-bg"><div class="bar-fill" style="width:' + pct + '%"></div></div>';
      html += '</div>';
    }

    document.getElementById('content').innerHTML = html;
    const now = new Date();
    document.getElementById('last-update').textContent =
      'Actualizado: ' + now.toLocaleTimeString('es', {hour:'2-digit', minute:'2-digit', second:'2-digit'});
  }

  function fetchStatus() {
    fetch('/status')
      .then(r => r.json())
      .then(d => renderContent(d))
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
//  handleStatus  —  Endpoint JSON que consume la UI
// ============================================================
void handleStatus() {
  bool heating = (digitalRead(PIN_SSR) == HIGH);

  unsigned long elapsed   = (startTime > 0) ? (millis() - startTime) : 0;
  unsigned long remaining = (durationMillis > elapsed) ? (durationMillis - elapsed) / 1000 : 0;
  int progress = (durationMillis > 0)
      ? (int)((float)elapsed / (float)durationMillis * 100.0f)
      : 0;
  int countdown = (int)(4 - (millis() - lastPotMove) / 1000);

  String json = "{";
  json += "\"on\":"      + String(sistemaEncendido ? "true" : "false") + ",";
  json += "\"locked\":"  + String(tiempoBloqueado  ? "true" : "false") + ",";
  json += "\"done\":"    + String(finalizado        ? "true" : "false") + ",";
  json += "\"heating\":" + String(heating           ? "true" : "false") + ",";
  json += "\"current\":" + String(currentTemp, 1)  + ",";
  json += "\"target\":"  + String((int)targetTemp)  + ",";
  json += "\"hours\":"   + String(horasSelec)       + ",";
  json += "\"remaining\":" + String(remaining)      + ",";
  json += "\"progress\":" + String(progress)        + ",";
  json += "\"countdown\":" + String(max(0, countdown));
  json += "}";

  server.send(200, "application/json", json);
}

// ============================================================
//  VENTILADOR  —  Velocidad proporcional según diferencia ΔT
//  • ΔT ≤ 0  → velocidad mínima (circulación base)
//  • 0 < ΔT ≤ 5 → escala lineal entre mínimo y máximo
//  • ΔT > 5  → velocidad máxima
// ============================================================
void actualizarVentilador(float actual, float meta) {
  const int VEL_MIN = 170;
  const int VEL_MAX = 255;
  float diferencia = meta - actual;
  int velocidad;
  if (diferencia <= 0.0f) {
    velocidad = VEL_MIN;
  } else if (diferencia > 5.0f) {
    velocidad = VEL_MAX;
  } else {
    velocidad = (int)map((long)(diferencia * 10), 0, 50, VEL_MIN, VEL_MAX);
  }
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

  // ---- DNS: Redirigir todo a la IP del Wemos (Portal Cautivo) ----
  dnsServer.start(DNS_PORT, "*", apIP);

  // ---- mDNS: Acceso vía http://deshi.local ----
  if (MDNS.begin("deshi")) {
    Serial.println(F("mDNS iniciado: http://deshi.local"));
  }

  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  
  // Manejador para cuando el navegador busca direcciones raras (Captive Portal)
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
  dnsServer.processNextRequest(); // Mantiene el DNS funcionando
  MDNS.update();                  // Mantiene el mDNS funcionando
  server.handleClient();

  int potValue = analogRead(PIN_POT);

  // ---- APAGADO (histéresis) ----
  if (potValue < UMBRAL_APAGADO) {
    if (sistemaEncendido) {
      digitalWrite(PIN_SSR, LOW);
      analogWrite(PIN_FAN, 0);
      sistemaEncendido  = false;
      tiempoBloqueado   = false;
      finalizado        = false;
      startTime         = 0;
      durationMillis    = 0;
      horasSelec        = 0;
    }
  }
  // ---- ENCENDIDO ----
  else if (potValue > UMBRAL_ENCENDIDO && !finalizado) {
    if (!sistemaEncendido) {
      sistemaEncendido = true;
      lastPotMove  = millis();
      lastPotValue = potValue;
    }

    if (!tiempoBloqueado) {
      // Selección de horas (1–24) con el potenciómetro
      horasSelec = map(potValue, UMBRAL_ENCENDIDO, 1023, 1, 24);

      // Detectar movimiento del potenciómetro (umbral de 15 ADC counts)
      if (abs(potValue - lastPotValue) > 15) {
        lastPotMove  = millis();
        lastPotValue = potValue;
      }

      // Bloquear tiempo tras 4 s de inactividad
      if ((millis() - lastPotMove) > 4000UL) {
        durationMillis = (unsigned long)horasSelec * 3600000UL;
        startTime      = millis();
        tiempoBloqueado = true;
        Serial.print(F("Tiempo bloqueado: "));
        Serial.print(horasSelec);
        Serial.println(F(" h"));
      }
    }
    else {
      // ---- Control de temperatura (PID on-off con histéresis) ----
      targetTemp = (float)map(potValue, UMBRAL_ENCENDIDO, 1023, TEMP_MIN, TEMP_MAX);

      // Lectura DHT no-bloqueante
      if (millis() - lastDHTRead > DHT_INTERVAL) {
        float t = dht.readTemperature();
        if (!isnan(t)) currentTemp = t;
        lastDHTRead = millis();
      }

      // Histéresis ±0.5 °C
      if      (currentTemp < (targetTemp - 0.5f)) digitalWrite(PIN_SSR, HIGH);
      else if (currentTemp >= targetTemp)         digitalWrite(PIN_SSR, LOW);

      actualizarVentilador(currentTemp, targetTemp);

      // Verificar fin de ciclo
      if ((millis() - startTime) >= durationMillis) {
        finalizado = true;
        Serial.println(F("Proceso finalizado."));
      }
    }
  }

  // ---- Seguridad: apagar resistencia y ventilador al finalizar ----
  if (finalizado) {
    digitalWrite(PIN_SSR, LOW);
    analogWrite(PIN_FAN, 0);
  }

  delay(20);
}
