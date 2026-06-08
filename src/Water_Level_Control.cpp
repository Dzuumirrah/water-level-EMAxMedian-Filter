#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

const int SCREEN_WIDTH = 128; // OLED display width, in pixels
const int SCREEN_HEIGHT = 64; // OLED display height, in pixels
const int OLED_RESET = -1; // Reset pin # (or -1 if sharing Arduino reset pin)

const int TRIG_PIN = 17;    //tx
const int ECHO_PIN = 16;    //rx
// const int BUZZER_OUT = 10;
const int LED_OUT = 15;
const int LED_MIN = 23;
const int LED_AVG = 19; 
const int LED_MAX = 18;
const int CAL_PB = 4; 

const bool USE_DISPLAY = true;        // Display on/off (true = display on, false = display off)
bool motorActivated = false;           // Status motor (true = on, false = off)

const char* WIFI_SSID = "pudding coklat";
const char* WIFI_PASSWORD = "12233445";
const char* MDNS_HOSTNAME = "tandon";
const char* UDP_TARGET_IP = "255.255.255.255";
const uint16_t UDP_TARGET_PORT = 4210;
const unsigned long UDP_SEND_INTERVAL = 500;

WiFiUDP udp;
WebServer webServer(80);
Preferences preferences;
bool udpEnabled = false;
unsigned long lastUdpSend = 0;
int currentWaterLevel = 0;

float tankZero = 30.0;
const int tankFull = 3;

float MIN_RANGE = 0.8 * tankZero;               // 20% tanki penuh
float MAX_RANGE = tankFull + 0.2 * tankZero;    // 80% tanki penuh

float DISTANCE_READ = 0;             // in cm
const float SOUND_SPEED = 0.034;     // Sound speed in cm/μs
long DELAY_PRINT = 0;                // For print purpose
const int DELAY_TIME = 1500;         // in ms

// Initiate declarator
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
float getDistance();
void setupWifiUdp();
void sendUdpTelemetry(int waterLevel);
void setupWebServer();
void setupMdns();
void handleRoot();
void handleApiState();
void loadCalibration();
void saveCalibration();
void updateCalibrationRanges();
bool buttonPressed = false;

const char WEB_DASHBOARD[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Water Level Monitor</title>
<style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#111827;color:#f9fafb;font-family:Segoe UI,Arial,sans-serif}main{width:min(860px,100%);min-height:100vh;display:grid;grid-template-columns:minmax(260px,340px) 1fr;gap:24px;align-items:center;padding:24px}h1{margin:0 0 8px;font-size:clamp(28px,5vw,42px);line-height:1.05}.status{margin-bottom:24px;color:#34d399}.status.offline{color:#fbbf24}.value{font:800 clamp(64px,14vw,120px)/.95 Consolas,monospace;color:#38bdf8}.meta{display:grid;gap:10px;margin-top:24px;color:#d1d5db}.meta div{display:flex;justify-content:space-between;gap:16px;border-bottom:1px solid #1f2937;padding-bottom:10px}.meta span{color:#9ca3af}.tank{position:relative;height:min(76vh,600px);min-height:380px;border:4px solid #e5e7eb;background:repeating-linear-gradient(to bottom,transparent 0,transparent calc(10% - 1px),#1f2937 calc(10% - 1px),#1f2937 10%),#0f172a;overflow:hidden}.water{position:absolute;inset:auto 0 0;height:0%;background:#0ea5e9;transition:height .24s ease,background-color .24s ease}.water:before{content:"";position:absolute;left:-5%;right:-5%;top:-12px;height:24px;background:radial-gradient(24px 14px at 24px 14px,#bae6fd 48%,transparent 52%) 0 0/48px 24px repeat-x;animation:wave 1.4s linear infinite}.labels{position:absolute;inset:18px;display:flex;flex-direction:column;justify-content:space-between;color:#6b7280;font-size:13px;text-align:right;pointer-events:none}@keyframes wave{from{transform:translateX(0)}to{transform:translateX(48px)}}@media(max-width:740px){main{grid-template-columns:1fr;align-items:start}.tank{height:54vh;min-height:320px}}
</style>
</head>
<body>
<main>
<section>
<h1>Water Level Monitor</h1>
<div id="status" class="status offline">Connecting to ESP32...</div>
<div id="value" class="value">0%</div>
<div class="meta">
<div><span>Distance</span><strong id="distance">- cm</strong></div>
<div><span>Motor</span><strong id="motor">-</strong></div>
<div><span>ESP32 IP</span><strong id="ip">-</strong></div>
</div>
</section>
<section class="tank" aria-label="Water tank">
<div id="water" class="water"></div>
<div class="labels"><span>100%</span><span>75%</span><span>50%</span><span>25%</span><span>0%</span></div>
</section>
</main>
<script>
const statusEl=document.getElementById("status"),valueEl=document.getElementById("value"),distanceEl=document.getElementById("distance"),motorEl=document.getElementById("motor"),ipEl=document.getElementById("ip"),waterEl=document.getElementById("water");
function draw(d){const level=Math.max(0,Math.min(Number(d.waterLevel||0),100));statusEl.textContent="Live from ESP32";statusEl.classList.remove("offline");valueEl.textContent=Math.round(level)+"%";distanceEl.textContent=Number(d.distanceCm||0).toFixed(2)+" cm";motorEl.textContent=d.motor?"ON":"OFF";ipEl.textContent=d.ip||"-";waterEl.style.height=level+"%";waterEl.style.backgroundColor=level<20?"#ef4444":level>85?"#f59e0b":"#0ea5e9"}
async function refresh(){try{const r=await fetch("/api/state",{cache:"no-store"});draw(await r.json())}catch(e){statusEl.textContent="ESP32 connection lost";statusEl.classList.add("offline")}}
refresh();setInterval(refresh,500);
</script>
</body>
</html>
)rawliteral";

