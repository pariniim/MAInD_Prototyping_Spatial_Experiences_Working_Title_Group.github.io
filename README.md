# Prototyping Spacial Experiences 2026
## SUPSI - Master of Arts in Interaction Design

# About

**Course name**: **Prototyping Spacial Experiences 2026**

**Teacher**: Leonardo Angelucci 

**Assistants**: Luca Draisci, Alice Mioni

# Psyphantys  
*The Soul Revealer*

Psyphantys is an interactive sound‑driven installation that awakens through human touch and expresses itself through resonant sand patterns. Visitors activate and modulate the creature by touching capacitive pads embedded in its wooden body. An ESP32 synthesizes sound in real time, driving a speaker‑plate assembly that animates sand into dynamic modal geometries.

---

## ✨ Features

- **Capacitive touch interaction** via MPR121  
- **Real‑time audio synthesis** on ESP32 (I2S, 44.1 kHz)  
- **Dynamic Chladni patterns** on a metal plate  
- **Dual‑nature effect pads** (solo voices vs. body modifiers)  
- **Random frequency assignment** on every touch  
- **Three‑pad awakening ritual**  
- **10‑minute auto‑sleep**  
- **Activation chime** when waking  
- **Weighted blending** of multiple body pads  
- **Last‑touched effect priority**  

---

## 🧩 System Architecture

Psyphantys is composed of three main layers:

### 1. Touch Input Layer
- MPR121 reads 12 capacitive pads  
- Pads **CH0–CH3** → body frequencies  
- Pads **CH8–CH11** → effect organs  
- Every touch assigns a new random frequency from a curated pool  

### 2. Audio Synthesis Layer
- ESP32 generates audio using I2S  
- Waveforms: sine, square, triangle  
- Effects: inhale, exhale, ripple, burst  
- Burst overrides waveform temporarily  
- Ripple uses a slow FM LFO  
- Amplitude and frequency smoothing  

### 3. Mechanical Output Layer
- I2S amplifier drives a Visaton W100S speaker  
- Speaker → bolt → metal plate → sand  
- Plate must be free except at the center bolt  
- Sand forms modal patterns based on frequency  

---

## 🔌 Hardware Requirements

- **ESP32 DevKit** (I2S capable)  
- **MPR121** capacitive touch controller  
- **MAX98357A** I2S amplifier (or equivalent)  
- **Visaton W100S** or similar full‑range driver  
- **3–5 mm metal plate** (steel or aluminum)  
- **Central bolt + washers** for rigid coupling  
- **Fine sand or black quartz powder**  
- **5 V / 2 A USB power bank** (recommended)  
- Custom wooden enclosure  

---

## 🔧 Wiring

### ESP32 → MPR121
| MPR121 Pin | ESP32 Pin |
|------------|-----------|
| SDA        | GPIO 21   |
| SCL        | GPIO 22   |
| VCC        | 3.3 V     |
| GND        | GND       |

### ESP32 → I2S Amplifier (MAX98357A)
| Amplifier Pin | ESP32 Pin |
|---------------|-----------|
| DIN           | GPIO 25   |
| BCLK          | GPIO 26   |
| LRC / WS      | GPIO 27   |
| VIN           | 5 V from power bank |
| GND           | Common ground |

### Amplifier → Speaker
| Amplifier Pin | Speaker |
|---------------|---------|
| SPK+          | +       |
| SPK–          | –       |

---

## 🧠 Interaction Model

### Awakening
- Touch any **three** of CH0–CH3  
- Psyphantys wakes  
- Plays activation chime  

### Body Mode (CH0–CH3)
- Each touch assigns a new random frequency  
- Frequencies blend  
- Effects modify the blended tone  

### Solo Effect Mode (CH8–CH11)
- Each pad gets its own random frequency  
- Last‑touched effect applies to its own tone  
- Others play plain tones  

### Auto‑Sleep
- No touch for **10 minutes** → fade out → sleep  

---

## 🛠️ Firmware

The firmware is organized into:

- `touch.cpp` — MPR121 handling  
- `audio.cpp` — waveform generation, effects, I2S  
- `interaction.cpp` — awakening, auto‑sleep, mode logic  
- `main.cpp` — system loop and orchestration  

Upload using Arduino IDE or PlatformIO.

---

## 🧪 Troubleshooting

### Sand not moving
- Ensure plate edges are free (no fabric touching)  
- Tighten center bolt  
- Reduce sand amount  
- Increase amplitude in code  
- Try mid‑range frequencies (100–300 Hz)  

### Touch sensor unstable
- Ensure common ground  
- Use a stable 5 V power bank  
- Keep amplifier wiring away from MPR121  

### Audio weak or distorted
- Check I2S wiring (DIN/BCLK/LRC)  
- Ensure amplifier has enough current  
- Verify bolt coupling is rigid  

---

## 🐚 About Psyphantys
Psyphantys is an expressive, semi‑autonomous sound creature that reveals the hidden geometries of vibration. It is both instrument and organism, responding to touch with shifting frequencies, resonant patterns, and ritualistic behavior.

---

# Resources

[One Drive Link](https://drive.google.com/drive/folders/14P_7_v80qdhS_09BsH-U0207a6U55ahQ?usp=sharing)
