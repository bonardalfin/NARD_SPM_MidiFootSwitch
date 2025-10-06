////////////////////////////////////////////////////////////////////////////////
// Created by Bonard Alfin 2025
// bonardalfinproject.blogspot.com

#include <Arduino.h>
#include <Control_Surface.h>
#include <MIDI_Interfaces/BluetoothMIDI_Interface.hpp>

// MIDI interface
BluetoothMIDI_Interface midi_ble;

// ------------------------- MODES -------------------------
enum Mode {
  MODE_A_PRESET = 0,
  MODE_B_EFFECT = 1,
  MODE_C_LOOPER = 2
};
Mode currentMode = MODE_A_PRESET;

int currentBank = 1;
int lastPreset = 1; // Track last pressed preset for blinking
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long BLINK_INTERVAL = 500; // Blink every 500ms

// ===== DEBOUNCE SETTINGS  =====
const unsigned long DEBOUNCE_DELAY = 20; // 20ms debounce delay
// ========================================================================

// ------------------------- LED PINS -------------------------
const uint8_t RGB_R_PIN = 32;  // MODE A - Red
const uint8_t RGB_G_PIN = 13;  // MODE B - Green  
const uint8_t RGB_B_PIN = 15;  // MODE C - Blue

const uint8_t ledPins[8] = {4, 16, 17, 5, 18, 23, 19, 22}; // LED1-LED8
unsigned long lastBlinkTime = 0;
bool blinkState = false;

// ------------------------- BUTTON MATRIX -------------------------
const uint8_t rowPins[3] = {12, 14, 27};   // row pins
const uint8_t colPins[3] = {26, 25, 33};   // column pins
const uint8_t MATRIX_ROWS = 3;
const uint8_t MATRIX_COLS = 3;

// Button matrix state
bool buttonMatrix[MATRIX_ROWS][MATRIX_COLS];
bool prevButtonMatrix[MATRIX_ROWS][MATRIX_COLS];

const uint8_t buttonMapping[9][2] = {
  {0, 0}, // BTN1
  {1, 0}, // BTN2
  {1, 1}, // BTN3
  {2, 1}, // BTN4 
  {0, 1}, // BTN5 
  {0, 2}, // BTN6 
  {1, 2}, // BTN7
  {2, 2}, // BTN8 
  {2, 0}  // BTN9 (Mode)
};

// ------------------------- BUTTON STATE WITH DEBOUNCE -------------------------
struct BtnState {
  bool pressed = false;
  bool longPressTriggered = false;
  unsigned long pressTime = 0;
  
  // DEBOUNCE VARIABLES
  bool lastRawState = false;
  bool debouncedState = false;
  bool prevDebouncedState = false;
  unsigned long lastDebounceTime = 0;
};

BtnState btnState[9];

// ------------------------- MODE B EFFECT STATES -------------------------
bool effectStates[8] = {false,false,false,false,false,false,false,false};

// Array for CC numbers in Mode B Effect
const uint8_t effectCCNumbers[8] = {43, 44, 45, 48, 49, 50, 51, 58};
const char* effectNames[8] = {"NR", "FX1", "DRV", "EQ", "FX2", "DLY", "RVB", "TUNER"};

// ------------------------- MODE A PRESET ARRAYS -------------------------
// Preset values ​​for each bank (8 banks, 8 presets each)
const uint8_t presetValues[8][8] = {
  {1, 2, 3, 4, 5, 6, 7, 8},          // Bank 1: values 1-8
  {9, 10, 11, 12, 13, 14, 15, 16},   // Bank 2: values 9-16  
  {17, 18, 19, 20, 21, 22, 23, 24},  // Bank 3: values 17-24
  {25, 26, 27, 28, 29, 30, 31, 32},  // Bank 4: values 25-32
  {33, 34, 35, 36, 37, 38, 39, 40},  // Bank 5: values 33-40
  {41, 42, 43, 44, 45, 46, 47, 48},  // Bank 6: values 41-48
  {49, 50, 51, 52, 53, 54, 55, 56},  // Bank 7: values 49-56
  {57, 58, 59, 60, 61, 62, 63, 64}   // Bank 8: values 57-64
};

// ------------------------- LOOPER CC MAPPING -------------------------
struct LooperCC {
  uint8_t cc;
  uint8_t value;
};
LooperCC looperMapping[8] = {
  {25,127}, // BTN1 PresetPlus
  {60,127}, // BTN2 RecordLooper
  {62,127}, // BTN3 PlayLooper
  {62,0},   // BTN4 StopLooper
  {24,127}, // BTN5 PresetMin
  {64,127}, // BTN6 DeleteLooper
  {67,127}, // BTN7 PreLooper
  {67,0}    // BTN8 PostLooper
};
const uint8_t CC_LOOPER_MODE = 59; // CC#59 for Looper on/off