// Display function
void displayWaterLevel(int waterLevel, bool motorActivated = false) {
    display.clearDisplay();
    // ================= LAMP INDICATOR =================
    int lampX = 55;
    int lampY = 15;
    int lampRadius = 4;

    if (motorActivated) {
        // Lamp ON → filled + glow + rays
        
        // Core lamp
        display.fillCircle(lampX, lampY, lampRadius, WHITE);

        // Glow effect (outer circle)
        display.drawCircle(lampX, lampY, lampRadius + 3, WHITE);

        // Light rays (simple animation effect)
        display.drawLine(lampX, lampY - 10, lampX, lampY - 6, WHITE);
        display.drawLine(lampX, lampY + 10, lampX, lampY + 6, WHITE);
        display.drawLine(lampX - 10, lampY, lampX - 6, lampY, WHITE);
        display.drawLine(lampX + 10, lampY, lampX + 6, lampY, WHITE);

        display.drawLine(lampX - 7, lampY - 7, lampX - 4, lampY - 4, WHITE);
        display.drawLine(lampX + 7, lampY - 7, lampX + 4, lampY - 4, WHITE);
        display.drawLine(lampX - 7, lampY + 7, lampX - 4, lampY + 4, WHITE);
        display.drawLine(lampX + 7, lampY + 7, lampX + 4, lampY + 4, WHITE);
    }
    else {
        // Lamp OFF → only outline (dim look)
        display.drawCircle(lampX, lampY, lampRadius, WHITE);
    }

    // Display "Water" on one line, "Level" on the next line
    display.setCursor(10, 5);
    display.setTextSize(1);
    display.print("Water");
    
    display.setCursor(10, 15);
    display.print("Level");
    
    display.setCursor(0, 30);
    display.setTextSize(3);
    display.print(waterLevel);
    display.print("%");
    
    // Tank Position
    int tankX = 72;  // Shifted to center
    int tankY = 5;
    int tankWidth = 55;  // Increased width
    int tankHeight = 50;
    
    // Draw tank body with rounded bottom
    display.drawRoundRect(tankX, tankY, tankWidth, tankHeight, 5, WHITE);
    
    // Draw tank lid
    display.drawLine(tankX + 5, tankY - 3, tankX + tankWidth - 5, tankY - 3, WHITE);
    display.drawLine(tankX + 5, tankY - 3, tankX, tankY, WHITE);
    display.drawLine(tankX + tankWidth - 5, tankY - 3, tankX + tankWidth, tankY, WHITE);
    
    // Draw inlet pipe on top left
    display.drawLine(tankX + 8, tankY - 6, tankX + 8, tankY - 12, WHITE);
    display.drawLine(tankX + 8, tankY - 12, tankX + 14, tankY - 12, WHITE);
    display.drawLine(tankX + 14, tankY - 12, tankX + 14, tankY - 6, WHITE);
    
    // Draw water level inside the tank
    int waterHeight = map(waterLevel, 0, 100, 0, tankHeight);
    display.fillRoundRect(tankX + 2, tankY + tankHeight - waterHeight, tankWidth - 4, waterHeight, 5, WHITE);
    
    display.display();
}

