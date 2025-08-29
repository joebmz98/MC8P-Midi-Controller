# MC8P – 8-Pot Standalone MIDI Controller (Teensy 4.0)

A customizable, open-source MIDI controller featuring **8 potentiometers**, each capable of sending **up to 10 assignable MIDI CC messages**, built around the high-performance **Teensy 4.0**.

!MC8P_TOPANGLED_RENDER_V1_0

✅ **Pre-Assembled** | ✅ **DIY Kit** (excl. Teensy 4.0) | ✅ **PCB Only**

---

## Features

- **8x B100K Linear Pots** for smooth analog control  
- **Teensy 4.0** for fast, reliable MIDI performance  
- **SSD1306 I²C OLED (128x64)** for intuitive UI  
- **Per-Pot CC Assignment** – Up to **10 MIDI CCs per pot**  
- **3.5mm MIDI TRS Out** (PJ307 socket, Type A)  
- **4x Cherry MX Switches** for menu navigation  
- **Standalone Operation** – No computer required after setup  
- **Open-Source Firmware** – Arduino IDE compatible  

---

## Bill of Materials (BOM)

| Component                    | Qty | Notes                          | Link |
|-----------------------------|-----|--------------------------------|------|
| Teensy 4.0                  | 1   | Main microcontroller           | The Pi Hut (UK) |
| SSD1306 OLED (128×64)       | 1   | I²C interface                  | AliExpress |
| B100K Vertical Potentiometer| 8   | Linear taper                   | Example |
| 3.5mm Stereo Socket (PJ307) | 1   | MIDI TRS Type A                | Example |
| Cherry MX Switches          | 4   | For menu navigation            | —    |
| 10nF Ceramic Capacitors     | 8   | Potentiometer filtering        | —    |
| 10k Resistors               | 4   | Pull-up/down for buttons       | —    |
| 220Ω Resistors              | 2   | For MIDI circuitry             | —    |

---

## Purchase Options

1. **Pre-Assembled Unit** – Fully built & tested → *Not currently available*  
2. **Full DIY Kit** – All components + PCB, unassembled (Teensy 4.0 not included) → *Not currently available*  
3. **PCB Only** – For custom builds → *Coming soon*

---

## Installation & Setup

### 1. Flashing the Firmware
- Install **Arduino IDE** and **Teensyduino**
- Clone this repo and open the `.ino` file in Arduino IDE
- Select **Teensy 4.0** as the board
- Upload the firmware

### 2. Hardware Assembly
- Refer to the *Build Guide* for soldering and assembly instructions

### 3. First Boot
- Power via USB
- Use the **OLED menu** to assign MIDI CCs

---

## Configuration Guide

Each potentiometer supports up to **10 assignable MIDI CCs**. Here's how to configure them:

1. **Enter ASSIGN Mode** – Hold the ASSIGN button in performance mode  
2. **Select Pot** – Hold ASSIGN + press NEXT/PREV to cycle through pots  
3. **Toggle Parameter** – Press ASSIGN to switch between MIDI Channel and CC Number  
4. **Edit Value** – Use NEXT/PREV to adjust the selected parameter  
5. **Add New CC** – Hold ENTER + NEXT (2 sec) to add a new CC (default: Ch 1, CC1)  
6. **Cycle Through CCs** – Hold ENTER + press NEXT/PREV to browse assigned CCs  
7. **Delete CC** – Hold ENTER + PREV (2 sec) to remove the selected CC  
8. **Save & Exit** – Hold ASSIGN + ENTER (2 sec) to save to EEPROM and return to performance mode  

**Reset Parameters** – Hold NEXT + PREV (5 sec) to restore defaults (Ch 1–8, CC7). *Note: This does not save to EEPROM.*

📘 **Owner's Manual:** View on Google Docs

---

## MIDI Implementation

- **TRS MIDI Out** (3.5mm Type A)  
- Up to **10 CC messages per potentiometer**  
- Future expansion planned!

---

## Building from Source

- Requires **Arduino IDE + Teensyduino**  
- Libraries used:
  - `Adafruit_SSD1306` – OLED display  
  - `Adafruit_GFX` – UI rendering  
  - `MIDI` – MIDI communication  
  - `ResponsiveAnalogRead` – Pot smoothing  
  - `Picopixel` – Font rendering

---

## License

**MIT License** – Free for personal and commercial use

---

## Support & Contributions

- **Issues:** Open a GitHub issue  
- **Custom Requests:** DM via [Twitter/email/etc.]  
- **Want to contribute?** Pull requests welcome!
