# MC8P Standalone MIDI Controller

---
 
# **8-Pot Standalone MIDI Controller (Teensy 4.0)**  
 
A customizable, open-source MIDI controller with **8 potentiometers**, each supporting **10 assignable MIDI CCs**, built around the **Teensy 4.0** for high-performance USB MIDI control.  
 
✅ **DIY Kit** | ✅ **Pre-Assembled** | ✅ **PCB Only**  
 
---
 
## **Features**  
- **8x B100K Pots** with smooth analog control  
- **Teensy 4.0** for fast, reliable USB MIDI  
- **SSD1306 OLED (128x64)** for real-time feedback  
- **Per-Pot CC Assignment** – Each pot can send **10 different MIDI CCs**  
- **3.5mm MIDI TRS Out** (PJ307 socket for DIN MIDI compatibility)  
- **4x Cherry MX Switches** for navigation/configuration  
- **Standalone Operation** – No computer needed after setup  
- **Open-Source Firmware** (Arduino-compatible)  
 
---
 
## **Bill of Materials (BOM)**  
| Component | Qty | Notes |  
|-----------|-----|-------|  
| Teensy 4.0 | 1 | Main microcontroller |  
| SSD1306 OLED (128x64) | 1 | I²C interface |  
| B100K Vertical Potentiometer | 8 | Linear taper recommended |  
| 3.5mm Stereo Socket (PJ307) | 1 | MIDI TRS Type A/B selectable |  
| Cherry MX Switches | 4 | For menu navigation |  
| 10nF Ceramic Capacitors | 8 | Potentiometer debouncing |  
| 10k Resistors | 4 | Pull-up/down for buttons |  
| 220Ω Resistors | 2 | OLED protection |  
 
---
 
## **Available Purchase Options**  
1. **Full DIY Kit** – All components + PCB, unassembled  
2. **Pre-Assembled Unit** – Fully built & tested  
3. **PCB Only** – For custom builds  
 
---
 
## **Installation & Setup**  
### **1. Flashing the Firmware**  
- Install **Arduino IDE + Teensyduino**  
- Clone this repo and open the `.ino` file  
- Select **Teensy 4.0** in Arduino IDE  
- Upload the firmware  
 
### **2. Hardware Assembly**  
- Solder components per the included schematic  
- Connect pots, OLED, and MIDI jack  
 
### **3. First Boot**  
- Plug in via USB (MIDI device should enumerate)  
- Use the **OLED menu** to assign CCs  
 
---
 
## **Configuration**  
Each potentiometer can be assigned **10 different MIDI CCs** via the interface:  
1. **Enter Setup Mode** (Hold Menu Button)  
2. **Select Pot** → Navigate with buttons  
3. **Assign CCs** – Choose from 10 slots per pot  
4. **Set MIDI Channel** (1-16)  
5. **Save & Exit**  
 
---
 
## **MIDI Implementation**  
- **USB MIDI** (Class-compliant, no drivers needed)  
- **TRS MIDI Out** (3.5mm Type A/B switchable)  
- **14-bit CC Support** (High-resolution mode)  
- **Customizable Min/Max Ranges** per CC  
 
---
 
## **Building from Source**  
- Requires **Arduino IDE + Teensyduino**  
- Libraries:  
  - `Adafruit_SSD1306` (OLED)  
  - `Bounce2` (Button debouncing)  
  - `MIDIUSB` (Teensy MIDI)  
 
---
 
## **License**  
**MIT License** – Open-source for personal & commercial use  
 
---
 
## **Support & Contributions**  
- **Issues:** Open a GitHub ticket  
- **Custom Requests:** DM on [Twitter/email/etc.]  
- **Want to improve it?** PRs welcome!  
 
---
