# 🌿 Deshi - Control de Deshidratador Eléctrico

**Deshi** es un sistema de control inteligente para un deshidratador de alimentos basado en un microcontrolador **ESP8266** (Wemos D1 Mini). Ofrece un panel de control web moderno, responsivo y fácil de usar para gestionar el tiempo, la temperatura y monitorear la humedad del proceso.

## ✨ Características Principales

- **Panel de Control Web Premium:** Interfaz oscura (_Dark Mode_) moderna con indicadores en tiempo real de temperatura, humedad actual, progreso y estado de la resistencia.
- **Alerta de Humedad Grave:** Indicador visual parpadeante si la humedad interna alcanza niveles críticos (>= 70%), listo para futura lógica de extracción.
- **Conexión Directa (Punto de Acceso):** Crea su propia red Wi-Fi ("Deshi") para que puedas controlarlo desde cualquier smartphone o PC sin necesidad de internet.
- **Control Inteligente de Temperatura:** Utiliza un sensor **DHT22** para lecturas precisas y un **Relé de Estado Sólido (SSR)** para una regulación suave con histéresis de ±0.5°C.
- **Ventilación Dinámica:** Control de ventilador por **PWM** que ajusta su velocidad automáticamente según la diferencia térmica.
- **Seguridad y Automatización:** Apagado automático al finalizar el ciclo y bloqueo de configuración durante el proceso activo.
- **Portal Cautivo & mDNS:** Redirección automática al panel al conectar con el Wi-Fi y acceso simple mediante `http://deshi.local`.

## 🛠️ Hardware y Pinout (Wemos D1 Mini)

| Componente             | Pin (Literal) | Pin (GPIO) | Descripción                                                                                     |
| :--------------------- | :------------ | :--------- | :---------------------------------------------------------------------------------------------- |
| **Resistencia (SSR)**  | `D5`          | `GPIO14`   | Control de encendido/apagado de calor.                                                          |
| **Ventiladores (PWM)** | `D6`          | `GPIO12`   | Control de velocidad proporcional (y futura inversión de polaridad para extracción de humedad). |
| **Sensor (DHT22)**     | `D7`          | `GPIO13`   | Sensor de temperatura y humedad en tiempo real.                                                 |

_(Nota: Está planeado implementar un módulo Puente H para invertir la polaridad de los ventiladores como sistema de extracción de humedad en una próxima versión)._

## 🚀 Cómo Empezar

1.  **Cargar el Código:** Abre el archivo `Deshi.ino` en el IDE de Arduino.
2.  **Librerías Necesarias:** Asegúrate de tener instaladas las librerías `ESP8266WiFi`, `ESP8266WebServer` y `DHT sensor library` (por Adafruit).
3.  **Configurar Placa:** Selecciona "Wemos D1 R1" o "NodeMCU 1.0" en herramientas.
4.  **Conexión:**
    - Busca la red Wi-Fi llamada **"Deshi"**.
    - La contraseña por defecto es: `Deshiudone`.
    - Abre tu navegador y ve a `http://deshi.local` o deja que el portal cautivo te redirija.

## 📊 Funcionamiento del Sistema

### Rangos de Operación

- **Temperatura:** 30°C a 70°C.
- **Tiempo:** 1 a 24 horas (ajustable en pasos de 5 minutos).

### Lógica de Control

- **Histéresis Térmica:** La resistencia se enciende si la temperatura cae 0.5°C del objetivo, y se apaga al alcanzarlo.
- **PWM Ventilador:**
  - Diferencia > 5°C: Velocidad Máxima de soplado.
  - Temperatura Alcanzada: Velocidad Mínima para circulación.
  - Proceso Finalizado: Apagado Total.

---

Desarrollado para automatizar el secado perfecto.