// ------------------------- POTS -------------------------
const uint8_t potPins[3] = {2, 34, 35}; // Potensiometer 1, 2, 3
int potValues[3] = {0,0,0};           // Last sent values

// ------------------------- FUNCTION DECLARATIONS -------------------------
void switchToModeA();
void switchToModeB();  
void switchToModeC();
void exitLooperToModeA();
void readButtonMatrix();
void debounceButtons();
bool isButtonPressed(int buttonIndex);
bool isButtonJustPressed(int buttonIndex);
bool isButtonJustReleased(int buttonIndex);

// ------------------------- SETUP -------------------------
void setup() {
  midi_ble.setName("NARD_SPM");
  Serial.begin(115200);
  Control_Surface.begin();

  // Init button matrix pins
  for(int i = 0; i < MATRIX_ROWS; i++) {
    pinMode(rowPins[i], INPUT_PULLUP);
  }
  for(int i = 0; i < MATRIX_COLS; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], HIGH);
  }

  // Initialize matrix state
  for(int i = 0; i < MATRIX_ROWS; i++) {
    for(int j = 0; j < MATRIX_COLS; j++) {
      buttonMatrix[i][j] = false;
      prevButtonMatrix[i][j] = false;
    }
  }

  // Initialize button states with debounce
  for(int i = 0; i < 9; i++) {
    btnState[i].lastRawState = false;
    btnState[i].debouncedState = false;
    btnState[i].prevDebouncedState = false;
    btnState[i].lastDebounceTime = 0;
  }

  // Init LED pins
  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);
  for(int i=0;i<8;i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Init potentiometers
  for (int i = 0; i < 3; i++) {
    pinMode(potPins[i], INPUT);
    potValues[i] = map(analogRead(potPins[i]), 0, 4095, 0, 100);
  }

  updateRGBLED();
  updateButtonLEDs();

  Serial.println("ESP32-S3 MIDI Controller Started");
  Serial.println("MODE A - PRESET MODE - BANK 1");
  Serial.println("Button mapping fixed for physical layout");
  Serial.printf("Button debounce delay: %lu ms\n", DEBOUNCE_DELAY);
}

// ------------------------- LOOP -------------------------
void loop() {
  Control_Surface.loop();

  readButtonMatrix();
  debounceButtons();
  handleModeButton();
  handleBlinking();

  switch(currentMode){
    case MODE_A_PRESET: handlePresetMode(); break;
    case MODE_B_EFFECT: handleEffectMode(); break;
    case MODE_C_LOOPER: handleLooperMode(); break;
  }

  handlePots();
  updateButtonLEDs();
}

// ------------------------- BUTTON MATRIX FUNCTIONS -------------------------
void readButtonMatrix() {
  // Copy current state to previous
  for(int i = 0; i < MATRIX_ROWS; i++) {
    for(int j = 0; j < MATRIX_COLS; j++) {
      prevButtonMatrix[i][j] = buttonMatrix[i][j];
    }
  }

  // Read new state
  for(int col = 0; col < MATRIX_COLS; col++) {
    // Set current column LOW, others HIGH
    for(int c = 0; c < MATRIX_COLS; c++) {
      digitalWrite(colPins[c], (c == col) ? LOW : HIGH);
    }
    
    delayMicroseconds(10); // Small delay for pin settling
    
    // Read all rows for this column
    for(int row = 0; row < MATRIX_ROWS; row++) {
      buttonMatrix[row][col] = !digitalRead(rowPins[row]); // Inverted because of pullup
    }
  }
  
  // Set all columns HIGH again
  for(int c = 0; c < MATRIX_COLS; c++) {
    digitalWrite(colPins[c], HIGH);
  }
}

// ------------------------- DEBOUNCE FUNCTIONS -------------------------
void debounceButtons() {
  for(int i = 0; i < 9; i++) {
    int row = buttonMapping[i][0];
    int col = buttonMapping[i][1];
    bool rawState = buttonMatrix[row][col];
    
    // If the raw state changes, reset the debounce timer.
    if (rawState != btnState[i].lastRawState) {
      btnState[i].lastDebounceTime = millis();
    }
    
    // If it has passed the debounce delay, update the state.
    if ((millis() - btnState[i].lastDebounceTime) > DEBOUNCE_DELAY) {
      // Update previous state before changing current state
      btnState[i].prevDebouncedState = btnState[i].debouncedState;
      btnState[i].debouncedState = rawState;
    }
    
    btnState[i].lastRawState = rawState;
  }
}


bool isButtonPressed(int buttonIndex) {
  if(buttonIndex < 0 || buttonIndex >= 9) return false;
  return btnState[buttonIndex].debouncedState;
}

