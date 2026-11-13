#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display configuration (128x64 I2C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions for Arduino Nano
const int SIGNAL_PIN = A0;   // Analog signal input probe
const int POT_PIN = A1;      // Potentiometer for sampling rate / pause control
const int LED_PIN = 9;       // PWM-capable pin for dimming control
const int TEST_PIN = 3;      // Built-in square wave test signal output

// State variables
bool isPaused = false;
int waveform[SCREEN_WIDTH];
int sampleDelay = 0;

void setup() {
  // Set ADC prescaler to 16 for stable, fast readings (16MHz / 16 = 1MHz ADC clock)
  ADCSRA = (ADCSRA & 0xF8) | 0x04;

  // Initialize Serial Monitor for debugging
  Serial.begin(9600);

  // Configure hardware pins
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0); // LED off initially

  // Configure Test Signal Generator on Pin 3 (Jumper Pin 3 to A0 to test!)
  pinMode(TEST_PIN, OUTPUT);
  analogWrite(TEST_PIN, 127); // 50% duty cycle square wave (~490Hz)

  // Initialize I2C and boost speed to 400kHz for smooth updates
  Wire.begin();
  Wire.setClock(400000);

  // Initialize the OLED display (I2C address 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // Halt if initialization fails
  }

  // Flip the OLED screen 180 degrees (upside down if needed)
  display.setRotation(2);
  
  // Startup splash screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 25);
  display.println(F("Nano Oscilloscope"));
  display.display();
  delay(1000);
}

void loop() {
  // 1. Read potentiometer and handle pause state
  int potValue = analogRead(POT_PIN);

  if (potValue < 20) {
    isPaused = true;
    sampleDelay = 0;
  } else {
    isPaused = false;
    // Map potentiometer range to a microsecond delay for finer control
    sampleDelay = map(potValue, 20, 1023, 0, 3000); 
  }

  // 2. Set LED brightness (dim glow when paused)
  if (isPaused) {
    analogWrite(LED_PIN, 60); 
  } else {
    analogWrite(LED_PIN, 0);
  }

  // 3. RAPID BURST SAMPLING WITH STABLE TRIGGER
  if (!isPaused) {
    unsigned long timeout = millis();
    
    // Software Trigger: Wait for signal to go low (below middle threshold 512)
    while (analogRead(SIGNAL_PIN) > 512) {
      if (millis() - timeout > 30) break; // Prevent hanging on DC signals
    }
    
    timeout = millis(); // Reset timeout for rising edge
    
    // Wait for signal to flip high (rising edge trigger)
    while (analogRead(SIGNAL_PIN) < 512) {
      if (millis() - timeout > 30) break;
    }

    // Capture all 128 screen pixels in a single tight block
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      int rawVal = analogRead(SIGNAL_PIN);
      waveform[i] = map(rawVal, 0, 1023, SCREEN_HEIGHT - 1, 12);
      
      // If user turned the pot, add a tiny microsecond delay between samples
      if (sampleDelay > 0) {
        delayMicroseconds(sampleDelay);
      }
    }
  }

  // 4. Refresh and draw the OLED user interface
  display.clearDisplay();

  // Draw status header at the top
  display.setCursor(0, 0);
  if (isPaused) {
    display.print(F("PAUSED"));
  } else {
    display.print(F("RUN"));
  }

  // Display sampling time delay on the right
  display.setCursor(62, 0);
  display.print(F("Dly:"));
  display.print(sampleDelay);
  display.print(F("us"));

  // Draw separator line below the header
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Render the crisp waveform graph
  for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
    display.drawLine(i, waveform[i], i + 1, waveform[i + 1], SSD1306_WHITE);
  }

  display.display();
}