// Filter for Sensor
// ===================== FILTER CONFIG =====================
const int MEDIAN_SAMPLES = 5;        // Jumlah sampel untuk median filter
const float EMA_ALPHA = 0.3;         // Koefisien EMA (0.0-1.0), makin kecil makin halus
                                     // 0.1 = lambat & halus, 0.3 = lebih responsif

float emaDistance = -1;              // -1 = belum diinisialisasi

// ===================== MEDIAN FILTER =====================
// Buang noise spike dari sensor (misal pantulan gelombang palsu)
float getMedianDistance() {
    float samples[MEDIAN_SAMPLES];
    
    for (int i = 0; i < MEDIAN_SAMPLES; i++) {
        samples[i] = getDistance();
        delay(10); // Jeda antar pembacaan agar tidak saling interferensi
    }
    
    // Bubble sort sederhana
    for (int i = 0; i < MEDIAN_SAMPLES - 1; i++) {
        for (int j = 0; j < MEDIAN_SAMPLES - 1 - i; j++) {
            if (samples[j] > samples[j + 1]) {
                float temp = samples[j];
                samples[j] = samples[j + 1];
                samples[j + 1] = temp;
            }
        }
    }
    
    return samples[MEDIAN_SAMPLES / 2]; // Ambil nilai tengah
}

// ===================== EMA FILTER =====================
// Haluskan perubahan gradual — tangki tidak mungkin berubah drastis
// Rumus: EMA = alpha * nilai_baru + (1 - alpha) * nilai_sebelumnya
float applyEMA(float newValue) {
    if (emaDistance < 0) {
        emaDistance = newValue; // Inisialisasi pertama kali
    }
    emaDistance = EMA_ALPHA * newValue + (1 - EMA_ALPHA) * emaDistance;
    return emaDistance;
}
void setup() {
    Serial.begin(9600);
    loadCalibration();
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    setupWifiUdp();
    setupMdns();
    setupWebServer();

    if ((!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) && (USE_DISPLAY)) { // Address 0x3C for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
        
    }
    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text

    // pinMode(BUZZER_OUT, OUTPUT);
    pinMode(LED_OUT, OUTPUT);
    pinMode(LED_MIN, OUTPUT);
    pinMode(LED_AVG, OUTPUT);
    pinMode(LED_MAX, OUTPUT);
    pinMode(CAL_PB, INPUT_PULLUP);
}

bool displayCalibrating = false; // Flag to indicate calibration message should be displayed
void loop(){
    // Read distance taken from the sensor
    float rawDistance = getMedianDistance();
    DISTANCE_READ = applyEMA(rawDistance);
    
    if ((digitalRead(CAL_PB) == LOW) && (!buttonPressed)) { // Button pressed
            tankZero = DISTANCE_READ;
            updateCalibrationRanges();
            saveCalibration();

            delay(100); // Debounce delay
            buttonPressed = true; // Mark button as pressed
            displayCalibrating = true; // Start displaying calibration message
            Serial.printf("Kalibrasi dilakukan. Tinggi tangki %f cm\n", tankZero);
            return;
        } else if ((digitalRead(CAL_PB) == HIGH) && (displayCalibrating)) {
            buttonPressed = false; // Mark button as not pressed
            displayCalibrating = false; // Stop displaying calibration message
    }

    int waterLevel = map(DISTANCE_READ, tankZero, tankFull, 0, 100);
    waterLevel = constrain(waterLevel, 0, 100);
    currentWaterLevel = waterLevel;


    // Check if distance read <= minimum range for activing LED and buzzer
    if (DISTANCE_READ <= MAX_RANGE) {   
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, HIGH);
        digitalWrite(LED_MAX, HIGH);
        
        digitalWrite(LED_OUT, HIGH);
        motorActivated = false;
        // Print some information with delay
    }
    else if (DISTANCE_READ <= MIN_RANGE) {   
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, HIGH);
        digitalWrite(LED_MAX, LOW);
        
    }
    
    else { // air kosong
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, LOW);
        digitalWrite(LED_MAX, LOW);

        digitalWrite(LED_OUT, LOW);
        motorActivated = true;
    }
    
    if (millis() - DELAY_PRINT >= DELAY_TIME) {
        Serial.print(waterLevel);
        Serial.println("%");
        Serial.println(DISTANCE_READ);
        Serial.println("");
        DELAY_PRINT = millis();
    }

    if (millis() - lastUdpSend >= UDP_SEND_INTERVAL) {
        sendUdpTelemetry(waterLevel);
        lastUdpSend = millis();
    }
    
    if (USE_DISPLAY) {
        displayWaterLevel(waterLevel, motorActivated);
    }

    webServer.handleClient();
    
    delay(100);
}