bool isButtonJustPressed(int buttonIndex) {
  if(buttonIndex < 0 || buttonIndex >= 9) return false;
  return btnState[buttonIndex].debouncedState && !btnState[buttonIndex].prevDebouncedState;
}

bool isButtonJustReleased(int buttonIndex) {
  if(buttonIndex < 0 || buttonIndex >= 9) return false;
  return !btnState[buttonIndex].debouncedState && btnState[buttonIndex].prevDebouncedState;
}

// ------------------------- MODE SWITCHING FUNCTIONS -------------------------
void switchToModeA() {
  currentMode = MODE_A_PRESET;
  updateRGBLED();
  Serial.print("MODE A - PRESET MODE - BANK "); 
  Serial.println(currentBank);
}

void switchToModeB() {
  currentMode = MODE_B_EFFECT;
  // Reset effect states when entering mode B
  for(int i = 0; i < 8; i++) {
    effectStates[i] = false;
  }
  updateRGBLED();
  Serial.println("MODE B - EFFECT MODE");
}

void switchToModeC() {
  currentMode = MODE_C_LOOPER;
  midi_ble.sendCC({CC_LOOPER_MODE, CHANNEL_1}, 127);
  updateRGBLED();
  Serial.println("MODE C - LOOPER MODE ON");
}

void exitLooperToModeA() {
  currentMode = MODE_A_PRESET;
  midi_ble.sendCC({CC_LOOPER_MODE, CHANNEL_1}, 0);
  updateRGBLED();
  Serial.print("MODE C - LOOPER MODE OFF -> MODE A - BANK ");
  Serial.println(currentBank);
}

// ------------------------- LED FUNCTIONS -------------------------
void updateRGBLED() {
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);

  switch(currentMode) {
    case MODE_A_PRESET: digitalWrite(RGB_R_PIN, HIGH); break;
    case MODE_B_EFFECT: digitalWrite(RGB_G_PIN, HIGH); break;
    case MODE_C_LOOPER: digitalWrite(RGB_B_PIN, HIGH); break;
  }
}

void handleBlinking() {
  unsigned long currentTime = millis();
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlinkTime = currentTime;
  }
}

void updateButtonLEDs() {
  switch(currentMode) {
    case MODE_A_PRESET: updatePresetLEDs(); break;
    case MODE_B_EFFECT: updateEffectLEDs(); break;
    case MODE_C_LOOPER: updateLooperLEDs(); break;
  }
}

void updatePresetLEDs() {
  for(int i = 0; i < 8; i++) {
    bool shouldBeOn = false;
    if(i + 1 == currentBank) shouldBeOn = true;
    if(i + 1 == lastPreset) shouldBeOn = blinkState;
    digitalWrite(ledPins[i], shouldBeOn ? HIGH : LOW);
  }
}

void updateEffectLEDs() {
  for(int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], effectStates[i] ? HIGH : LOW);
  }
}

void updateLooperLEDs() {
  for(int i = 0; i < 8; i++) {
    bool shouldBeOn = false;
    if(i == 1) shouldBeOn = blinkState;        // BTN2 blink
    else if(i == 2 || i == 3 || i == 5) shouldBeOn = true; // BTN3,4,6 on
    digitalWrite(ledPins[i], shouldBeOn ? HIGH : LOW);
  }
}

// ------------------------- MODE BUTTON (BTN9) -------------------------
void handleModeButton(){
  int modeButtonIndex = 8; // BTN9

  // Press detection
  if(isButtonJustPressed(modeButtonIndex)){
    btnState[modeButtonIndex].pressed = true;
    btnState[modeButtonIndex].pressTime = millis();
    btnState[modeButtonIndex].longPressTriggered = false;
  }

  // Check long press WHILE STILL PRESSED
  if(btnState[modeButtonIndex].pressed && !btnState[modeButtonIndex].longPressTriggered){
    unsigned long duration = millis() - btnState[modeButtonIndex].pressTime;
    if(duration >= LONG_PRESS_TIME){
      btnState[modeButtonIndex].longPressTriggered = true;
      
      // Long press action - execute immediately
      if(currentMode != MODE_C_LOOPER){
        switchToModeC();
      } else {
        exitLooperToModeA(); // ALWAYS return to MODE A
      }
    }
  }

  // Release handling - only for short press
  if(isButtonJustReleased(modeButtonIndex)){
    if(btnState[modeButtonIndex].pressed && !btnState[modeButtonIndex].longPressTriggered){
      // Short press → toggle between Mode A and B (NOT including Mode C)
      if(currentMode == MODE_A_PRESET){
        switchToModeB();
      } else if(currentMode == MODE_B_EFFECT){
        switchToModeA();
      }
      // If in Mode C, short press does nothing
    }
    
    // Reset state
    btnState[modeButtonIndex].pressed = false;
    btnState[modeButtonIndex].longPressTriggered = false;
  }
}

