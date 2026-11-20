#include <Arduino.h>

/*
  Ocarina comando - Arduino Uno + Grove Base Shield
  Toque capacitivo SEM resistencias (usa a pull-up interna).
  Envia linhas "101111100011,0.62" a 9600 baud:
    12 bits dos buracos (q w e r 2 y u i o 7 c n) + sopro de 0 a 1
*/

#define DEBUG_RAW 0   // 1 = mostra valores brutos para calibrar (o site NAO funciona assim)

const int NUM_HOLES = 12;

//                              q  w  e  r  2  y  u  i  o   7   c   n
const int HOLE_PINS[NUM_HOLES] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const int SOUND_PIN = A3;

// ---------- CALIBRACAO ----------
const byte  TRIALS       = 40;    // repeticoes por leitura (mais = mais estavel)
const byte  MAX_COUNT    = 100;   // limite de contagem por repeticao
const long  TOUCH_MARGIN = 40;    // quanto acima do repouso conta como "tocado"
const float BLOW_RANGE   = 200.0; // amplitude do microfone = sopro maximo (1.0)
// --------------------------------

volatile uint8_t* ddrReg[NUM_HOLES];
volatile uint8_t* outReg[NUM_HOLES];
volatile uint8_t* inReg[NUM_HOLES];
uint8_t pinMask[NUM_HOLES];

long baseline[NUM_HOLES];
bool touched[NUM_HOLES];
int  noiseFloor = 0;

// Mede quanto tempo o fio demora a "carregar" via pull-up interna.
// Com um dedo a tocar, demora mais, logo o valor sobe.
long readTouch(int i) {
  long total = 0;
  for (byte t = 0; t < TRIALS; t++) {
    // descarrega o fio (saida a LOW)
    *outReg[i] &= ~pinMask[i];
    *ddrReg[i] |= pinMask[i];
    delayMicroseconds(10);

    byte count = 0;
    noInterrupts();
    // passa a entrada com pull-up e conta ate ficar HIGH
    *ddrReg[i] &= ~pinMask[i];
    *outReg[i] |= pinMask[i];
    while (!(*inReg[i] & pinMask[i]) && count < MAX_COUNT) count++;
    interrupts();

    total += count;
  }
  // deixa o fio descarregado
  *outReg[i] &= ~pinMask[i];
  *ddrReg[i] |= pinMask[i];
  return total;
}

int readSoundAmplitude() {
  unsigned long start = millis();
  int mn = 1023, mx = 0;
  while (millis() - start < 10) {
    int v = analogRead(SOUND_PIN);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  return mx - mn;
}

void calibrate() {
  // NAO toques nos fios nem sopres durante este tempo
  delay(1000);
  const int ROUNDS = 20;

  for (int i = 0; i < NUM_HOLES; i++) baseline[i] = 0;

  for (int r = 0; r < ROUNDS; r++) {
    for (int i = 0; i < NUM_HOLES; i++) baseline[i] += readTouch(i);
    int a = readSoundAmplitude();
    if (a > noiseFloor) noiseFloor = a;
  }
  for (int i = 0; i < NUM_HOLES; i++) baseline[i] /= ROUNDS;
  noiseFloor += 2;
}

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < NUM_HOLES; i++) {
    uint8_t port = digitalPinToPort(HOLE_PINS[i]);
    pinMask[i] = digitalPinToBitMask(HOLE_PINS[i]);
    ddrReg[i]  = portModeRegister(port);
    outReg[i]  = portOutputRegister(port);
    inReg[i]   = portInputRegister(port);
    touched[i] = false;
  }

  calibrate();
}

void loop() {
  long raw[NUM_HOLES];

  for (int i = 0; i < NUM_HOLES; i++) {
    long v = readTouch(i);
    raw[i] = v;

    if (!touched[i] && v > baseline[i] + TOUCH_MARGIN) {
      touched[i] = true;
    } else if (touched[i] && v < baseline[i] + (TOUCH_MARGIN * 6) / 10) {
      touched[i] = false;
    }
  }

  int amp = readSoundAmplitude() - noiseFloor;
  if (amp < 0) amp = 0;
  float breath = amp / BLOW_RANGE;
  if (breath > 1.0) breath = 1.0;

#if DEBUG_RAW
  Serial.print("raw: ");
  for (int i = 0; i < NUM_HOLES; i++) { Serial.print(raw[i]); Serial.print(' '); }
  Serial.print("| base: ");
  for (int i = 0; i < NUM_HOLES; i++) { Serial.print(baseline[i]); Serial.print(' '); }
  Serial.print("| sopro: ");
  Serial.println(amp);
  delay(100);
#else
  for (int i = 0; i < NUM_HOLES; i++) Serial.print(touched[i] ? '1' : '0');
  Serial.print(',');
  Serial.println(breath, 2);
#endif
}