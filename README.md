# MC8P Standalone MIDI Controller

---
 
# **8-Pot Standalone MIDI Controller (Teensy 4.0)**  
 
A customizable, open-source MIDI controller with **8 potentiometers**, each supporting **Up to 10 assignable MIDI CCs per Potentiometer**, built around the **Teensy 4.0** for high-performance MIDI control.  


!MC8P Render

 
✅ **Pre-Assembled** | ✅**DIY Kit** (excl. Teensy 4.0) | ✅ **PCB Only**  
 
---
 
## **Features**  
- **8x B100K Pots** with smooth analog control  
- **Teensy 4.0** for fast, reliable MIDI   
- **SSD1306 I2C OLED (128x64)** for the UI  
- **Per-Pot CC Assignment** – Each pot can send **10 different MIDI CCs**  
- **3.5mm MIDI TRS Out** (PJ307 socket for Type A MIDI)  
- **4x Cherry MX Switches** for UI navigation/configuration  
- **Standalone Operation** – No computer needed after setup  
- **Open-Source Firmware** (Arduino IDE-compatible)  
 
---
 
## **Bill of Materials (BOM)**  

| Component                  | Qty | Notes                          | Links |
|----------------------------|-----|--------------------------------|-------|
| Teensy 4.0                 | 1   | Main microcontroller           | [The Pi Hut (UK)](https://thepihut.com/products/pjrc-teensy-4-0-usb-development-board) |
| SSD1306 OLED (128×64)      | 1   | I²C interface                  | [AliExpress](https://www.aliexpress.com/item/1005005941908229.html) |
| B100K Vertical Potentiometer | 8  | Linear taper                   | [Example](https://shorturl.at/ZGaz2) |
| 3.5mm Stereo Socket (PJ307) | 1  | MIDI TRS Type A                | [Example](https://shorturl.at/lp2T3) |
| Cherry MX Switches         | 4   | For menu navigation            | —     |
| 10nF Ceramic Capacitors    | 8   | Potentiometer filtering        | —     |
| 10k Resistors              | 4   | Pull-up/down for buttons       | —     |
| 220Ω Resistors             | 2   | OLED protection                | —     | 
---
 
## **Available Purchase Options**  
1. **Pre-Assembled Unit** – Fully built & tested → 
2. **Full DIY Kit** – All components + PCB, unassembled - No Teensy 4.0 Provided → 
3. **PCB Only** – For custom builds → 
 
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
1. **Enter ASSIGN Mode** → Hold ASSIGN Button in performance mode.  
2. **Select Pot** → Hold ASSIGN + press NEXT/PREV to jump between potentiometers. 
4. **Toggle Parameter** → Press ASSIGN to toggle between editing MIDI Channel or CC Number
5. **Edit value of MIDI Channel/CC Number** → Press NEXT/PREV to increment/decrement the selected value.
6. **ADD new CC to Potentiometer** → Hold ENTER + NEXT (2 sec) to create a blank CC (default: Ch 1, CC1). Limit: 10 CCs per pot (to avoid visual clutter).
7. **Switch between CC Messages** → Hold ENTER + press NEXT/PREV to cycle through CCs on the current pot.
8. **DELETE CC from Potentiometer** → Hold ENTER + PREV (2 sec) to remove the selected CC.
9. **Save & Exit** → Hold ASSIGN + ENTER (2 sec) to save to EEPROM and return to PERFORMANCE Mode.

**Parameter Reset** → Hold NEXT + PREV (5 sec) to restore defaults (*Ch 1–8, CC7 [OP-XY Track Volume]*). This does not save the parameter reset to the EEPROM. 

**Owner's Manual:** https://docs.google.com/document/d/1LK1hT5nvRXzgEnc-npS-HN6GpmSG35B1l2-KNDyIfV0/edit?usp=sharing 
 
---
 
## **MIDI Implementation**   
- **TRS MIDI Out** (3.5mm Type A)  
-  Up to **10 CC Messages** per potentiometer - further expansion maybe soon! 
 
---
 
## **Building from Source**  
- Requires **Arduino IDE + Teensyduino**  
- Libraries:  
  - `Adafruit_SSD1306` (OLED)  
  - `Adafruit_GFX` (Adafruit UI Design)  
  - `MIDI` (MIDI)
  - `ResponsiveAnalogRead` (Potentiometer Smoothing)
  - `Picopixel` (Picopixel Font)
 
---
 
## **License**  
**MIT License** – Open-source for personal & commercial use  
 
---
 
## **Support & Contributions**  
- **Issues:** Open a GitHub ticket  
- **Custom Requests:** DM on [Twitter/email/etc.]  
- **Want to improve it?** PRs welcome!  
 
---
