// dnb_knobs.ino  —  Teensy 4.1 + MAX98357A
// 174 BPM two-step drum & bass, with pots + buttons.
// Pots: A0-A3, outer legs 3.3V/GND, wiper to analog pin.
// Buttons: pin to GND, uses INPUT_PULLUP (no resistor needed).
// Serial monitor: 115200 baud.

#include <Audio.h>
#include <Bounce.h>
#include <math.h>

// ---- drums ----
AudioSynthWaveformSine   kick;
AudioSynthNoiseWhite     snareNoise;
AudioSynthNoiseWhite     hatNoise;
AudioFilterStateVariable snareFilt;
AudioFilterStateVariable hatFilt;
AudioEffectEnvelope      kickEnv, snareEnv, hatEnv;
AudioMixer4              drumMix;

// ---- reese bass: two detuned saws ----
AudioSynthWaveform       saw1, saw2;
AudioMixer4              bassMix;
AudioFilterStateVariable bassFilt;
AudioEffectEnvelope      bassEnv;

// ---- stab ----
AudioSynthWaveform       stab;
AudioEffectEnvelope      stabEnv;

AudioMixer4              finalMix;
AudioAmplifier           master;
AudioOutputI2S           i2s1;

AudioConnection d1(kick, 0, kickEnv, 0);
AudioConnection d2(kickEnv, 0, drumMix, 0);
AudioConnection d3(snareNoise, 0, snareFilt, 0);
AudioConnection d4(snareFilt, 1, snareEnv, 0);      // 1 = bandpass
AudioConnection d5(snareEnv, 0, drumMix, 1);
AudioConnection d6(hatNoise, 0, hatFilt, 0);
AudioConnection d7(hatFilt, 2, hatEnv, 0);          // 2 = highpass
AudioConnection d8(hatEnv, 0, drumMix, 2);

AudioConnection b1(saw1, 0, bassMix, 0);
AudioConnection b2(saw2, 0, bassMix, 1);
AudioConnection b3(bassMix, 0, bassFilt, 0);
AudioConnection b4(bassFilt, 0, bassEnv, 0);        // 0 = lowpass
AudioConnection b5(bassEnv, 0, finalMix, 1);

AudioConnection s1(stab, 0, stabEnv, 0);
AudioConnection s2(stabEnv, 0, finalMix, 2);

AudioConnection f1(drumMix, 0, finalMix, 0);
AudioConnection f2(finalMix, 0, master, 0);
AudioConnection f3(master, 0, i2s1, 0);
AudioConnection f4(master, 0, i2s1, 1);

// ---- controls ----
const int POT_CUTOFF = A0;   // reese filter cutoff
const int POT_RES    = A1;   // reese resonance / growl depth
const int POT_TEMPO  = A2;   // step time
const int POT_VOL    = A3;   // master volume

Bounce btnPlay = Bounce(2, 10);   // pin 2, 10ms debounce — play/stop
Bounce btnPat  = Bounce(3, 10);   // pin 3 — next bassline
Bounce btnLfo  = Bounce(4, 10);   // pin 4 — LFO wobble on/off

const float MAX_VOL         = 0.35;   // ceiling. amp adds 9dB on top.
const int   BASS_TRANSPOSE  = 12;     // 0 once you have real drivers / a sub

// ---- patterns, 32 steps = 1 bar. 2 = ghost/quiet hit ----
const int kickPat[32] = { 1,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,2,0,
                          0,0,0,0,  1,0,0,0,  0,0,0,0,  0,0,0,0 };
const int snarePat[32]= { 0,0,0,0,  0,0,2,0,  1,0,0,0,  0,0,0,0,
                          0,0,2,0,  0,0,0,0,  1,0,0,0,  0,0,2,0 };
const int hatPat[32]  = { 1,0,2,0,  1,0,2,0,  1,0,2,0,  1,0,2,0,
                          1,0,2,0,  1,0,2,0,  1,0,2,0,  1,2,1,2 };

