// --- DIAGNOSTIC MODE FOR ALL INPUTS ---
// Run this standalone sketch to verify every pin connection before using the synth code.

const int KNOB_PITCH  = A10;
const int KNOB_CUTOFF = A11;
const int KNOB_VOL    = A12;

const int BTN_1 = 30;
const int BTN_2 = 31;
const int BTN_3 = 32; 

const int SW_1  = 28;
const int SW_2  = 29;

const int PIEZO_PIN = A14;

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000);
  
  Serial.println("=================================");
  Serial.println("   HARDWARE DIAGNOSTIC CHECK");
  Serial.println("=================================");

  // Configure digital inputs with pullups
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(SW_1, INPUT_PULLUP);
  pinMode(SW_2, INPUT_PULLUP);
}

void loop() {
  // 1. Read Analog Knobs
  int pVal = analogRead(KNOB_PITCH);
  int cVal = analogRead(KNOB_CUTOFF);
  int vVal = analogRead(KNOB_VOL);

  // 2. Read Digital Buttons & Switches (LOW = Pressed/Closed, HIGH = Released/Open)
  int b1 = digitalRead(BTN_1);
  int b2 = digitalRead(BTN_2);
  int b3 = digitalRead(BTN_3);
  int s1 = digitalRead(SW_1);
  int s2 = digitalRead(SW_2);

  // 3. Read Piezo Disc
  int piezo = analogRead(PIEZO_PIN);

  // Print formatted status line
  Serial.print("KNOBS [A10/A11/A12]: ");
  Serial.print(pVal); Serial.print("\t");
  Serial.print(cVal); Serial.print("\t");
  Serial.print(vVal); Serial.print(" | ");

  Serial.print("BTNS [30/31/32]: ");
  Serial.print(b1 == LOW ? "ON " : "OFF"); Serial.print(" ");
  Serial.print(b2 == LOW ? "ON " : "OFF"); Serial.print(" ");
  Serial.print(b3 == LOW ? "ON " : "OFF"); Serial.print(" | ");

  Serial.print("SW [28/29]: ");
  Serial.print(s1 == LOW ? "ON " : "OFF"); Serial.print(" ");
  Serial.print(s2 == LOW ? "ON " : "OFF"); Serial.print(" | ");

  Serial.print("PIEZO [A14]: ");
  Serial.println(piezo);

  delay(200); // Refresh rate for readability
}
