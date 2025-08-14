# MC8P Standalone MIDI Controller

---
 
# **8-Pot Standalone MIDI Controller (Teensy 4.0)**  
 
A customizable, open-source MIDI controller with **8 potentiometers**, each supporting **10 assignable MIDI CCs**, built around the **Teensy 4.0** for high-performance USB MIDI control.  
 
✅ **DIY Kit** | ✅ **Pre-Assembled** | ✅ **PCB Only**  
 
---
 
## **Features**  
- **8x B100K Pots** with smooth analog control  
- **Teensy 4.0** for fast, reliable MIDI   
- **SSD1306 OLED (128x64)** for UI  
- **Per-Pot CC Assignment** – Each pot can send **10 different MIDI CCs**  
- **3.5mm MIDI TRS Out** (PJ307 socket for Type A MIDI)  
- **4x Cherry MX Switches** for UI navigation/configuration  
- **Standalone Operation** – No computer needed after setup  
- **Open-Source Firmware** (Arduino IDE-compatible)  
 
---
 
## **Bill of Materials (BOM)**  
| Component | Qty | Notes |  
|-----------|-----|-------|  
| Teensy 4.0 | 1 | Main microcontroller |  
| SSD1306 OLED (128x64) | 1 | I²C interface |  
| B100K Vertical Potentiometer | 8 | Linear taper  |  
| 3.5mm Stereo Socket (PJ307) | 1 | MIDI TRS Type A |  
| Cherry MX Switches | 4 | For menu navigation |  
| 10nF Ceramic Capacitors | 8 | Potentiometer filtering |  
| 10k Resistors | 4 | Pull-up/down for buttons |  
| 220Ω Resistors | 2 | OLED protection |  
 
---
 
## **Available Purchase Options**  
1. **Pre-Assembled Unit** – Fully built & tested  
2. **Full DIY Kit** – All components + PCB, unassembled  
3. **PCB Only** – For custom builds  
 
---
 
## **Installation & Setup**  
### **1. Flashing the Firmware**  
- Install **Arduino IDE + Teensyduino** dependents  
- Clone this repo and open the `.ino` file in the Arduino IDE  
- Select **Teensy 4.0** in Arduino IDE  
- Upload the firmware  
 
### **2. Hardware Assembly**  
- Refer to the *Build Guide*  
 
### **3. First Boot**  
- Power  via USB 
- Use the **OLED menu** to assign CCs  
 
---
 
## **Configuration**  
Each potentiometer can be assigned **10 different MIDI CCs** via the interface:  
1. **Enter ASSIGN Mode** (Hold Menu Button)  
2. **Select Pot** → Navigate with buttons  
3. **Assign CCs** – Choose from 10 slots per pot  
4. **Set MIDI Channel** (1-16)  
5. **Save & Exit**  
 
---
 
## **MIDI Implementation**   
- **TRS MIDI Out** (3.5mm Type A)  
-  Up to **10 CC Messages** per potentiometer - further expansion coming soon! 
 
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
