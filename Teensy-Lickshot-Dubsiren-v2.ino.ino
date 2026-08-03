// dub_siren.ino  —  Teensy 4.1 + MAX98357A
// Classic dub siren: hold the button, pitch wails up and down.
// Pots A0-A3, buttons on 2/3/4 — same wiring as before.
// Serial monitor: 115200 baud.

#include <Audio.h>
#include <Bounce.h>
#include <math.h>

AudioSynthWaveform   osc;
AudioEffectEnvelope   env;
AudioAmplifier        master;
AudioOutputI2S         i2s1;

AudioConnection c1(osc, 0, env, 0);
AudioConnection c2(env, 0, master, 0);
AudioConnection c3(master, 0, i2s1, 0);
AudioConnection c4(master, 0, i2s1, 1);

// ---- controls ----
const int POT_RATE  = A0;   // how fast the siren wails
const int POT_DEPTH = A1;   // how wide the pitch sweep is
const int POT_BASE  = A2;   // center pitch
const int POT_VOL   = A3;   // volume

Bounce btnFire = Bounce(2, 5);    // pin 2 — HOLD to sound the siren
Bounce btnWave = Bounce(3, 10);   // pin 3 — cycle waveform
Bounce btnMode = Bounce(4, 10);   // pin 4 — sweep mode: up-only / up-down

const float MAX_VOL = 0.45;

const short waveforms[3] = { WAVEFORM_SINE, WAVEFORM_TRIANGLE, WAVEFORM_SAWTOOTH };
const char* waveNames[3] = { "sine", "triangle", "saw" };
int curWave = 0;

bool  upDownMode = true;   // true = wobble up/down, false = ramp up and hold
bool  firing = false;
float phase = 0;           // 0..1 sweep position
float smooth[4] = { 0.3, 0.4, 0.4, 0.4 };

float readPot(int pin, int idx) {
  float raw = analogRead(pin) / 4095.0f;
  smooth[idx] += (raw - smooth[idx]) * 0.25f;
  return smooth[idx];
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  analogReadResolution(12);
  analogReadAveraging(8);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  AudioMemory(12);

  osc.begin(waveforms[curWave]);
  osc.amplitude(0.8);

  env.attack(3);
  env.decay(10);
  env.sustain(1.0);
  env.release(120);   // the tail when you let go — the wet echo-y part

  Serial.println("=== dub siren ===");
  Serial.println("A0 rate   A1 depth   A2 base pitch   A3 volume");
  Serial.println("HOLD btn2 to fire   btn3 waveform   btn4 sweep mode");
}

void loop() {
  btnFire.update();
  btnWave.update();
  btnMode.update();

  if (btnWave.fallingEdge()) {
    curWave = (curWave + 1) % 3;
    osc.begin(waveforms[curWave]);
    Serial.print(">>> waveform: "); Serial.println(waveNames[curWave]);
  }
  if (btnMode.fallingEdge()) {
    upDownMode = !upDownMode;
    Serial.println(upDownMode ? ">>> mode: up-down wobble" : ">>> mode: ramp & hold");
  }

  // fire = held, not pressed-once. Real dub sirens are momentary.
  bool held = (digitalRead(2) == LOW);
  if (held && !firing) {
    firing = true;
    phase = 0;
    env.noteOn();
    Serial.println(">>> FIRE");
  }
  if (!held && firing) {
    firing = false;
    env.noteOff();
    Serial.println(">>> release");
  }

  float rate  = readPot(POT_RATE,  0);   // 0..1
  float depth = readPot(POT_DEPTH, 1);   // 0..1
  float base  = readPot(POT_BASE,  2);   // 0..1
  float vol   = readPot(POT_VOL,   3);   // 0..1

  float baseFreq  = 80.0f + base * 500.0f;     // 80 - 580 Hz center
  float sweepHz   = 40.0f + depth * 900.0f;    // how wide the wail swings
  float rateHz    = 0.4f + rate * 5.0f;        // 0.4 - 5.4 Hz wobble speed

  if (firing) {
    phase += (rateHz / 1000.0f) * 16.0f;       // ~60Hz loop tick estimate
    if (upDownMode) {
      float lfo = sinf(phase * 2.0f * PI);         // -1..1
      osc.frequency(baseFreq + sweepHz * lfo);
    } else {
      float ramp = fminf(phase, 1.0f);             // 0..1, then holds
      osc.frequency(baseFreq + sweepHz * ramp);
    }
  }

  master.gain(vol * MAX_VOL);

  static unsigned long lastDbg = 0;
  if (millis() - lastDbg >= 500) {
    lastDbg = millis();
    Serial.print(firing ? "FIRING " : "idle    ");
    Serial.print(" base="); Serial.print(baseFreq, 0);
    Serial.print("Hz depth="); Serial.print(sweepHz, 0);
    Serial.print("Hz rate="); Serial.print(rateHz, 1);
    Serial.print("Hz vol="); Serial.println(vol, 2);
  }
}