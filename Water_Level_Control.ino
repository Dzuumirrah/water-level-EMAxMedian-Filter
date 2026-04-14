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

const bool USE_DISPLAY = true;        // Display on/off (true = display on, false = display off)

const int MIN_RANGE = 30;           // in cm
const int MAX_RANGE = 10;           // in cm

const int tankHeight = 40
const int tankFloor = 5;
float DISTANCE_READ = 0;             // in cm
const float SOUND_SPEED = 0.034;     // Sound speed in cm/μs
long DELAY_PRINT = 0;                // For print purpose
const int DELAY_TIME = 1500;         // in ms

// Initiate declarator
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
float getDistance();

// Display function
void displayWaterLevel(int waterLevel) {
    display.clearDisplay();

    // Display "Water" on one line, "Level" on the next line
    display.setCursor(20, 5);
    display.setTextSize(1);
    display.print("Water");

    display.setCursor(20, 15);
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

void setup() {
    Serial.begin(9600);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    // pinMode(BUZZER_OUT, OUTPUT);
    pinMode(LED_OUT, OUTPUT);
    pinMode(LED_MIN, OUTPUT);
    pinMode(LED_AVG, OUTPUT);
    pinMode(LED_MAX, OUTPUT);

    if ((!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) && (USE_DISPLAY)) { // Address 0x3C for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever

        display.clearDisplay();
        display.setTextSize(1);             // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE);        // Draw white text
    }
}

void loop(){
    // Read distance taken from the sensor
    DISTANCE_READ = getDistance();

    int waterLevel = map(DISTANCE_READ, tankHeight, tankFloor, 0, 100);
    waterLevel = constrain(waterLevel, 0, 100);


    // Check if distance read <= minimum range for activing LED and buzzer
    if (DISTANCE_READ > MIN_RANGE) {   // Distance > 150cm
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, LOW);
        digitalWrite(LED_MAX, LOW);
        
        digitalWrite(LED_OUT, HIGH);
    }

    else if (DISTANCE_READ > MAX_RANGE) { // Distance < 150
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, HIGH);
        digitalWrite(LED_MAX, LOW);
    }
    else {
        // Turning on buzzer and LED
        // digitalWrite(BUZZER_OUT, HIGH);
        digitalWrite(LED_MIN, HIGH);
        digitalWrite(LED_AVG, HIGH);
        digitalWrite(LED_MAX, HIGH);
        
        digitalWrite(LED_OUT, LOW);
        // Print some information with delay
    }
    
    if (millis() - DELAY_PRINT >= DELAY_TIME) {
        Serial.print(waterLevel);
        Serial.println("%");
        DELAY_PRINT = millis();
    }

    if (USE_DISPLAY) {
        displayWaterLevel(waterLevel);
    }
    
    // else {
    //     // Turning off buzzer and LED
    //     // digitalWrite(BUZZER_OUT, LOW);
    //     digitalWrite(LED_OUT, LOW);
    //     // Print some information with delay
    //     if (millis() - DELAY_PRINT >= DELAY_TIME) {
    //         Serial.println("(X) Lampu dan Buzzer mati!");
    //         DELAY_PRINT = millis();
    //     }
    // }
    
    Serial.println(DISTANCE_READ);
    Serial.println("");
    delay(1000);
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
