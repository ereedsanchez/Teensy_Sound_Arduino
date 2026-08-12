#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      // Sub-bass body (808 fundamental)
AudioSynthWaveform       waveform2;      // Ethereal high harmonic tone
AudioMixer4              mixer1;         // Blends the sub-bass and high tone
AudioFilterStateVariable filter1;        // Shapes warmth (Lowpass / Bandpass)
AudioEffectBitcrusher    bitcrusher1;    // Adds retro gritty distortion when enabled
AudioEffectEnvelope      envelope1;      // Long, sustained 808 decay envelope
AudioOutputI2S           i2s2;           // Audio shield headphone output

AudioConnection          patchCord1(waveform1, 0, mixer1, 0);
AudioConnection          patchCord2(waveform2, 0, mixer1, 1);
AudioConnection          patchCord3(mixer1, 0, filter1, 0);
// Route filter through bitcrusher, then envelope, then out
AudioConnection          patchCord4(filter1, 0, bitcrusher1, 0);
AudioConnection          patchCord5(bitcrusher1, 0, envelope1, 0);
AudioConnection          patchCord6(envelope1, 0, i2s2, 0);
AudioConnection          patchCord7(envelope1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     
// GUItool: end automatically generated code

// Pin Assignments
const int KNOB_PITCH = A10;   
const int KNOB_CUTOFF = A11;  
const int KNOB_VOL = A12;     

const int BTN_1 = 30;         // Button 1 (Inverted)
const int BTN_2 = 31;         // Button 2 (Standard)
const int BTN_3 = 32;         // Button 3 (Inverted)

const int SW_1 = 28;          // Switch 1: Filter Mode (Lowpass vs Bandpass)
const int SW_2 = 29;          // Switch 2: Bitcrusher Grime Effect (ON/OFF)

const int PIEZO_PIN = A14;

// Piezo configuration (Floats at ~204, triggers when dropped below 170)
const int PIEZO_THRESHOLD_LOW = 170; 
unsigned long lastTriggerTime = 0;
const int DEBOUNCE_DELAY = 100; 

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000);
  Serial.println("--- 808 Synth with Switch Effects Started ---");

  AudioMemory(20);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.6);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(SW_1, INPUT_PULLUP);
  pinMode(SW_2, INPUT_PULLUP);

  // Setup Waveforms
  waveform1.begin(WAVEFORM_SINE);
  waveform1.amplitude(0.0);
  
  waveform2.begin(WAVEFORM_TRIANGLE);
  waveform2.amplitude(0.0);

  // Mixer gains
  mixer1.gain(0, 0.8); 
  mixer1.gain(1, 0.2); 

  // Bitcrusher setup (bits, sample rate)
  bitcrusher1.bits(8);       // Crushes audio down to gritty 8-bit depth when active
  bitcrusher1.sampleRate(22050); 

  // Long, sustained 808 style envelope
  envelope1.attack(2);
  envelope1.hold(50);
  envelope1.decay(400);     
  envelope1.sustain(0.1);   
  envelope1.release(100);

  filter1.resonance(2.0);   
}

void loop() {
  // Read Controls
  float knobPitchVal = (float)analogRead(KNOB_PITCH) / 1023.0;
  float knobCutoffVal = (float)analogRead(KNOB_CUTOFF) / 1023.0;
  float knobVolVal = (float)analogRead(KNOB_VOL) / 1023.0;

  sgtl5000_1.volume(knobVolVal);

  float cutoffFreq = 200.0 + (knobCutoffVal * 3000.0);
  filter1.frequency(cutoffFreq);

  // --- SWITCH EFFECTS ---
  // Switch 1: Filter Type (LOW = Bandpass for a sharp click, HIGH = Lowpass for warm bass)
  if (digitalRead(SW_1) == LOW) {
    // Read from the Bandpass output of the filter (Index 1)
    // Note: patchCord4 needs to map filter1.output1 instead of output0 if using bandpass, 
    // but a cleaner way via software control is adjusting filter resonance or changing behavior.
    // Let's use Switch 1 to toggle a resonant "punch boost":
    filter1.resonance(4.5); // Sharp resonant punch
  } else {
    filter1.resonance(1.5); // Standard warm body
  }

  // Switch 2: Bitcrusher Grime Effect (LOW = Enabled, HIGH = Disabled/Bypassed)
  if (digitalRead(SW_2) == LOW) {
    bitcrusher1.bits(6);       // Heavy 6-bit crunch
    bitcrusher1.sampleRate(11025); // Lo-fi sample rate reduction
  } else {
    bitcrusher1.bits(16);      // Clean 16-bit (effectively bypasses crunch)
    bitcrusher1.sampleRate(44100); 
  }

  // --- BUTTON LOGIC ---
  bool btn1Pressed = (digitalRead(BTN_1) == HIGH); // Inverted
  bool btn2Pressed = (digitalRead(BTN_2) == LOW);  // Standard
  bool btn3Pressed = (digitalRead(BTN_3) == HIGH); // Inverted

  float pitchModifier = 1.0;
  if (btn1Pressed) pitchModifier = 0.7;   
  if (btn2Pressed) pitchModifier = 1.3;   

  if (btn3Pressed) {
    mixer1.gain(2, 0.5); 
  } else {
    mixer1.gain(2, 0.0); 
  }

  // --- REVERSED PIEZO TRIGGER CHECK ---
  int rawPiezo = analogRead(PIEZO_PIN);

  if (rawPiezo < PIEZO_THRESHOLD_LOW && (millis() - lastTriggerTime > DEBOUNCE_DELAY)) {
    
    int dropAmount = 204 - rawPiezo;
    Serial.print("808 Hit! Drop: "); Serial.println(dropAmount);
    
    float baseFreq = (45.0 + (knobPitchVal * 80.0)) * pitchModifier;
    
    waveform1.frequency(baseFreq);
    waveform2.frequency(baseFreq * 2.96); 

    float hitAmplitude = constrain((float)dropAmount / 30.0, 0.3, 1.0);
    waveform1.amplitude(hitAmplitude);
    waveform2.amplitude(hitAmplitude * 0.4); 

    envelope1.noteOn();
    lastTriggerTime = millis();
  }
  
  if (millis() - lastTriggerTime > 600) {
    waveform1.amplitude(0.0);
    waveform2.amplitude(0.0);
  }
}
