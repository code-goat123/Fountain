/*
 * =============================================================================
 * ESP32 Wi-Fi Pump Control (local web UI + relay/MOSFET driver)
 * =============================================================================
 *
 * WIRING (read before connecting anything)
 * -----------------------------------------
 * - Pump power: The pump MUST have its own power supply rated for the pump’s
 *   voltage and current. Do NOT power the pump from the ESP32 3.3 V or 5 V pins.
 * - Control signal: Connect ESP32 GPIO (see PUMP_CONTROL_PIN below) to the
 *   relay module “IN” pin, or to the input of your MOSFET gate driver circuit.
 * - Ground: Tie ESP32 GND to the common ground shared by the relay/MOSFET
 *   driver and the pump’s power supply (per your module’s datasheet).
 * - Relay/MOSFET: The ESP32 pin only switches the relay coil or MOSFET; the
 *   relay/MOSFET switches the pump’s high-current path.
 *
 * IMPORTANT — power and switching
 * --------------------------------
 * The pump needs its own proper power supply (voltage/current per pump specs)
 * and suitable switching hardware (relay module or MOSFET driver). The ESP32
 * GPIO only provides a low-current logic signal; it must never power the pump
 * directly or carry motor current.
 *
 * SOFTWARE SETUP (Arduino IDE)
 * ----------------------------
 * 1) Install ESP32 board support:
 *    File → Preferences → Additional boards manager URLs → add:
 *    https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *    Then Tools → Board → Boards Manager → search “esp32” → install
 *    “esp32” by Espressif Systems.
 * 2) Select your board: Tools → Board → (your ESP32 module, e.g. “ESP32 Dev Module”).
 * 3) Set Wi-Fi: edit WIFI_SSID and WIFI_PASSWORD near the top of this sketch.
 * 4) Upload: connect USB, pick the correct port (Tools → Port), click Upload.
 * 5) Open Serial Monitor at 115200 baud to see “[Wi-Fi] Connected.” and the
 *    “[Wi-Fi] IP address:” line with your ESP32’s local IP.
 *
 * ACCESSING THE WEB PAGE FROM PHONE/LAPTOP
 * ----------------------------------------
 * Use the ESP32’s IP from Serial Monitor (e.g. http://192.168.1.50/) or, if
 * mDNS works on your network, http://esp32-pump.local/
 * Do NOT use “localhost” or 127.0.0.1 on your phone/laptop — that refers to
 * the device you’re holding, not the ESP32. The ESP32 is another host on Wi-Fi.
 *
 * =============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// -----------------------------------------------------------------------------
// Wi-Fi credentials — change these to match your network
// -----------------------------------------------------------------------------
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// -----------------------------------------------------------------------------
// Hardware: GPIO that drives the relay IN or MOSFET gate driver (logic only)
// -----------------------------------------------------------------------------
// Many relay boards are “active LOW” (relay energizes when GPIO is LOW). If your
// pump turns on when the pin should be “off”, set RELAY_ACTIVE_LOW to true.
static const uint8_t PUMP_CONTROL_PIN = 4;
static const bool RELAY_ACTIVE_LOW = false;

// mDNS hostname → http://esp32-pump.local/ (when supported by your OS/router)
static const char *MDNS_HOSTNAME = "esp32-pump";

// HTTP server on standard port 80
WebServer server(80);

// Pump state while ESP32 is powered (starts OFF for safety)
static bool pumpIsOn = false;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
static void applyPinForPumpState(bool on);
static void setPumpOn(void);
static void setPumpOff(void);
static String buildIndexHtml(void);
static void logRequest(const char *path);

// -----------------------------------------------------------------------------
// Pump control — ESP32 only signals the relay/MOSFET; pump uses its own supply
// -----------------------------------------------------------------------------
static void applyPinForPumpState(bool on) {
  // SAFETY: The pump is powered by its own supply through the relay/MOSFET.
  // The ESP32 pin must never carry pump load current—only the driver input.
  bool pinHigh = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(PUMP_CONTROL_PIN, pinHigh ? HIGH : LOW);
}

static void setPumpOn(void) {
  if (!pumpIsOn) {
    pumpIsOn = true;
    applyPinForPumpState(true);
    Serial.println(F("[PUMP] turned ON"));
  }
}

static void setPumpOff(void) {
  if (pumpIsOn) {
    pumpIsOn = false;
    applyPinForPumpState(false);
    Serial.println(F("[PUMP] turned OFF"));
  }
}

// -----------------------------------------------------------------------------
// HTML for "/" — simple, centered, large buttons, inline CSS
// -----------------------------------------------------------------------------
static String buildIndexHtml(void) {
  const char *statusText = pumpIsOn ? "ON" : "OFF";
  const char *statusClass = pumpIsOn ? "on" : "off";

  String html;
  html.reserve(2048);
  html += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += F("<title>Pump Control</title><style>");
  html += F("*,*::before,*::after{box-sizing:border-box;}");
  html += F("body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;");
  html += F("font-family:system-ui,-apple-system,sans-serif;background:#0f1419;color:#e6edf3;}");
  html += F(".card{width:100%;max-width:420px;padding:24px;border-radius:16px;");
  html += F("background:#161b22;box-shadow:0 8px 32px rgba(0,0,0,.35);}");
  html += F("h1{margin:0 0 8px;font-size:1.75rem;font-weight:700;text-align:center;}");
  html += F(".sub{margin:0 0 20px;text-align:center;font-size:.9rem;color:#8b949e;}");
  html += F(".status{margin:0 0 24px;padding:16px;border-radius:12px;text-align:center;");
  html += F("font-size:1.25rem;font-weight:600;}");
  html += F(".status.on{background:#1a3d2e;color:#3fb950;border:1px solid #238636;}");
  html += F(".status.off{background:#2d333b;color:#adbac7;border:1px solid #444c56;}");
  html += F(".row{display:flex;gap:12px;}");
  html += F("a.btn{flex:1;display:block;padding:20px 16px;border-radius:12px;text-align:center;");
  html += F("font-size:1.25rem;font-weight:700;text-decoration:none;color:#fff;}");
  html += F("a.on{background:#238636;}a.on:active{background:#2ea043;}");
  html += F("a.off{background:#da3633;}a.off:active{background:#f85149;}");
  html += F("</style></head><body><div class=\"card\">");
  html += F("<h1>Pump Control</h1><p class=\"sub\">Local Wi-Fi control</p>");
  html += F("<p class=\"status ");
  html += statusClass;
  html += F("\">Pump is <strong>");
  html += statusText;
  html += F("</strong></p><div class=\"row\">");
  html += F("<a class=\"btn on\" href=\"/on\">ON</a>");
  html += F("<a class=\"btn off\" href=\"/off\">OFF</a>");
  html += F("</div></div></body></html>");
  return html;
}

static void logRequest(const char *path) {
  Serial.print(F("[HTTP] "));
  Serial.print(WiFi.localIP());
  Serial.print(path);
  Serial.print(F(" from "));
  Serial.println(server.client().remoteIP());
}

static void handleRoot(void) {
  logRequest(" GET /");
  server.send(200, "text/html", buildIndexHtml());
}

static void handleOn(void) {
  logRequest(" GET /on");
  setPumpOn();
  server.sendHeader(F("Location"), F("/"));
  server.send(302, F("text/plain"), F(""));
}

static void handleOff(void) {
  logRequest(" GET /off");
  setPumpOff();
  server.sendHeader(F("Location"), F("/"));
  server.send(302, F("text/plain"), F(""));
}

static void handleNotFound(void) {
  Serial.print(F("[HTTP] 404 "));
  Serial.println(server.uri());
  server.send(404, F("text/plain"), F("Not found"));
}

void setup(void) {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("ESP32 Pump Control — starting"));

  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  // Safe default: de-energize relay / keep pump off before Wi-Fi comes up
  pumpIsOn = false;
  applyPinForPumpState(false);
  Serial.println(F("[BOOT] Pump defaulted to OFF (safe start)"));

  Serial.print(F("[Wi-Fi] Connecting to "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    if (++dots >= 40) {
      dots = 0;
      Serial.println();
    }
  }
  Serial.println();
  Serial.println(F("[Wi-Fi] Connected."));
  Serial.print(F("[Wi-Fi] IP address: "));
  Serial.println(WiFi.localIP());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.print(F("[mDNS] http://"));
    Serial.print(MDNS_HOSTNAME);
    Serial.println(F(".local/  (if your device supports mDNS)"));
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println(F("[mDNS] failed to start — use the IP address above"));
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/on", HTTP_GET, handleOn);
  server.on("/off", HTTP_GET, handleOff);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("[HTTP] Server listening on port 80"));
}

void loop(void) {
  server.handleClient();
}
