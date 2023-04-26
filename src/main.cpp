/**
 * Nadel-Timer für Arduino Uno/Nano by cm@make-magazin.de
 * Für Multi Function Shield, siehe
 * https://www.makershop.de/download/arduino-multi-function-shield.pdf
 *
 * Taster A1: Reset Counter
 * Taster A2: Eingang Detektor
 * Taster A3: Fast Forward (setzte Anfangs-Stundenzahl)
 * LED D1: Zähler aktiv
 * LED D4: Betrieb
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "Ticker.h"
#include <EEPROM.h>
#include <MultiFuncShield.h>

#define WHITE 0
#define BLACK 1
#define RESET_BTN A1
#define AUDIO_DETECT A2
#define FAST_FORWARD A3
// Anzeigezeit und Save-Timeout in Zehntelsekunden
#define DISPLAY_TIME 20
#define SAVE_TIMEOUT 300

void ms100_tick();  // forward-Deklaration
Ticker count_timer(ms100_tick, 100, 0, MILLIS);

volatile uint16_t msec100;
volatile long minutes_counted;
uint16_t audio_was_detected = 0;
uint16_t save_request = 0;
uint16_t audio_timeout = DISPLAY_TIME;
uint16_t save_timeout = SAVE_TIMEOUT;
uint16_t autorepeat_inc = 0;

void eepromWriteLong(long lo, int adr) {
// long int Wert in das EEPROM schreiben
  byte by;
  for(int i=0;i< 4;i++) {
    by = (lo >> ((3-i)*8)) & 0x000000ff;
    EEPROM.write(adr+i, by);
  }
} // eepromWriteLong

long eepromReadLong(int adr) {
// long int Wert aus 4 Byte EEPROM lesen
  long lo=0;
  for(int i=0;i< 3;i++){
    lo += EEPROM.read(adr+i);
    lo = lo << 8;
  }
  lo += EEPROM.read(adr+3);
  return lo;
} // eepromReadLong

/* display all values on ePaper */
void display_all(uint32_t val_minutes, uint16_t active) {
  Serial.println("update display");
  MFS.write(val_minutes/60, 0);
  if (active) {
    MFS.writeLeds(LED_1, ON);  // blinkende LED
    // bei LCD ggf. Plattenspieler-Symbol zeichnen
  } else {
    MFS.writeLeds(LED_1, OFF);
  }
}

void ms100_tick() {
  // Ticker Zehntelsekunden
  msec100++;
  if (audio_timeout) {
    audio_timeout--; // jede Zehntelsekunde
  }
  if (save_timeout) {
    save_timeout--;  // jede Zehntelsekunde
  }
  if (msec100 == 600) {
    msec100 = 0;
    if (audio_was_detected) {
      minutes_counted++;
    }
  }
}

void setup() {
  Serial.begin(9600);
  count_timer.start();
  Timer1.initialize();
  MFS.initialize(&Timer1);
  MFS.writeLeds(LED_ALL, OFF);
  MFS.writeLeds(LED_4, ON);
  MFS.blinkLeds(LED_1, ON);  // blinkt erst, wenn auch eingeschaltet
  pinMode(AUDIO_DETECT, INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);
  Serial.println("init done");
  if (EEPROM.read(12) == 0x55) {
    minutes_counted = eepromReadLong(4);
  } else {
    minutes_counted = 0;
    eepromWriteLong(minutes_counted, 4);
    EEPROM.write(12, 0x55);
  }
  display_all(minutes_counted, 0);
}

void loop() {
  count_timer.update();
  if (digitalRead(RESET_BTN) == LOW) {
    MFS.write("RSET");
    minutes_counted = 0;
    Serial.println("reset counter");
    eepromWriteLong(minutes_counted, 4);
    audio_was_detected = 0;
    while (digitalRead(RESET_BTN) == LOW) {
      delay(10);  // chores processing
    }
    display_all(minutes_counted, 0);
    Serial.println("btn released");
  }

  if (digitalRead(FAST_FORWARD) == LOW) {
    save_request = 1;
    save_timeout = DISPLAY_TIME;  // nach 2 Sekunden speichern
    if (autorepeat_inc < 100) {
      autorepeat_inc++;
      minutes_counted += 4;
    } else {
      minutes_counted += 15;
    }
    display_all(minutes_counted, 0);
  } else {
    autorepeat_inc = 0;
  }

  if (digitalRead(AUDIO_DETECT) == LOW) {
    // Retriggerbarer Timer
    audio_was_detected = 1;
    save_request = 1;
    if (audio_timeout == 0) {
      Serial.println("audio started");
      display_all(minutes_counted, audio_was_detected);
    }
    audio_timeout = DISPLAY_TIME; // min. 2 Sekunden anzeigen
    save_timeout = SAVE_TIMEOUT;  // nach 30 Sekunden speichern
  }

  if (audio_was_detected && (audio_timeout == 0)) {
    Serial.println("audio stopped");
    display_all(minutes_counted, 0);
    audio_was_detected = 0;
  }

  if (save_request && (save_timeout == 0)) {
    Serial.println("save to eeprom");
    eepromWriteLong(minutes_counted, 4);
    save_request = 0;
  }

  delay(20);
}