// three basslines, switchable with button 2
const int bassPats[3][32] = {
  // classic
  {  0,-2,-2,-2,  -2,-1, 0,-2,  -2,-1,12,-2,  -1,-1, 0,-2,
    -2,-2,-1, 7,  -2,-2,-1, 0,  -2,-2,-2,-1,   5,-2,-1,12 },
  // busy
  {  0,-2,-1, 7,  -2,-1,12,-2,  -1, 0,-2,-1,   5,-2,-1, 0,
    -2,-1, 7,-2,  -1,12,-2,-1,   0,-2,-1, 5,  -2,-1, 0,-2 },
  // sparse
  {  0,-2,-2,-2,  -2,-2,-2,-2,  -2,-2,-2,-2,  -2,-2,-2,-2,
     7,-2,-2,-2,  -2,-2,-2,-2,  -2,-2,-2,-2,  -2,-2,-2,-1 }
};
const char* patNames[3] = { "classic", "busy", "sparse" };

const int stabPat[32] = { -1,-1,-1,-1,  -1,-1,12,-1,  -1,-1,-1,-1,   7,-1,-1,-1,
                          -1,-1,-1,10,  -1,-1,-1,-1,  -1, 0,-1,-1,  -1,-1,12,-1 };

// D minor-ish, 4 bars
const int roots[4] = { 26, 26, 31, 29 };   // D  D  G  F

// ---- state ----
int  step = 0, bar = 0, curPat = 0;
bool running = true, lfoOn = true;
unsigned long lastStep = 0, lastPot = 0;
float kickFreq = 0;
float smooth[4] = { 0.4, 0.5, 0.35, 0.4 };
int   stepMs = 43;   // ~174 bpm default

