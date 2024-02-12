/*
 * CADBoard, a Mini keyboard with just the important bits
 * Use an Arduino Micro or similar for this.
 * Credit for the HID code belongs to Konstantin Schauwecker, and "qwelyt" - https://github.com/qwelyt/Keyboard/tree/master/Code/ModuleA/V2/ModuleA
 * 
 * Copyright (c) 2020, Hans Liss
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "KeyboardLib.h"

#define BTN_CTRL_PIN 9        // buttons included on shield
#define BTN_ALT_PIN 8        // these are Arduino pin numbers
#define BTN_DEL_PIN 7

#define LED_PWR_PIN 6
#define LED_CTRL_PIN 5
#define LED_ALT_PIN 3
#define LED_DEL_PIN 10

#define BTN_CTRL 0
#define BTN_ALT 1
#define BTN_DEL 2

const int buttons[] = { BTN_CTRL_PIN, BTN_ALT_PIN, BTN_DEL_PIN};

// See below; we can't handle more than 6 simultaneous keys here at the moment, since
// we are always sending a single report. It's perfectly possible to update this to handle
// arbitrarily long strings. I just don't want to.
// Or maybe it will work anyway, actually. I haven't tried it. Whatever.
#define REGCOUNT 7

int reg[REGCOUNT];

void setup() {
  initButtons();
  initKeyboard();
  pinMode(LED_PWR_PIN, OUTPUT);
  analogWrite(LED_PWR_PIN, 255);
  pinMode(LED_CTRL_PIN, OUTPUT);
  pinMode(LED_ALT_PIN, OUTPUT);
  pinMode(LED_DEL_PIN, OUTPUT);
  for (int i=0; i<80; i++) {
    analogWrite(LED_CTRL_PIN, i);
    delay(5);
  }
  for (int i=0; i<80; i++) {
    analogWrite(LED_ALT_PIN, i);
    delay(5);
  }
  for (int i=0; i<80; i++) {
    analogWrite(LED_DEL_PIN, i);
    delay(5);
  }
  
  for (int i = 0; i < REGCOUNT; i++) {
    reg[i] = 0;
  }
  /*
  Serial.begin(9600);
  while(!Serial) {
    ;
  }
  */
}

void loop() {
  static int oldmreg=0;
  static int oldbreg=0;
  static boolean hadButton = false;
  readButtons();
  reg[REGCOUNT-1] = 0;
  reg[0] = 0;
  if (isButtonPressed(BTN_CTRL)) {
    reg[REGCOUNT-1] |= 0x01;
  }
  if (isButtonPressed(BTN_ALT)) {
    reg[REGCOUNT-1] |= 0x04;
  }
  
  if (isButtonPressed(BTN_DEL)) {
    reg[0] = 0x4c;
  }
  
  if (oldbreg != reg[0] || oldmreg != reg[REGCOUNT-1]) {
    sendCode(reg, 0, reg[REGCOUNT-1]);
    if (reg[0]) {
      sendCode(reg, 1, reg[REGCOUNT-1]);
    } else {
      sendCode(reg, 0, reg[REGCOUNT-1]);
    }
  }
  oldbreg = reg[0];
  oldmreg = reg[REGCOUNT-1];
  delay(20);
}

#pragma region "Button handling code"

#define NBUTTONS (sizeof(buttons)/sizeof(int))
int buttonState[NBUTTONS];
boolean isButtonRead[NBUTTONS];
int lastButtonState[NBUTTONS];
int lastDebounceTime[NBUTTONS];
unsigned long debounceDelay = 10;    // the debounce time; increase if the output flickers

void readButton(int bNo) 
{
  int reading = digitalRead(buttons[bNo]);
  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState[bNo]) 
  {
    // reset the debouncing timer
    lastDebounceTime[bNo] = millis();
  }

  if ((millis() - lastDebounceTime[bNo]) > debounceDelay) 
  {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState[bNo]) 
    {
      buttonState[bNo] = reading;
      isButtonRead[bNo] = false;
    }
  }
  lastButtonState[bNo] = reading;
}

void readButtons() {
  for (int i = 0; i < NBUTTONS; i++) {
    readButton(i);
  }
}

void initButtons() {
  for (int i = 0; i < NBUTTONS; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    buttonState[i] = HIGH;
    lastButtonState[i] = HIGH;
    isButtonRead[i] = true;
  }
}

boolean hasButtonPressEvent(int bNo) {
  if (buttonState[bNo] == LOW && !isButtonRead[bNo]) {
    isButtonRead[bNo] = true;
    return true;
  }
  else {
    return false;
  }
}

boolean isButtonPressed(int bNo) {
  isButtonRead[bNo] = true;
  return buttonState[bNo] == LOW;
}

#pragma endregion "Button handling code"

#pragma region "HID code"

const byte keyLimit = 6;
byte keyPlace = 0;
uint8_t keyBuf[keyLimit];
uint8_t meta = 0x0;
void addKeyToBuffer(uint8_t key) {
  keyBuf[keyPlace++] = key;
}

void sendCode(int code[], int numkeys, int metaIn) {
  resetKeys();
  for (int i=0; i < numkeys; i++) {
    addKeyToBuffer(code[i]);
  }
  meta = metaIn;
  sendBuffer(meta, keyBuf);
}

void sendBuffer(uint8_t meta, uint8_t keyBuf[]) {
  sendKeyBuffer(meta, keyBuf);
  resetKeys();
}

void resetKeys() {
  for (byte b = 0; b < keyLimit; ++b) {
    keyBuf[b] = 0x0;
  }
  keyPlace = 0;
  meta = 0x0;
}

#pragma endregion "HID code"
