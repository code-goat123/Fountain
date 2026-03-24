# ESP32 Wireless Pump Controller

Turn a pump **ON** / **OFF** from a phone or browser on your **home Wi‑Fi**. The ESP32 hosts a tiny web page (port **80**); no cloud and no external APIs.

**Why ESP32:** Built-in Wi‑Fi, Arduino support, one chip for Wi‑Fi + HTTP + GPIO.

## Safety

- **Never power the pump from the ESP32 GPIO.** The pin is only a **logic signal** to a **relay or MOSFET driver**.
- The pump needs its **own supply** (right voltage/current); the driver switches that circuit. Follow your module’s datasheet.

## What’s included

- Web UI at `/` with ON/OFF and status; `/on` and `/off` control the output
- Optional mDNS: `http://esp32-pump.local/` (works on some networks/devices)
- Pump **OFF** at boot; state lasts until power is removed
- Default control pin: **GPIO 4** (`PUMP_CONTROL_PIN`); set `RELAY_ACTIVE_LOW` to `true` if your relay is active-low

## Parts

ESP32 board, USB cable, relay or MOSFET driver (rated for the pump), pump + **separate** pump supply, jumpers, common GND per your wiring.

## Wiring (short)

1. **GPIO 4** → relay IN / gate driver input.  
2. **GND** shared as required by the relay, driver, and pump supply.  
3. Pump power through the relay/MOSFET only—not from the ESP32 I/O pins.

## Setup

1. Arduino IDE → Preferences → Additional board URLs → add  
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`  
   → Boards Manager → install **esp32** (Espressif).
2. **Tools → Board:** your ESP32 (e.g. ESP32 Dev Module). Open `esp32-pump-control/esp32-pump-control.ino`.
3. Set Wi‑Fi at the top of the sketch:

```cpp
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

4. USB in → **Tools → Port** → **Upload**. Serial Monitor **115200** → note **`[Wi-Fi] IP address:`**.

## Using the page

On the **same Wi‑Fi**, open `http://<that-ip>/` (example: `http://192.168.1.50/`). You can try `http://esp32-pump.local/`.

**Do not use `localhost` on your phone/laptop**—that is *this* device, not the ESP32. Use the ESP32’s IP or `esp32-pump.local` when it resolves.

Use at your own risk; you are responsible for safe wiring and loads.
