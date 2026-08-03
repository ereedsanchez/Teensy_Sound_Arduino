/*
  Ginuwine's "Pony" - Sexy Synth Edition
  Teensy 4.1 + MAX98357A I2S Amp
  
  Features: Lowpass filter sweeps, portamento (pitch glide), and ADSR envelopes.
*/

#include <Audio.h>

// --- SYNTH GRAPHICAL ROUTING ---
AudioSynthWaveform       synth;      // Our oscillator
AudioFilterStateVariable filter;     // Multi-mode filter for the squelch/wah
AudioEffectEnvelope      envelope;   // Controls note volume over time
AudioAmplifier           ampL;
AudioAmplifier           ampR;
AudioOutputI2S           i2s1;

// Route: Synth -> Filter (Lowpass) -> Envelope -> Amps -> I2S Output
AudioConnection pc1(synth, 0, filter, 0);
AudioConnection pc2(filter, 0, envelope, 0); // Port 0 of StateVariable is Lowpass
AudioConnection pc3(envelope, 0, ampL, 0);
AudioConnection pc4(envelope, 0, ampR, 0);
AudioConnection pc5(ampL, 0, i2s1, 0);
AudioConnection pc6(ampR, 0, i2s1, 1);

// Gain configuration (Keep low as the MAX98357A adds +9dB)
const float SOFTWARE_GAIN = 0.50;   

// Tempo (Approx 71 BPM)
const int bpm = 71;
const int beatDuration = 60000 / bpm; 

// --- Note Frequencies (Hz) ---
#define NOTE_FS2 93.0
#define NOTE_GS2 104.0
#define NOTE_B2  123.0
#define NOTE_CS3 139.0
#define NOTE_GS3 208.0
#define NOTE_B3  247.0
#define NOTE_CS4 277.0
#define NOTE_DS4 311.0
#define NOTE_E4  330.0
#define REST     0.0

// Tracking the previous note's pitch for portamento (glide)
float lastFrequency = 0.0;

// --- SECTION 1: Bassline ---
float bassMelody[] = {
  NOTE_CS3, NOTE_CS3, NOTE_GS2, NOTE_FS2, NOTE_FS2, NOTE_B2, REST
};
int bassDurations[] = {
  8, 8, 4, 8, 8, 4, 4
};

// --- SECTION 2: Vocal Hook ---
float vocalMelody[] = {
  NOTE_CS4, NOTE_DS4, NOTE_E4, NOTE_CS4, REST,
  NOTE_B3, NOTE_CS4, NOTE_GS3, REST,          
  NOTE_CS4, NOTE_DS4, NOTE_E4, NOTE_CS4, REST,
  NOTE_B3, NOTE_CS4, NOTE_CS4, REST           
};
int vocalDurations[] = {
  8, 8, 8, 4, 8,
  8, 8, 4, 4,
  8, 8, 8, 4, 8,
  8, 8, 4, 4
};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial && millis() < 2000) { }

  Serial.println("=== MAX98357A Sexy Synth Booted ===");
  AudioMemory(16);

  ampL.gain(SOFTWARE_GAIN);
  ampR.gain(SOFTWARE_GAIN);

  // Configure global ADSR Envelope parameters
  envelope.attack(15);    // Fast, punchy attack
  envelope.decay(120);   // Decay slightly down to sustain level
  envelope.sustain(0.7); // Let the note ring out at 70% volume
  envelope.release(100); // Smooth release tail when note stops
}

void loop() {
  // Play the squelchy bassline 3 times
  for (int i = 0; i < 3; i++) {
    playSegment(bassMelody, bassDurations, sizeof(bassMelody) / sizeof(float), true);
    delay(150); 
  }
  
  // Play the smooth vocal hook 1 time
  playSegment(vocalMelody, vocalDurations, sizeof(vocalMelody) / sizeof(float), false);
  delay(1000); 
}

// Custom player function that handles portamento and real-time filter sweeps
void playSegment(float melody[], int durations[], int length, bool isBassline) {
  for (int thisNote = 0; thisNote < length; thisNote++) {
    int noteDuration = beatDuration / (durations[thisNote] / 2);
    float targetFreq = melody[thisNote];
    
    if (targetFreq != REST) {
      // 1. SETUP SYNTH CHARACTER BASED ON PART
      if (isBassline) {
        synth.begin(WAVEFORM_SAWTOOTH); // Gritty, harmonic buzz
        filter.resonance(4.2);          // HIGH resonance for wet, squelchy "wah"
      } else {
        synth.begin(WAVEFORM_TRIANGLE); // Smooth, vocal-like flute wave
        filter.resonance(1.5);          // Milder resonance
      }

      // 2. TRIGGER AMPLITUDE ENVELOPE
      envelope.noteOn();
      digitalWrite(LED_BUILTIN, HIGH);

      // Determine where pitch glide starts
      float startFreq = (lastFrequency > 0 && isBassline) ? lastFrequency : targetFreq;
      lastFrequency = targetFreq;

      // 3. THE LIVE SYNTH LOOP (Runs during the note duration)
      int steps = noteDuration / 4; // Update filter & pitch every 4 milliseconds
      for (int step = 0; step < steps; step++) {
        float progress = (float)step / steps;

        // Portamento: Slide frequency to target over first 18% of note duration
        if (progress < 0.18) {
          float glideProgress = progress / 0.18;
          float currentFreq = startFreq + (targetFreq - startFreq) * glideProgress;
          synth.frequency(currentFreq);
        } else {
          synth.frequency(targetFreq);
        }

        // Squelchy Filter Sweep: Cutoff drops exponentially for that "wow" talkbox sound
        float sweepFreq;
        if (isBassline) {
          sweepFreq = 1400.0 * exp(-3.8 * progress) + 110.0; // Sweeps 1400Hz -> 110Hz
        } else {
          sweepFreq = 2200.0 * exp(-1.5 * progress) + 350.0; // Sweeps 2200Hz -> 350Hz
        }
        filter.frequency(sweepFreq);

        delay(4);
      }

    } else {
      // Handle Rest
      envelope.noteOff();
      digitalWrite(LED_BUILTIN, LOW);
      delay(noteDuration);
      lastFrequency = 0.0; // Reset glide on rests
    }

    // 4. NOTE OFF / RELEASE INTER-NOTE GAP
    envelope.noteOff();
    delay(noteDuration * 0.20); 
  }
}