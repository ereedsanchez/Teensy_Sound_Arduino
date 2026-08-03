// io_diagnostic.ino  —  Teensy 4.1
// Raw pot + button readout, no audio. Confirms wiring before touching the sequencer.
// Serial monitor: 115200 baud.

#include <Bounce.h>

const int POT_CUTOFF = A0;
const int POT_RES    = A1;
const int POT_TEMPO  = A2;
const int POT_VOL    = A3;

Bounce btnPlay = Bounce(2, 10);
Bounce btnPat  = Bounce(3, 10);
Bounce btnLfo  = Bounce(4, 10);

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  analogReadResolution(12);   // 0-4095

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  Serial.println("=== I/O diagnostic ===");
  Serial.println("Turn each pot fully both directions.");
  Serial.println("Idle pot ~2048, drifting a little is normal.");
  Serial.println("Idle button HIGH(1). Press -> should read LOW(0).");
  Serial.println();
}

void loop() {
  btnPlay.update();
  btnPat.update();
  btnLfo.update();

  // Print instantly on any button edge so presses are unmistakable.
  if (btnPlay.fallingEdge()) Serial.println(">>> BUTTON 2 (play) PRESSED");
  if (btnPlay.risingEdge())  Serial.println(">>> BUTTON 2 (play) released");
  if (btnPat.fallingEdge())  Serial.println(">>> BUTTON 3 (pattern) PRESSED");
  if (btnPat.risingEdge())   Serial.println(">>> BUTTON 3 (pattern) released");
  if (btnLfo.fallingEdge())  Serial.println(">>> BUTTON 4 (lfo) PRESSED");
  if (btnLfo.risingEdge())   Serial.println(">>> BUTTON 4 (lfo) released");

  // Continuous pot readout, twice a second.
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();

    int a0 = analogRead(POT_CUTOFF);
    int a1 = analogRead(POT_RES);
    int a2 = analogRead(POT_TEMPO);
    int a3 = analogRead(POT_VOL);

    Serial.print("A0(cut)="); Serial.print(a0);
    Serial.print("  A1(res)="); Serial.print(a1);
    Serial.print("  A2(tempo)="); Serial.print(a2);
    Serial.print("  A3(vol)="); Serial.print(a3);
    Serial.print("   |   btn2="); Serial.print(digitalRead(2));
    Serial.print(" btn3="); Serial.print(digitalRead(3));
    Serial.print(" btn4="); Serial.println(digitalRead(4));
  }
}