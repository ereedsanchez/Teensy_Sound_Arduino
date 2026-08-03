/*
  Star Wars Main Theme - Space Brass Synth Edition
  Teensy 4.1 + MAX98357A I2S Amp
  
  Wiring:
    MAX98357A        Teensy 4.1
    Vin        ->    VIN (5V)
    GND        ->    GND
    DIN        ->    pin 7
    BCLK       ->    pin 21
    LRC        ->    pin 20
*/

#include <Audio.h>

// --- SYNTH GRAPHICAL ROUTING ---
AudioSynthWaveform       synth;      // Brass Oscillator
AudioFilterStateVariable filter;     // Sweeping Lowpass Filter
AudioAmplifier           ampL;
AudioAmplifier           ampR;
AudioOutputI2S           i2s1;

AudioConnection pc1(synth, 0, filter, 0);
AudioConnection pc2(filter, 0, ampL, 0); // Lowpass output (Port 0)
AudioConnection pc3(filter, 0, ampR, 0);
AudioConnection pc4(ampL, 0, i2s1, 0);
AudioConnection pc5(ampR, 0, i2s1, 1);

// Software Output Gain (Keep low as the MAX98357A adds +9dB on its own)
const float SOFTWARE_GAIN = 0.50;

// Star Wars Theme Tempo (Approx 108 BPM)
const int bpm = 108;
const float beatDuration = 60000.0 / bpm; // Duration of 1 beat in ms (approx 555.5ms)

// --- Note Frequencies (Hz) ---
#define NOTE_D4  293.66
#define NOTE_G4  392.00
#define NOTE_A4  440.00
#define NOTE_B4  493.88
#define NOTE_C5  523.25
#define NOTE_D5  587.33
#define NOTE_G5  783.99
#define REST     0.0

// Tracking previous frequency for subtle pitch glide (Portamento)
float lastFrequency = 0.0;

// Star Wars Theme Melody
float melody[] = {
  NOTE_D4, NOTE_D4, NOTE_D4,                       // Pickup triplets
  NOTE_G4, NOTE_D5,                                // Measure 1
  NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G5, NOTE_D5,     // Measure 2
  NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G5, NOTE_D5,     // Measure 3
  NOTE_C5, NOTE_B4, NOTE_C5, NOTE_A4,              // Measure 4
  REST
};

// Durations in BEATS (1.0 = Quarter Note, 2.0 = Half, 0.333 = Triplet Eighth)
float durations[] = {
  0.333, 0.333, 0.333,      // Triplet (1 beat total)
  2.0, 2.0,                 // G4 (2 beats), D5 (2 beats)
  0.333, 0.333, 0.333, 2.0, 1.0, // Triplet (1 beat), G5 (2 beats), D5 (1 beat)
  0.333, 0.333, 0.333, 2.0, 1.0, // Triplet (1 beat), G5 (2 beats), D5 (1 beat)
  0.333, 0.333, 0.333, 2.0,      // Triplet (1 beat), A4 (2 beats)
  2.0                       // Rest to complete the last bar
};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  Serial.println("=== MAX98357A Star Wars Space Synth ===");
  AudioMemory(12);

  ampL.gain(SOFTWARE_GAIN);
  ampR.gain(SOFTWARE_GAIN);

  // Initialize synth to classic sci-fi brass: Sawtooth Wave
  synth.begin(WAVEFORM_SAWTOOTH);
  synth.amplitude(0.35); // Safe scaling to prevent digital filter clipping
  synth.frequency(220);

  // Initialize filter settings
  filter.frequency(1000);
  filter.resonance(1.8); // Moderate warm resonance (brassy rather than acid-squelchy)
}

void playNote(float targetFreq, float beats) {
  int noteDuration = beatDuration * beats;
  
  if (targetFreq != REST) {
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Determine start frequency for pitch glide (Portamento)
    float startFreq = (lastFrequency > 0) ? lastFrequency : targetFreq;
    lastFrequency = targetFreq;
    
    // Update synth parameters every 4 milliseconds
    int steps = noteDuration / 4;
    for (int step = 0; step < steps; step++) {
      float progress = (float)step / steps;
      
      // 1. Pitch Glide (Portamento)
      // Glide only on larger leaps over the first 12% of the note's duration
      if (progress < 0.12) {
        float glideProgress = progress / 0.12;
        float currentFreq = startFreq + (targetFreq - startFreq) * glideProgress;
        synth.frequency(currentFreq);
      } else {
        synth.frequency(targetFreq);
      }
      
      // 2. Brass-style Volume Envelope
      // Attack (5%) -> Decay to sustain (20%) -> 100% Sustain -> Release (final 10%)
      float vol = 0.0;
      if (progress < 0.05) {
        vol = progress / 0.05; 
      } else if (progress < 0.25) {
        vol = 1.0 - (0.3 * ((progress - 0.05) / 0.20)); 
      } else if (progress < 0.90) {
        vol = 0.7; 
      } else {
        vol = 0.7 * (1.0 - ((progress - 0.90) / 0.10)); 
      }
      synth.amplitude(vol * 0.35); 
      
      // 3. Brass-swell Filter Sweep
      // Rapid filter opening to mimic brass horn "plack" onset
      float sweepFreq;
      if (progress < 0.10) {
        sweepFreq = 400.0 + (1800.0 * (progress / 0.10)); // Swell up to 2200Hz
      } else if (progress < 0.40) {
        sweepFreq = 2200.0 - (1200.0 * ((progress - 0.10) / 0.30)); // Settle down to 1000Hz
      } else {
        sweepFreq = 1000.0; // Warm sustain
      }
      filter.frequency(sweepFreq);
      
      delay(4);
    }
  } else {
    // Handle Rest
    synth.amplitude(0.0);
    digitalWrite(LED_BUILTIN, LOW);
    delay(noteDuration);
    lastFrequency = 0.0; 
  }
  
  // Strict inter-note gap to keep note articulation crisp (15% of note duration)
  synth.amplitude(0.0);
  delay(noteDuration * 0.15);
}

void loop() {
  int numNotes = sizeof(melody) / sizeof(float);
  for (int i = 0; i < numNotes; i++) {
    playNote(melody[i], durations[i]);
  }
  delay(1500); // 1.5 second pause before looping the theme
}