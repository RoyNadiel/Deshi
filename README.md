# 🌿 Deshi - Control de Deshidratador Eléctrico v2.0

**Deshi** es un sistema de control inteligente para un deshidratador de alimentos basado en el microcontrolador **ESP8266** (Wemos D1 Mini). Esta versión introduce una interfaz web de última generación con estética futurista y monitorización precisa para un secado perfecto.

## ✨ Características Principales

- **Interfaz Premium v2.0:** Diseño ultra-moderno con **Glassmorphism**, fondos dinámicos y tipografía _Audiowide_ de estilo retro-futurista.
- **Monitorización en Tiempo Real:** Visualización persistente de temperatura objetivo/actual, humedad ambiente y **velocidad del ventilador en %**.
- **Ventilación Proporcional:** Dos ventiladores de alto flujo configurados en paralelo que ajustan su potencia automáticamente mediante **PWM** según la demanda térmica.
- **Alertas Visuales:** Sistema de advertencia intermitente en la interfaz si la humedad interna supera niveles críticos (>= 70%).
- **Control de Precisión:** Regulación térmica mediante sensor **DHT22** y un **Relé de Estado Sólido (SSR)** con una histéresis optimizada de ±0.5°C.
- **Conectividad Autónoma:** Crea su propio Punto de Acceso Wi-Fi ("Deshi") con Portal Cautivo para un acceso instantáneo desde cualquier dispositivo.
- **Seguridad:** Bloqueo automático de configuración durante el funcionamiento y apagado total de seguridad al finalizar el tiempo programado.

## 🛠️ Hardware y Pinout (Wemos D1 Mini)

| Componente             | Pin (Literal) | Pin (GPIO) | Función                               |
| :--------------------- | :------------ | :--------- | :------------------------------------ |
| **Resistencia (SSR)**  | `D5`          | `GPIO14`   | Control de potencia calorífica.       |
| **Ventiladores (PWM)** | `D6`          | `GPIO12`   | Control de flujo de aire (0% a 100%). |
| **Sensor (DHT22)**     | `D7`          | `GPIO13`   | Lectura de temperatura y humedad.     |

## 📊 Lógica de Operación

### Control Térmico

El sistema mantiene la temperatura deseada (ajustable de 30°C a 70°C) activando el SSR si la temperatura cae por debajo del objetivo y desactivándolo inmediatamente al alcanzarlo, asegurando una temperatura constante.

### Sistema de Ventilación

La velocidad de los ventiladores es adaptativa:

- **Diferencial > 5°C:** Los ventiladores operan al 100% para distribuir el calor rápidamente.
- **Cerca del objetivo:** La velocidad se reduce suavemente para mantener una circulación de aire eficiente y constante (mínimo 66%).
- **Finalización:** Apagado total de ventilación y calor.

## 🚀 Instalación y Uso

1.  **Carga el firmware:** Abre `Deshi.ino` en el IDE de Arduino y cárgalo en tu ESP8266.
2.  **Conexión:** Conéctate a la red Wi-Fi **"Deshi"** (clave: `Deshiudone`).
3.  **Panel de Control:** Navega a `http://deshi.local` o espera la redirección del portal cautivo.
4.  **Configura:** Desliza los controles de tiempo y temperatura e inicia el proceso.

---

Desarrollado para elevar la deshidratación artesanal a un nivel tecnológico superior.
