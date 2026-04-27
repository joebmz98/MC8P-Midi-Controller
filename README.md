# MC8P Standalone MIDI Controller

---

# **8-Pot Standalone MIDI Controller (Teensy 4.0)**

A customizable, open-source MIDI controller with **8 potentiometers**, each supporting **Up to 10 assignable MIDI CCs per Potentiometer**, built around the **Teensy 4.0** for high-performance MIDI control.

<img src="https://github.com/joebmz98/MC8P-Midi-Controller/raw/main/MC8P_MIDI_CONTROLLERS_MEDIA/PHOTOS/P1155438.JPG" alt="Photo of the assembled MC8P MIDI Controller" width="600"/>

---

## **Features**
- **8x B100K Pots** with smooth analog control
- **Teensy 4.0** for fast, reliable MIDI
- **SSD1306 I2C OLED (128x64)** for the UI
- **Per-Pot CC Assignment** – Each pot can send **10 different MIDI CCs**
- **3.5mm MIDI TRS Out** (PJ307 socket for Type A MIDI)
- **4x Kailh Switches** for UI navigation/configuration
- **Standalone Operation** – No computer needed after setup
- **Open-Source Firmware** (Arduino IDE-compatible)

---

## **Bill of Materials (BOM)**

| Component                  | Qty | Notes                          | Links |
|----------------------------|-----|--------------------------------|-------|
| Teensy 4.0                 | 1   | Main microcontroller           | [The Pi Hut (UK)](https://thepihut.com/products/pjrc-teensy-4-0-usb-development-board) |
| SSD1306 OLED (128×64)      | 1   | I²C interface                  | [AliExpress](https://www.aliexpress.com/item/1005006997755041.html) |
| B100K Vertical Potentiometer | 8  | Linear taper                   | [Example](https://shorturl.at/ZGaz2) |
| 3.5mm Stereo Socket (PJ307) | 1  | MIDI TRS Type A                | [Example](https://shorturl.at/lp2T3) |
| Kailh Switches             | 4   | For menu navigation            | —     |
| Kailh Switch Caps          | 4   | For Kailh switches             | [AliExpress](https://www.aliexpress.com/item/1005006477890497.html) |
| Potentiometer Knobs        | 8   | For B100K potentiometers       | [AliExpress](https://www.aliexpress.com/item/1005006064249289.html) |
| DC-005 Barrel Jack Socket  | 1   | 5V DC power input              | [AliExpress](https://www.aliexpress.com/item/32839712664.html) |
| 10nF Ceramic Capacitors    | 8   | Potentiometer filtering        | —     |
| 10k Resistors              | 4   | Pull-down for buttons          | —     |
| 220Ω Resistors             | 2   | For MIDI Circuitry             | —     |
| 100nF Ceramic Capacitor    | 1   | Power rail stabilisation       | —     |
| 10uF Ceramic Capacitor     | 1   | Power rail stabilisation       | —     |
| 10uF Electrolytic Capacitor| 1   | 5V input stability             | —     |
| 100uF Electrolytic Capacitor | 1 | Power rail stabilisation       | —     |

---

## **Available Purchase Options**
**PCB Only** – For custom builds → [**Purchase from my Tindie store**](https://www.tindie.com/products/axsinstruments/diy-mc8p-standalone-midi-controller-pcbs-only/)

---

## **Installation & Setup**
### **1. Flashing the Firmware**
- Install **Arduino IDE + Teensyduino** and other dependencies referenced in the Building from Source section
- Clone this repo and open the `.ino` file in the Arduino IDE
- Select **Teensy 4.0** in Arduino IDE
- Upload the firmware

### **2. Hardware Assembly**
- Refer to the *Build Guide*

### **3. First Boot**
- Power via **USB** on the Teensy or via DC-005 input jack 
- Use the **OLED menu** to assign CCs

---

## **Configuration**

### Button Functions

| Button | Primary Function |
|--------|-----------------|
| ASSIGN | Navigate backward, cancel actions, return to previous screens |
| ENTER | Confirm selections, activate temporary override mode |
| PREV | Move up or decrease values |
| NEXT | Move down or increase values |

### Screen Hierarchy

The interface is organized into several screens accessible from the main display:

**MAIN SCREEN** → **MENU** → **SETTINGS** → various configuration screens

### Main Screen

This is your default operating view. The screen displays eight circular indicators, one for each potentiometer, showing current MIDI values (0-127). Each circle fills proportionally to the value being sent.

From the main screen:
- Press **ASSIGN** to open the menu
- Press and hold **ENTER** to activate temporary override mode (the display inverts as visual feedback)

### Temporary Override Mode

When you press and hold ENTER on the main screen, the controller enters temporary override mode. In this mode, all potentiometer adjustments are temporary. When you release ENTER, the controller returns to the stored values and activates catch up mode, where potentiometers must pass through their stored positions before taking control again.

### Menu Screen

Press ASSIGN from the main screen to reach the menu. Use PREV and NEXT to scroll through options:

- **CC assign** – Configure MIDI messages for each potentiometer
- **Settings** – Access device configuration

Press ENTER to select an option, or press ASSIGN to return to the main screen.

### Assign Screen (CC Assign)

This screen lets you configure up to 10 MIDI messages per potentiometer.

**Navigation flow:**
1. Select which potentiometer to edit (flashing number)
2. Select which message slot to edit, or choose Add/Remove
3. Edit individual parameters

**Parameters you can edit for each message:**
- **Channel** – MIDI channel (1-16) or OMNI
- **CC** – Control Change number (0-127)
- **Min Value** – Minimum output value (0-127)
- **Max Value** – Maximum output value (0-127)
- **Direction** – Normal (0-127) or Inverted (127-0)

**Editing values:** When editing a parameter, use potentiometer 8 to smoothly adjust the value across its full range. The selected parameter flashes to indicate edit mode.

**Navigation within Assign Screen:**
- **PREV/NEXT** – Navigate between pots, messages, or Add/Remove buttons
- **ENTER** – Enter edit mode for a parameter, or cycle to the next parameter
- **ASSIGN** – Return to pot selection, or save and exit (confirmation popup appears)

**Add/Remove messages:**
- Navigate to "+ Add" to create a new message (up to 10 per pot)
- Navigate to "- Remove" to delete the selected message (minimum 1 message per pot)

### Settings Screen

From the menu, select Settings to access:

**Assign Settings** – Choose which potentiometer (1-8) to use for editing values in the Assign screen. Use PREV/NEXT to change the selection.

**Display Settings** – Toggle the display inversion between normal and inverted. Press ENTER to enter edit mode, then use PREV or NEXT to toggle between Yes and No. Press ENTER again to save, or ASSIGN to cancel.

**About** – View firmware version and device information.

**Factory Reset** – Restore all settings to default values. A confirmation screen prevents accidental resets.

### Confirmation Popups

When saving changes or performing a factory reset, a popup screen appears asking for confirmation. Use PREV/NEXT to toggle between Y (Yes) and N (No), then press ENTER to confirm or ASSIGN to cancel.

---

## **MIDI Implementation**
- USB MIDI **OR** TRS MIDI Out (3.5mm Type A)
- Up to **10 CC Messages** per potentiometer - further expansion maybe soon!

---

## **Building from Source**
- Requires **Arduino IDE + Teensyduino**
- Libraries:
  - `Adafruit_SSD1306` (OLED)
  - `Adafruit_GFX` (Adafruit UI Design)
  - `MIDI` (MIDI)
  - `ResponsiveAnalogRead` (Potentiometer Smoothing)
  - `Picopixel` (Picopixel Font)

**Owner's Manual:** https://docs.google.com/document/d/1LK1hT5nvRXzgEnc-npS-HN6GpmSG35B1l2-KNDyIfV0/edit?usp=sharing

---

## **License**
**MIT License** – Open-source for personal & commercial use

---

## **Support & Contributions**
- **Issues:** Open a GitHub ticket
- **Custom Requests:** Start a thread in the discussions page or send me an E-mail! axs.instruments@gmail.com
- **Want to improve it?** PRs welcome!
- **Special Thanks:** This project is supported by **PCBWay** who have kindly provided the PCB stock available at the moment

---

## **Acknowledgments**

PCBway very kindly offered to sponsor this project and have sent me some more stock for the shop. Thank you to PCBway for supporting this passion project of mine. Emily from the marketing department reached out to me when I first launched the product and store and very kindly offered to sponsor the project. Both working with them and using their services was really easy. All I needed to do was export my PCB gerber files, drag and drop them on the 'PCB Instant Quote' page and send it off for approval. I received my order a week later and was very happy with the quality of the PCBs. Thank you again PCBway for your support.

You can find their website here: https://www.pcbway.com/