float midiToFreq(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

float readPot(int pin, int idx) {
  float raw = analogRead(pin) / 4095.0f;
  smooth[idx] += (raw - smooth[idx]) * 0.20f;
  return smooth[idx];
}

void allNotesOff() {
  kickEnv.noteOff(); snareEnv.noteOff(); hatEnv.noteOff();
  bassEnv.noteOff(); stabEnv.noteOff();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  analogReadResolution(12);
  analogReadAveraging(8);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  AudioMemory(40);

  kick.amplitude(0.9);
  snareNoise.amplitude(0.7);
  hatNoise.amplitude(0.5);

  snareFilt.frequency(1900);  snareFilt.resonance(1.4);
  hatFilt.frequency(7500);    hatFilt.resonance(0.8);

  saw1.begin(WAVEFORM_SAWTOOTH);  saw1.amplitude(0.75);
  saw2.begin(WAVEFORM_SAWTOOTH);  saw2.amplitude(0.75);
  bassMix.gain(0, 0.5); bassMix.gain(1, 0.5);
  bassFilt.octaveControl(3.0);

  stab.begin(WAVEFORM_SAWTOOTH);  stab.amplitude(0.4);

  kickEnv.attack(2);  kickEnv.decay(110); kickEnv.sustain(0.0); kickEnv.release(70);
  snareEnv.attack(1); snareEnv.decay(85); snareEnv.sustain(0.0); snareEnv.release(45);
  hatEnv.attack(1);   hatEnv.decay(22);   hatEnv.sustain(0.0);   hatEnv.release(10);
  bassEnv.attack(8);  bassEnv.decay(40);  bassEnv.sustain(0.9);  bassEnv.release(120);
  stabEnv.attack(2);  stabEnv.decay(140); stabEnv.sustain(0.0);  stabEnv.release(90);

  drumMix.gain(0, 0.70);   // kick
  drumMix.gain(1, 0.55);   // snare
  drumMix.gain(2, 0.56);   // hats

  finalMix.gain(0, 0.70);  // drums
  finalMix.gain(1, 0.40);  // bass
  finalMix.gain(2, 0.32);  // stab

  Serial.println("=== dnb · knobs ===");
  Serial.println("A0 cutoff  A1 growl(reso)  A2 tempo  A3 volume");
  Serial.println("btn2 play/stop   btn3 bassline   btn4 lfo wobble");
}

void loop() {
  unsigned long now = millis();

  // ---- buttons ----
  btnPlay.update(); btnPat.update(); btnLfo.update();

  if (btnPlay.fallingEdge()) {
    running = !running;
    if (!running) allNotesOff();
    Serial.println(running ? ">>> PLAY" : ">>> STOP");
  }
  if (btnPat.fallingEdge()) {
    curPat = (curPat + 1) % 3;
    Serial.print(">>> bassline: "); Serial.println(patNames[curPat]);
  }
  if (btnLfo.fallingEdge()) {
    lfoOn = !lfoOn;
    Serial.println(lfoOn ? ">>> LFO wobble ON" : ">>> manual filter");
  }

  // ---- pots, every 20 ms ----
  if (now - lastPot >= 20) {
    lastPot = now;

    float kCut  = readPot(POT_CUTOFF, 0);
    float kRes  = readPot(POT_RES,    1);
    float kTemp = readPot(POT_TEMPO,  2);
    float kVol  = readPot(POT_VOL,    3);

    // exponential cutoff: 100 Hz -> ~5.5 kHz
    float cutoff = 100.0f * powf(2.0f, kCut * 5.8f);

    if (lfoOn) {
      float lfo = 0.5f + 0.5f * sinf(now / 1400.0f);
      bassFilt.frequency(constrain(cutoff * (0.4f + 0.9f * lfo), 60.0f, 6000.0f));
    } else {
      bassFilt.frequency(cutoff);
    }

    bassFilt.resonance(0.7f + kRes * 4.3f);      // 0.7 - 5.0, the growl depth
    stepMs = (int)(70.0f - kTemp * 45.0f);       // ~25ms (fast) - 70ms (slow) per 32nd
    master.gain(kVol * MAX_VOL);
  }

  // ---- kick pitch drop ----
  if (kickFreq > 42.0f) { kickFreq *= 0.982f; kick.frequency(kickFreq); }

  // ---- sequencer ----
  if (running && now - lastStep >= (unsigned long)stepMs) {
    lastStep = now;
    int root = roots[bar];

    if (kickPat[step]) {
      kickFreq = (kickPat[step] == 2) ? 90.0f : 130.0f;
      kick.frequency(kickFreq);
      kick.amplitude(kickPat[step] == 2 ? 0.4 : 0.9);
      kickEnv.noteOn();
    }
    if (snarePat[step]) {
      snareNoise.amplitude(snarePat[step] == 2 ? 0.18 : 0.7);
      snareEnv.noteOn();
    }
    if (hatPat[step]) {
      hatNoise.amplitude(hatPat[step] == 2 ? 0.15 : 0.5);
      hatEnv.noteOn();
    }

    int bn = bassPats[curPat][step];
    if (bn >= 0) {
      float f = midiToFreq(root + bn + BASS_TRANSPOSE);
      saw1.frequency(f);
      saw2.frequency(f * 1.0075f);   // detune = the growl
      bassEnv.noteOn();
    } else if (bn == -1) {
      bassEnv.noteOff();
    }

    if (stabPat[step] >= 0) {
      stab.frequency(midiToFreq(root + stabPat[step] + 24));
      stabEnv.noteOn();
    }

    digitalWrite(LED_BUILTIN, (step % 8 == 0));

    if (++step >= 32) {
      step = 0;
      bar = (bar + 1) % 4;
      Serial.print("bar "); Serial.print(bar + 1);
      Serial.print("/4  "); Serial.print(patNames[curPat]);
      Serial.print("  cut="); Serial.print(100.0f * powf(2.0f, smooth[0] * 5.8f), 0);
      Serial.print("Hz  res="); Serial.print(0.7f + smooth[1] * 4.3f, 1);
      Serial.print("  bpm="); Serial.print((int)(60000.0f / (stepMs * 8)));
      Serial.print("  cpu="); Serial.println(AudioProcessorUsageMax(), 1);
    }
  }
}