// ------------------------- MODE A - PRESET MODE -------------------------
void handlePresetMode(){
  for(int i = 0; i < 8; i++){
    // Button pressed
    if(isButtonJustPressed(i)){
      btnState[i].pressed = true;
      btnState[i].pressTime = millis();
      btnState[i].longPressTriggered = false;
    }

    // Check long press for bank change
    if(btnState[i].pressed && !btnState[i].longPressTriggered){
      if(millis() - btnState[i].pressTime >= LONG_PRESS_TIME){
        currentBank = i + 1;  // Change to bank (BTN1=Bank1, BTN2=Bank2, etc.)
        btnState[i].longPressTriggered = true;
        Serial.printf("BANK CHANGED TO: %d\n", currentBank);
      }
    }

    // Button released
    if(isButtonJustReleased(i)){
      if(btnState[i].pressed && !btnState[i].longPressTriggered){
        // Short press → send preset on release
        uint8_t presetValue = presetValues[currentBank - 1][i];
        lastPreset = i + 1;
        midi_ble.sendCC({1, CHANNEL_1}, presetValue);
        Serial.printf("PRESET %d (Value: %d) from BANK %d\n", i+1, presetValue, currentBank);
      }
      btnState[i].pressed = false;
      btnState[i].longPressTriggered = false;
    }
  }
}

// ------------------------- MODE B - EFFECT MODE -------------------------
void handleEffectMode(){
  for(int i = 0; i < 8; i++){
    if(isButtonJustPressed(i)){
      btnState[i].pressed = true;
      effectStates[i] = !effectStates[i];
      
      // Send MIDI CC
      uint8_t ccValue = effectStates[i] ? 127 : 0;
      midi_ble.sendCC({effectCCNumbers[i], CHANNEL_1}, ccValue);
      
      Serial.printf("EFFECT %s (BTN%d) → %s\n", effectNames[i], i+1, effectStates[i] ? "ON" : "OFF");
    }
    if(isButtonJustReleased(i)) {
      btnState[i].pressed = false;
    }
  }
}

// ------------------------- MODE C - LOOPER MODE -------------------------
void handleLooperMode(){
  for(int i = 0; i < 8; i++){
    if(isButtonJustPressed(i)){
      btnState[i].pressed = true;
      midi_ble.sendCC({looperMapping[i].cc, CHANNEL_1}, looperMapping[i].value);
      Serial.printf("LOOPER BTN%d → CC#%d Value:%d\n", i+1, looperMapping[i].cc, looperMapping[i].value);
    }
    if(isButtonJustReleased(i)) {
      btnState[i].pressed = false;
    }
  }
}

// ------------------------- POTS (with smoothing + deadband) -------------------------
void handlePots() {
  const int samples = 10;     // smoothing: number of samples
  const int deadband = 2;     // deadband: tolerance of differences

  for (int i = 0; i < 3; i++) {
    long sum = 0;
    for (int j = 0; j < samples; j++) {
      sum += analogRead(potPins[i]);
    }
    int raw = sum / samples;
    int mappedVal = map(raw, 0, 4095, 0, 100);

    if (abs(mappedVal - potValues[i]) > deadband) {
      potValues[i] = mappedVal;
      
      switch (currentMode) {
        case MODE_A_PRESET:
        case MODE_B_EFFECT:
          if (i == 0) {
            midi_ble.sendCC({6, CHANNEL_1}, mappedVal);  // Master Vol
            Serial.printf("POT%d (Master Vol) → CC#6: %d\n", i+1, mappedVal);
          }
          else if (i == 1) {
            midi_ble.sendCC({7, CHANNEL_1}, mappedVal); // Preset Vol
            Serial.printf("POT%d (Preset Vol) → CC#7: %d\n", i+1, mappedVal);
          }
          break;
          
        case MODE_C_LOOPER:
          if (i == 0) {
            midi_ble.sendCC({6, CHANNEL_1}, mappedVal);   // Master Vol
            Serial.printf("POT%d (Master Vol) → CC#6: %d\n", i+1, mappedVal);
          }
          else if (i == 1) {
            midi_ble.sendCC({65, CHANNEL_1}, mappedVal); // Looper Rec
            Serial.printf("POT%d (Looper Rec) → CC#65: %d\n", i+1, mappedVal);
          }
          else if (i == 2) {
            midi_ble.sendCC({66, CHANNEL_1}, mappedVal); // Looper Play
            Serial.printf("POT%d (Looper Play) → CC#66: %d\n", i+1, mappedVal);
          }
          break;
      }
    }
  }
}



