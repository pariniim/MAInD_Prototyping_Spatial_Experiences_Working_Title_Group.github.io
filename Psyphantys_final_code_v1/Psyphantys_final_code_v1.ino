#include <Wire.h>
#include "driver/i2s.h"

#define MPR121_ADDR 0x5B
static const int I2S_SAMPLE_RATE = 44100;
static const i2s_port_t I2S_PORT = I2S_NUM_0;

const float twoPi = 6.28318530718f;

// -------- Waveforms --------
enum Waveform {
  WAVE_SINE,
  WAVE_SQUARE,
  WAVE_TRIANGLE
};

struct ModeDef {
  float freq;
  Waveform wave;
};

// Frequency pool
const ModeDef modePool[5] = {
  {40.0f,  WAVE_SQUARE},
  {100.0f, WAVE_TRIANGLE},
  {133.0f, WAVE_SQUARE},
  {188.0f, WAVE_SINE},
  {388.0f, WAVE_SINE}
};

// Current per-pad assignment (for 0–3, 8–11)
float padFreq[12];
Waveform padWave[12];

// -------- Effects --------
enum EffectType {
  EFFECT_NONE,
  EFFECT_INHALE,  // CH8
  EFFECT_BURST,   // CH9
  EFFECT_RIPPLE,  // CH10
  EFFECT_EXHALE   // CH11
};

struct EffectState {
  bool touched;
  unsigned long lastTouchMillis;
};

EffectState effectStates[4]; // 0:CH8,1:CH9,2:CH10,3:CH11
EffectType currentEffect = EFFECT_NONE;

// Burst timing
bool burstActive = false;
unsigned long burstStartMillis = 0;
const unsigned long burstDuration = 120; // ms

// Ripple LFO
float ripplePhase = 0.0f;

// Smoothing
float smoothedFreq = 0.0f;
float smoothedAmp  = 0.0f;

// Creature state
bool creatureOn = false;
unsigned long lastInteractionMillis = 0;
const unsigned long AUTO_SLEEP_MS = 10UL * 60UL * 1000UL; // 10 minutes

// Awakening
int prevBodyTouches = 0;
unsigned long lastWakeToggleMillis = 0;
const unsigned long WAKE_DEBOUNCE_MS = 800;

// Activation chime
bool chimeActive = false;
unsigned long chimeStartMillis = 0;
const unsigned long CHIME_DURATION_MS = 300;

// Touch tracking
uint16_t prevStatus = 0;

// I2S
void i2s_init() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 27,
    .data_out_num = 25,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_set_clk(I2S_PORT, I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
}

