#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      // Sub-bass body (808 fundamental)
AudioSynthWaveform       waveform2;      // Ethereal high harmonic tone
AudioMixer4              mixer1;         // Blends the sub-bass and high tone
AudioEffectEnvelope      envelope1;      // Long, sustained 808 decay envelope
AudioFilterStateVariable filter1;        // Softens and warms the sound
AudioOutputI2S           i2s2;           // Audio shield headphone output

AudioConnection          patchCord1(waveform1, 0, mixer1, 0);
AudioConnection          patchCord2(waveform2, 0, mixer1, 1);
AudioConnection          patchCord3(mixer1, 0, filter1, 0);
AudioConnection          patchCord4(filter1, 0, envelope1, 0);
AudioConnection          patchCord5(envelope1, 0, i2s2, 0);
AudioConnection          patchCord6(envelope1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     
// GUItool: end automatically generated code

// Pin Assignments
const int KNOB_PITCH = A10;   // Controls 808 tuning
const int KNOB_CUTOFF = A11;  // Controls filter warmth/brightness
const int KNOB_VOL = A12;     // Master volume

const int BTN_1 = 30;         // Button 1: Pitch down (deeper sub)
const int BTN_2 = 31;         // Button 2: Pitch up
const int BTN_3 = 32; 

const int SW_1 = 28;          
const int SW_2 = 29;

const int PIEZO_PIN = A14;

// REVERSED TRIGGER CONFIGURATION: Floats at ~204, triggers when dropped below 170
const int PIEZO_THRESHOLD_LOW = 170; 
unsigned long lastTriggerTime = 0;
const int DEBOUNCE_DELAY = 100; // Cooldown between hits

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000);
  Serial.println("--- 808 Synth with Ethereal Top Started ---");

  AudioMemory(20);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.6);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(SW_1, INPUT_PULLUP);
  pinMode(SW_2, INPUT_PULLUP);

  // Setup 808 Sub-Bass Waveform (Sine)
  waveform1.begin(WAVEFORM_SINE);
  waveform1.amplitude(0.0);
  
  // Setup Ethereal High Tone (Sine or Triangle for glassy sheen)
  waveform2.begin(WAVEFORM_TRIANGLE);
  waveform2.amplitude(0.0);

  // Mixer gains (blend heavy sub-bass with a subtle ethereal high layer)
  mixer1.gain(0, 0.8); // Sub-bass weight
  mixer1.gain(1, 0.2); // Ethereal high sheen

  // Long, sustained 808 style envelope (warm decay)
  envelope1.attack(2);
  envelope1.hold(50);
  envelope1.decay(400);     // Longer decay for that booming 808 tail
  envelope1.sustain(0.1);   // Slight sustain tail
  envelope1.release(100);

  filter1.resonance(1.5);   
}

void loop() {
  // Read Controls
  float knobPitchVal = (float)analogRead(KNOB_PITCH) / 1023.0;
  float knobCutoffVal = (float)analogRead(KNOB_CUTOFF) / 1023.0;
  float knobVolVal = (float)analogRead(KNOB_VOL) / 1023.0;

  sgtl5000_1.volume(knobVolVal);

  // Filter cutoff controls the warmth/muffled tone of the 808
  float cutoffFreq = 200.0 + (knobCutoffVal * 3000.0);
  filter1.frequency(cutoffFreq);

  float pitchModifier = 1.0;
  if (digitalRead(BTN_1) == LOW) pitchModifier = 0.7;   // Sub drop mode
  if (digitalRead(BTN_2) == HIGH) pitchModifier = 1.3;   // Higher pitch mode
  if (digitalRead(BTN_3) == LOW) pitchModifier = PIEZO_PIN * 0.1 * (random (0.1, 2));

  // --- REVERSED PIEZO TRIGGER CHECK ---
  int rawPiezo = analogRead(PIEZO_PIN);

  // Trigger occurs when the reading drops below 170 (since it floats at 204)
  if (rawPiezo < PIEZO_THRESHOLD_LOW && (millis() - lastTriggerTime > DEBOUNCE_DELAY)) {
    
    // Calculate hit depth (how far below 204 it dropped) to measure strike velocity
    int dropAmount = 204 - rawPiezo;
    Serial.print("808 Hit! Value drop: "); Serial.println(dropAmount);
    
    // Classic 808 base frequency range (around 40Hz to 120Hz)
    float baseFreq = (45.0 + (knobPitchVal * 80.0)) * pitchModifier;
    
    waveform1.frequency(baseFreq);
    // Ethereal high tone sits an exact octave or harmonic interval above it (e.g., 3x or 2x frequency)
    waveform2.frequency(baseFreq * 2.96); // Slightly detuned harmonic for spacey/ethereal shimmer

    // Scale amplitude based on how hard you struck it
    float hitAmplitude = constrain((float)dropAmount / 30.0, 0.3, 1.0);
    waveform1.amplitude(hitAmplitude);
    waveform2.amplitude(hitAmplitude * 0.4); // Keep the high sheen softer

    // Trigger envelope
    envelope1.noteOn();
    
    lastTriggerTime = millis();
  }
  
  // Cut audio amplitude to zero after the 808 tail finishes ringing out
  if (millis() - lastTriggerTime > 600) {
    waveform1.amplitude(0.0);
    waveform2.amplitude(0.0);
  }
}
