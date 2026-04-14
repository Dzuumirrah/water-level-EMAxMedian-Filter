#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

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

float tankZero = tankZero;
const int tankFull = 3;

float MIN_RANGE = 0.8*tankZero;                     // 20% tanki penuh
float MAX_RANGE = tankFull + 0.2*tankZero;      // 80% tanki penuh

float DISTANCE_READ = 0;             // in cm
const float SOUND_SPEED = 0.034;     // Sound speed in cm/μs
long DELAY_PRINT = 0;                // For print purpose
const int DELAY_TIME = 1500;         // in ms

// Initiate declarator
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
float getDistance();
bool buttonPressed = false;

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
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    // pinMode(BUZZER_OUT, OUTPUT);
    pinMode(LED_OUT, OUTPUT);
    pinMode(LED_MIN, OUTPUT);
    pinMode(LED_AVG, OUTPUT);
    pinMode(LED_MAX, OUTPUT);
    pinMode(CAL_PB, INPUT_PULLUP);

    if ((!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) && (USE_DISPLAY)) { // Address 0x3C for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
        
    }
    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
}

bool displayCalibrating = false; // Flag to indicate calibration message should be displayed
void loop(){
    // Read distance taken from the sensor
    float rawDistance = getMedianDistance();
    DISTANCE_READ = applyEMA(rawDistance);
    
    if ((digitalRead(CAL_PB) == LOW)) { // Button pressed
            tankZero = DISTANCE_READ;
            
            MIN_RANGE = 0.8*tankZero;
            MAX_RANGE = tankFull + 0.2*tankZero;

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


    // Check if distance read <= minimum range for activing LED and buzzer
    if (DISTANCE_READ <= MAX_RANGE) {   
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, HIGH);
        digitalWrite(LED_MAX, HIGH);
        
        digitalWrite(LED_OUT, LOW);
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

        digitalWrite(LED_OUT, HIGH);
        motorActivated = true;
    }
    
    if (millis() - DELAY_PRINT >= DELAY_TIME) {
        Serial.print(waterLevel);
        Serial.println("%");
        Serial.println(DISTANCE_READ);
        Serial.println("");
        DELAY_PRINT = millis();
    }
    
    if (USE_DISPLAY) {
        displayWaterLevel(waterLevel, motorActivated);
    }
    
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