// Return distance in cm
float getDistance() { 

    float DISTANCE;
    long DURATION = 0;
    // Clears the trigPin
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    // Sets the trigPin on HIGH state for 10 micro seconds
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    // Reads the echoPin, returns the sound wave travel time in microseconds
    DURATION = pulseIn(ECHO_PIN, HIGH, 30000);
    // Calculate the distance
    DISTANCE = DURATION * SOUND_SPEED/2;
    return DISTANCE; 
    }

void setupWifiUdp() {
    if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 || strlen(WIFI_SSID) == 0) {
        Serial.println("UDP disabled: set WIFI_SSID and WIFI_PASSWORD first.");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        udpEnabled = true;
        Serial.println();
        Serial.print("WiFi connected. ESP32 IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Open dashboard: http://");
        Serial.println(WiFi.localIP());
        Serial.print("Or use: http://");
        Serial.print(MDNS_HOSTNAME);
        Serial.println(".local");
        Serial.print("Sending UDP telemetry to ");
        Serial.print(UDP_TARGET_IP);
        Serial.print(":");
        Serial.println(UDP_TARGET_PORT);
    } else {
        Serial.println();
        Serial.println("WiFi connection failed. UDP telemetry disabled.");
    }
}

void sendUdpTelemetry(int waterLevel) {
    if (!udpEnabled || WiFi.status() != WL_CONNECTED) {
        return;
    }

    char payload[160];
    snprintf(
        payload,
        sizeof(payload),
        "{\"waterLevel\":%d,\"distanceCm\":%.2f,\"motor\":%s,\"ip\":\"%s\"}",
        waterLevel,
        DISTANCE_READ,
        motorActivated ? "true" : "false",
        WiFi.localIP().toString().c_str()
    );

    udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
    udp.write(reinterpret_cast<const uint8_t*>(payload), strlen(payload));
    udp.endPacket();
}

void setupWebServer() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Web dashboard disabled: WiFi is not connected.");
        return;
    }

    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/api/state", HTTP_GET, handleApiState);
    webServer.onNotFound([]() {
        webServer.send(404, "text/plain", "Not found");
    });
    webServer.begin();
    Serial.println("ESP32 web dashboard started.");
}

void setupMdns() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("mDNS disabled: WiFi is not connected.");
        return;
    }

    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("mDNS started: http://");
        Serial.print(MDNS_HOSTNAME);
        Serial.println(".local");
    } else {
        Serial.println("mDNS failed to start.");
    }
}

void handleRoot() {
    webServer.send_P(200, "text/html", WEB_DASHBOARD);
}

void handleApiState() {
    char payload[160];
    snprintf(
        payload,
        sizeof(payload),
        "{\"waterLevel\":%d,\"distanceCm\":%.2f,\"motor\":%s,\"ip\":\"%s\"}",
        currentWaterLevel,
        DISTANCE_READ,
        motorActivated ? "true" : "false",
        WiFi.localIP().toString().c_str()
    );

    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "application/json", payload);
}

void loadCalibration() {
    preferences.begin("water-level", false);
    tankZero = preferences.getFloat("tankZero", tankZero);
    updateCalibrationRanges();

    Serial.print("Loaded tankZero calibration: ");
    Serial.print(tankZero);
    Serial.println(" cm");
}

void saveCalibration() {
    preferences.putFloat("tankZero", tankZero);

    Serial.print("Saved tankZero calibration: ");
    Serial.print(tankZero);
    Serial.println(" cm");
}

void updateCalibrationRanges() {
    MIN_RANGE = 0.8 * tankZero;
    MAX_RANGE = tankFull + 0.2 * tankZero;
}