// MPR121
uint16_t readTouchStatus() {
  Wire.beginTransmission(MPR121_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom(MPR121_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t status = Wire.read() | (Wire.read() << 8);
    return status & 0x0FFF;
  }
  return 0;
}

void mpr121_init() {
  Wire.beginTransmission(MPR121_ADDR);
  Wire.write(0x5E);
  Wire.write(0x00);
  Wire.endTransmission();

  for (uint8_t i = 0; i < 12; i++) {
    Wire.beginTransmission(MPR121_ADDR);
    Wire.write(0x41 + i * 2);
    Wire.write(12);
    Wire.write(6);
    Wire.endTransmission();
  }

  Wire.beginTransmission(MPR121_ADDR);
  Wire.write(0x5E);
  Wire.write(0x8F);
  Wire.endTransmission();
}

// Random mode assignment for a pad
void assignRandomModeToPad(int pad) {
  int idx = random(0, 5);
  padFreq[pad] = modePool[idx].freq;
  padWave[pad] = modePool[idx].wave;
}

// Update effect states from status
void updateEffects(uint16_t status) {
  unsigned long now = millis();

  bool inhaleTouched = status & (1 << 8);
  bool burstTouched  = status & (1 << 9);
  bool rippleTouched = status & (1 << 10);
  bool exhaleTouched = status & (1 << 11);

  bool prevInhale = effectStates[0].touched;
  bool prevBurst  = effectStates[1].touched;
  bool prevRipple = effectStates[2].touched;
  bool prevExhale = effectStates[3].touched;

  if (inhaleTouched && !prevInhale) {
    effectStates[0].lastTouchMillis = now;
  }
  if (burstTouched && !prevBurst) {
    effectStates[1].lastTouchMillis = now;
    burstActive = true;
    burstStartMillis = now;
  }
  if (rippleTouched && !prevRipple) {
    effectStates[2].lastTouchMillis = now;
  }
  if (exhaleTouched && !prevExhale) {
    effectStates[3].lastTouchMillis = now;
  }

  effectStates[0].touched = inhaleTouched;
  effectStates[1].touched = burstTouched;
  effectStates[2].touched = rippleTouched;
  effectStates[3].touched = exhaleTouched;

  if (burstActive && (now - burstStartMillis > burstDuration)) {
    burstActive = false;
  }

  if (burstActive) {
    currentEffect = EFFECT_BURST;
    return;
  }

  EffectType best = EFFECT_NONE;
  unsigned long bestTime = 0;

  if (effectStates[0].touched && effectStates[0].lastTouchMillis >= bestTime) {
    best = EFFECT_INHALE;
    bestTime = effectStates[0].lastTouchMillis;
  }
  if (effectStates[2].touched && effectStates[2].lastTouchMillis >= bestTime) {
    best = EFFECT_RIPPLE;
    bestTime = effectStates[2].lastTouchMillis;
  }
  if (effectStates[3].touched && effectStates[3].lastTouchMillis >= bestTime) {
    best = EFFECT_EXHALE;
    bestTime = effectStates[3].lastTouchMillis;
  }

  currentEffect = best;
}

// Awakening chime: simple upward sine sweep
float generateChimeSample() {
  unsigned long now = millis();
  unsigned long elapsed = now - chimeStartMillis;
  if (elapsed >= CHIME_DURATION_MS) {
    chimeActive = false;
    return 0.0f;
  }
  float t = (float)elapsed / (float)CHIME_DURATION_MS; // 0..1
  float f = 200.0f + 400.0f * t; // 200 -> 600 Hz
  static float chimePhase = 0.0f;
  float inc = twoPi * f / I2S_SAMPLE_RATE;
  chimePhase += inc;
  if (chimePhase > twoPi) chimePhase -= twoPi;
  return sinf(chimePhase);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  randomSeed(analogRead(34));

  mpr121_init();
  i2s_init();

  for (int i = 0; i < 12; i++) {
    padFreq[i] = 0.0f;
    padWave[i] = WAVE_SINE;
  }
  for (int i = 0; i < 4; i++) {
    effectStates[i].touched = false;
    effectStates[i].lastTouchMillis = 0;
  }

  creatureOn = false;
  lastInteractionMillis = millis();
  Serial.println("Creature ready (sleeping).");
}

void loop() {
  uint16_t status = readTouchStatus();
  unsigned long now = millis();

  // Rising edges: assign random modes to 0–3 and 8–11
  for (int pad = 0; pad < 12; pad++) {
    bool isBody = (pad >= 0 && pad <= 3);
    bool isEffect = (pad >= 8 && pad <= 11);
    if (!isBody && !isEffect) continue;

    bool nowTouched = status & (1 << pad);
    bool prevTouched = prevStatus & (1 << pad);
    if (nowTouched && !prevTouched) {
      assignRandomModeToPad(pad);
      lastInteractionMillis = now;
    }
  }

  // Count body and effect touches
  int bodyTouches = 0;
  int effectTouches = 0;
  for (int i = 0; i < 4; i++) {
    if (status & (1 << i)) bodyTouches++;
  }
  for (int i = 8; i < 12; i++) {
    if (status & (1 << i)) effectTouches++;
  }

  bool anyTouch = (bodyTouches > 0) || (effectTouches > 0);
  if (anyTouch) {
    lastInteractionMillis = now;
  }

  // Awakening: any 3 body pads touched while creature is off
  if (!creatureOn) {
    if (bodyTouches == 3 && prevBodyTouches != 3 &&
        (now - lastWakeToggleMillis > WAKE_DEBOUNCE_MS)) {
      creatureOn = true;
      lastWakeToggleMillis = now;
      chimeActive = true;
      chimeStartMillis = now;
      Serial.println("Creature awakened.");
    }
  }

  prevBodyTouches = bodyTouches;

  // Auto-sleep after 10 minutes of no interaction
  if (creatureOn && (now - lastInteractionMillis > AUTO_SLEEP_MS)) {
    creatureOn = false;
    Serial.println("Creature auto-slept.");
  }

  // Update effects
  updateEffects(status);

  // Determine base frequency and waveform
  float baseFreq = 0.0f;
  Waveform baseWave = WAVE_SINE;
  float baseAmp = 0.0f;

  bool bodyActive = (bodyTouches > 0);
  bool effectsActive = (effectTouches > 0);

  if (creatureOn) {
    if (bodyActive) {
      // BODY MODE: blend body pads
      float sumFreq = 0.0f;
      int count = 0;
      int lastBodyIndex = -1;
      for (int i = 0; i < 4; i++) {
        if (status & (1 << i)) {
          sumFreq += padFreq[i];
          count++;
          lastBodyIndex = i;
        }
      }
      if (count > 0) {
        baseFreq = sumFreq / (float)count;
        baseWave = padWave[lastBodyIndex];
        baseAmp = 22000.0f;
      }
    } else if (effectsActive) {
      // SOLO / MIXED SOLO EFFECT MODE: use average of active effect freqs
      float sumFreq = 0.0f;
      int count = 0;
      int lastEffectIndex = -1;
      for (int i = 8; i < 12; i++) {
        if (status & (1 << i)) {
          sumFreq += padFreq[i];
          count++;
          lastEffectIndex = i;
        }
      }
      if (count > 0) {
        baseFreq = sumFreq / (float)count;
        baseWave = padWave[lastEffectIndex];
        baseAmp = 22000.0f;
      }
    }
  }

  // Apply effects (only if creatureOn and some baseFreq)
  float targetFreq = baseFreq;
  float targetAmp = baseAmp;
  Waveform finalWave = baseWave;

  ripplePhase += 2.0f * 3.14159f * 0.8f / I2S_SAMPLE_RATE;
  if (ripplePhase > twoPi) ripplePhase -= twoPi;

  if (creatureOn && baseFreq > 0.0f) {
    switch (currentEffect) {
      case EFFECT_INHALE:
        targetAmp = baseAmp * 0.5f;
        targetFreq = (baseFreq > 4.0f) ? baseFreq - 3.0f : baseFreq;
        break;
      case EFFECT_EXHALE:
        targetAmp = baseAmp * 1.4f;
        targetFreq = baseFreq + 3.0f;
        break;
      case EFFECT_RIPPLE:
        targetAmp = baseAmp;
        targetFreq = baseFreq + 2.0f * sinf(ripplePhase);
        break;
      case EFFECT_BURST:
        targetAmp = baseAmp * 1.2f;
        finalWave = WAVE_SQUARE;
        break;
      case EFFECT_NONE:
      default:
        break;
    }
  } else {
    targetAmp = 0.0f;
    targetFreq = 0.0f;
  }

  // If creature is off, fade to silence
  if (!creatureOn && !chimeActive) {
    targetAmp = 0.0f;
    targetFreq = 0.0f;
  }

  // Smoothing
  const float alpha = 0.1f;
  smoothedFreq = smoothedFreq + alpha * (targetFreq - smoothedFreq);
  smoothedAmp  = smoothedAmp  + alpha * (targetAmp  - smoothedAmp);

  // Audio generation
  const int samples = 256;
  int16_t buffer[samples * 2];
  static float phase = 0.0f;

  float phaseInc = (smoothedFreq > 0.0f) ? (twoPi * smoothedFreq / I2S_SAMPLE_RATE) : 0.0f;

  for (int i = 0; i < samples; i++) {
    float s = 0.0f;

    if (chimeActive) {
      s = generateChimeSample();
    } else if (smoothedAmp > 1.0f && smoothedFreq > 0.0f) {
      switch (finalWave) {
        case WAVE_SINE:
          s = sinf(phase);
          break;
        case WAVE_SQUARE:
          s = (sinf(phase) >= 0.0f) ? 1.0f : -1.0f;
          break;
        case WAVE_TRIANGLE: {
          float p = fmodf(phase, twoPi);
          if (p < M_PI) {
            s = (p / M_PI) * 2.0f - 1.0f;
          } else {
            s = 1.0f - ((p - M_PI) / M_PI) * 2.0f;
          }
          break;
        }
      }
    } else {
      s = 0.0f;
    }

    int16_t v = (int16_t)(s * smoothedAmp);
    buffer[2 * i]     = v;
    buffer[2 * i + 1] = v;

    if (!chimeActive) {
      phase += phaseInc;
      if (phase >= twoPi) phase -= twoPi;
    }
  }

  size_t written;
  i2s_write(I2S_PORT, buffer, sizeof(buffer), &written, portMAX_DELAY);

  prevStatus = status;
}