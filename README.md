# 🟦 Tetris

Implementación del Tetris clásico para la consola portátil **ESPectro** (ESP32-S3).

> Parte del proyecto ESPectro — [base_espectro](https://github.com/bf-upc/base_espectro)

---

## Descripción

Tetris clásico con las siete piezas estándar (tetrominós). Completa líneas horizontales para sumar puntos. El juego se acelera a medida que avanzas de nivel.

## Controles

| Control | Acción |
|---------|--------|
| Joystick eje X | Mover pieza izquierda/derecha |
| Joystick eje Y (abajo) | Caída rápida |
| Botón A | Rotar pieza |
| Botón B | Acceder al Game Loader |

## Puntuación

| Líneas simultáneas | Puntos |
|-------------------|--------|
| 1 línea | 100 pts |
| 2 líneas | 300 pts |
| 3 líneas | 500 pts |
| 4 líneas (Tetris) | 800 pts |

El récord se guarda automáticamente en la memoria flash (NVS) y el historial de las últimas 20 partidas es visible en el dashboard.

## Dashboard

Con la consola encendida, conéctate a la red WiFi **ESPectro** (contraseña: `gameloader`) y abre `http://192.168.4.1` para ver las estadísticas en tiempo real.

## Compilar y flashear

```bash
git clone https://github.com/USUARI/tetris_espectro
cd tetris_espectro
pio run --target upload
```

**Requisitos:**
- PlatformIO
- Librería `lovyan03/LovyanGFX @ ^1.1.12`

El binario compilado se encuentra en `.pio/build/rymcu-esp32-s3-devkitc-1/firmware.bin` y puede subirse vía Game Loader sin cables.