// dub_siren_tape_echo.ino  —  Teensy 4.1 + MAX98357A
// Dub siren + emulated tape echo: filtered feedback loop + wow modulation.
// Same wiring as before. Serial monitor: 115200 baud.

#include <Audio.h>
#include <Bounce.h>
#include <math.h>

AudioSynthWaveform       osc;
AudioEffectEnvelope      env;

// ---- tape echo chain ----
AudioMixer4              feedbackMix;    // dry signal + returning echo -> delay input
AudioEffectDelay         echoDelay;      // the tape loop itself
AudioFilterStateVariable echoFilt;       // darkens each repeat, like tape HF loss
AudioAmplifier            echoFeedback;   // how much comes back around
AudioMixer4              outMix;         // dry + wet -> master

AudioAmplifier            master;
AudioOutputI2S            i2s1;

AudioConnection c1(osc, 0, env, 0);
AudioConnection c2(env, 0, feedbackMix, 0);      // dry in
AudioConnection c3(echoFeedback, 0, feedbackMix, 1); // feedback return
AudioConnection c4(feedbackMix, 0, echoDelay, 0);
AudioConnection c5(echoDelay, 0, echoFilt, 0);   // 0 = lowpass output
AudioConnection c6(echoFilt, 0, echoFeedback, 0);
AudioConnection c7(env, 0, outMix, 0);           // dry to output
AudioConnection c8(echoFilt, 0, outMix, 1);       // wet to output
AudioConnection c9(outMix, 0, master, 0);
AudioConnection c10(master, 0, i2s1, 0);
AudioConnection c11(master, 0, i2s1, 1);

// ---- controls ----
const int POT_RATE  = A0;   // siren wobble speed
const int POT_DEPTH = A1;   // siren pitch sweep width
const int POT_BASE  = A2;   // siren center pitch
const int POT_VOL   = A3;   // master volume

Bounce btnFire = Bounce(2, 5);
Bounce btnWave = Bounce(3, 10);
Bounce btnMode = Bounce(4, 10);

const float MAX_VOL = 0.45;

// ---- tape echo character — tweak these ----
const float ECHO_TIME_MS   = 380.0f;   // gap between repeats
const float ECHO_FEEDBACK  = 0.42f;    // 0=one repeat, 0.7+=long trail, 0.95+=self-oscillate
const float ECHO_DAMPING_HZ= 2200.0f;  // lower = darker/warmer repeats, more "tape"
const float WOW_DEPTH_MS   = 5.0f;     // pitch wobble amount on repeats
const float WOW_RATE_HZ    = 0.35f;    // wobble speed — slow, uneven, tape-like

const short waveforms[3] = { WAVEFORM_SINE, WAVEFORM_TRIANGLE, WAVEFORM_SAWTOOTH };
const char* waveNames[3] = { "sine", "triangle", "saw" };
int curWave = 0;

bool  upDownMode = true;
bool  firing = false;
float phase = 0;
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

  AudioMemory(30);   // more blocks needed — the echo chain adds several nodes

  osc.begin(waveforms[curWave]);
  osc.amplitude(0.8);

  env.attack(3);
  env.decay(10);
  env.sustain(1.0);
  env.release(120);

  echoDelay.delay(0, ECHO_TIME_MS);
  echoFilt.frequency(ECHO_DAMPING_HZ);
  echoFilt.resonance(0.7);
  echoFeedback.gain(ECHO_FEEDBACK);

  feedbackMix.gain(0, 1.0);   // dry input, full
  feedbackMix.gain(1, 1.0);   // feedback return — echoFeedback already scales it

  outMix.gain(0, 0.75);   // dry
  outMix.gain(1, 0.55);   // wet

  Serial.println("=== dub siren + tape echo ===");
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
    Serial.println(">>> release — listen to the tail");
  }

  float rate  = readPot(POT_RATE,  0);
  float depth = readPot(POT_DEPTH, 1);
  float base  = readPot(POT_BASE,  2);
  float vol   = readPot(POT_VOL,   3);

  float baseFreq = 80.0f + base * 500.0f;
  float sweepHz  = 40.0f + depth * 900.0f;
  float rateHz   = 0.4f + rate * 5.0f;

  if (firing) {
    phase += (rateHz / 1000.0f) * 16.0f;
    if (upDownMode) {
      float lfo = sinf(phase * 2.0f * PI);
      osc.frequency(baseFreq + sweepHz * lfo);
    } else {
      float ramp = fminf(phase, 1.0f);
      osc.frequency(baseFreq + sweepHz * ramp);
    }
  }

  // tape wow: slowly wobble the delay time itself, not just the source pitch
  float wow = ECHO_TIME_MS + WOW_DEPTH_MS * sinf(millis() * WOW_RATE_HZ * 0.0063f);
  echoDelay.delay(0, wow);

  master.gain(vol * MAX_VOL);
}