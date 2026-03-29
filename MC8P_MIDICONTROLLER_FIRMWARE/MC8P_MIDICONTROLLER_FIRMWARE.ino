// ********************* //
// MC8P Standalone       //
// MIDI Controller       //
// firmware version 2.0  //
// Hardware Version: 1.0 //
// ********************* //
// controller by         //
// .axs instruments      //
// ********************* //
// Description: 8-potentiometer MIDI controller with
//              programmable CC assignments and
//              multi-message per pot support.
// ********************* //

// ********************* //
// LICENSE & COPYRIGHT
//
// This project is released under the MIT License.
// Copyright (c) 2024 .axs instruments
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ********************* //

// LIBRARIES
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <MIDI.h>
#include <ResponsiveAnalogRead.h>
#include <Fonts/Picopixel.h>

// USB NAME
#include <usb_names.h>

// FLASH
#include <EEPROM.h>
#define EEPROM_ADDR 0            // Starting address in EEPROM
#define EEPROM_SIGNATURE 0x55AA  // Signature to verify saved data

// OLED SETUP
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MIDI INSTANCE
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// STRUCTURE TO HOLD MULTIPLE MIDI MESSAGES PER POT - MOVED TO TOP
struct MidiMessageParams {
  byte channel;
  byte cc;
  bool inverted;  // true = inverted (127->0), false = normal (0->127)
  byte minValue;  // Minimum value for CC range (0-127)
  byte maxValue;  // Maximum value for CC range (0-127)
  int value;      // Current stored value
};

// Helper function to map potentiometer value with range and direction
int mapMidiValueWithParams(int rawValue, byte minVal, byte maxVal, bool inverted) {
  // Convert raw potentiometer value (0-1023) to the specified range
  int mappedValue;

  // Handle the range (allow min > max for inverse mapping)
  if (minVal <= maxVal) {
    // Normal range
    mappedValue = map(rawValue, 0, 1023, minVal, maxVal);
  } else {
    // Inverted range (min > max)
    mappedValue = map(rawValue, 0, 1023, maxVal, minVal);
  }

  // Constrain to the valid range
  mappedValue = constrain(mappedValue, min(minVal, maxVal), max(minVal, maxVal));

  // Apply direction inversion if needed
  if (inverted) {
    // Invert the value within the range
    int rangeMin = min(minVal, maxVal);
    int rangeMax = max(minVal, maxVal);
    mappedValue = rangeMin + (rangeMax - mappedValue);
  }

  return mappedValue;
}

// FIRMWARE VERSION
const char* FIRMWARE_VERSION = "2.0";

// BUTTON CONFIG
const int NUM_BUTTONS = 4;
const int BUTTON_PINS[NUM_BUTTONS] = { 5, 6, 7, 8 };
const char* BUTTON_NAMES[NUM_BUTTONS] = { "ASSIGN", "ENTER", "PREV", "NEXT" };
int buttonState[NUM_BUTTONS] = { 0 };
int lastButtonState[NUM_BUTTONS] = { 0 };
unsigned long lastDebounceTime[NUM_BUTTONS] = { 0 };
const unsigned long debounceDelay = 15;  // BUTTON DEBOUNCE

// SCREEN STATE MANAGEMENT
enum ScreenState { MAIN_SCREEN,
                   MENU_SCREEN,
                   ASSIGN_SCREEN,
                   STATES_SCREEN,
                   SETTINGS_SCREEN,
                   DISPLAY_SETTINGS_SCREEN,
                   ABOUT_SCREEN,
                   RESET_SCREEN,
                   SAVE_STATES_SCREEN,
                   LOAD_STATES_SCREEN,
                   CLEAR_STATES_SCREEN,
                   CONFIRM_SAVE_SCREEN,
                   CONFIRM_ASSIGN_SAVE_SCREEN,
                   CONFIRM_RESET_SCREEN };
ScreenState currentScreen = MAIN_SCREEN;

// ASSIGN SCREEN NAVIGATION STATES
enum AssignEditMode {
  ASSIGN_POT_SELECT,      // Selecting which pot to edit
  ASSIGN_MESSAGE_SELECT,  // Selecting which message to edit
  ASSIGN_EDIT_CHANNEL,    // Editing channel
  ASSIGN_EDIT_CC,         // Editing CC number
  ASSIGN_EDIT_INVERT,     // Editing invert setting
  ASSIGN_EDIT_MIN,        // Editing min value
  ASSIGN_EDIT_MAX         // Editing max value
};

AssignEditMode assignEditMode = ASSIGN_POT_SELECT;
bool assignEditingMode = false;  // True when editing values, false when selecting
unsigned long assignFlashTimer = 0;
bool assignFlashState = false;

// DISPLAY SETTINGS
bool displayInverted = false;          // Default: No
bool editingDisplaySetting = false;    // Flag to indicate we're editing the value
unsigned long editFlashTimer = 0;      // Timer for flashing effect
bool flashState = false;               // Current flash state
bool originalDisplayInverted = false;  // Store original value when entering edit mode

// MENU NAVIGATION
int selectedMenuItem = 0;  // 0: Assign, 1: States, 2: Settings

// BUTTON STATE MANAGEMENT
// ASSIGN
bool assignButtonHeld = false;
unsigned long assignButtonHoldStart = 0;
const unsigned long assignHoldDuration = 2000;  // 2 seconds
// PREV/NEXT
bool prevButtonPressed = false;
bool nextButtonPressed = false;
bool prevNextHeld = false;
unsigned long prevNextHoldStart = 0;
const unsigned long prevNextHoldDuration = 5000;  // 5 seconds
// ENTER
bool enterButtonHeld = false;
bool enterPrevHeld = false;
bool enterNextHeld = false;
unsigned long enterPrevHoldStart = 0;
unsigned long enterNextHoldStart = 0;
// MULTI-FUNCTIONAL
bool bothButtonsHeld = false;
unsigned long bothButtonsHoldStart = 0;

// POTENTIOMETER CONFIG
const int N_POTS = 8;  // Eight potentiometers
const int POT_PIN[N_POTS] = { A0, A1, A2, A3, A6, A7, A8, A9 };

// MIDI CONFIG
// START-UP CONFIG
byte potChannels[N_POTS] = { 0, 1, 2, 3, 4, 5, 6, 7 };  // Channels 1-8
byte potCCs[N_POTS] = { 7, 7, 7, 7, 7, 7, 7, 7 };       // CC numbers

const int MAX_MESSAGES_PER_POT = 10;                          // Maximum number of messages per pot
MidiMessageParams potMessages[N_POTS][MAX_MESSAGES_PER_POT];  // Array to store messages
byte messageCount[N_POTS] = { 0 };                            // Track how many messages each pot has

// ARRAYS TO STORE POT VALUES AND STATES
int potReading[N_POTS] = { 0 };
int potState[N_POTS] = { 0 };
int potPState[N_POTS] = { 0 };
int currentMidiValue[N_POTS] = { 0 };  // Store MIDI values for display

const byte potThreshold = 5;
const int POT_TIMEOUT = 300;
unsigned long pPotTime[N_POTS] = { 0 };
unsigned long potTimer[N_POTS] = { 0 };

float snapMultiplier = 0.01;
ResponsiveAnalogRead responsivePot[N_POTS] = {
  ResponsiveAnalogRead(POT_PIN[0], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[1], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[2], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[3], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[4], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[5], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[6], true, snapMultiplier),
  ResponsiveAnalogRead(POT_PIN[7], true, snapMultiplier)
};

// POT SELECTION AND EDITING
int selectedPot = 0;  // Default to pot 0
unsigned long lastPotSwitchTime = 0;
const unsigned long POT_SWITCH_DELAY = 100;  // ms delay between pot switches
bool editingChannel = true;                  // Tracks whether we're editing channel or CC
int valueIndicatorPos = 20;                  // X position of value indicator (starts under channel)

int selectedMessage = 0;  //

// TEMPORARY MIDI OVERRIDE SYSTEM
bool tempOverrideActive = false;                // Flag for temporary override mode
int originalMidiValues[N_POTS] = { 0 };         // Store original values when ENTER is pressed - INIT TO 0
int tempMidiValues[N_POTS] = { 0 };             // Store temporary values during override - INIT TO 0
unsigned long enterPressTime = 0;               // Time when ENTER was pressed
const unsigned long tempOverrideHoldTime = 20;  // Debounce time to activate override
int postOverridePotPositions[N_POTS] = { 0 };   // Store pot positions when temp override ends - INIT TO 0
bool catchUpActive[N_POTS] = { false };         // Flag to track if catch-up is active for each pot - INIT TO FALSE
int catchUpStartValue[N_POTS] = { 0 };          // MIDI value when catch-up starts - INIT TO 0
int catchUpStartPotPos[N_POTS] = { 0 };         // Pot position when catch-up starts - INIT TO 0

// Add this variable with your other global variables
bool confirmAssignSave = true;  // Track Y/N selection for ASSIGN save confirmation

// EEPROM STRUCTURES
// STRUCTURE TO SAVE TO EEPROM - UPDATED WITH NEW PARAMETERS
struct SavedSettings {
  uint16_t signature;
  uint8_t version;
  byte messageCount[N_POTS];
  struct SavedMidiMessage {
    byte channel;
    byte cc;
    bool inverted;
    byte minValue;
    byte maxValue;
    int value;
  } potMessages[N_POTS][MAX_MESSAGES_PER_POT];
};

// STRUCTURE FOR GLOBAL SETTINGS
struct GlobalSettings {
  uint16_t signature;
  uint8_t version;
  bool displayInverted;
  int lastLoadedState;
};

// Version constants
#define SETTINGS_VERSION 2
#define GLOBAL_SETTINGS_VERSION 1

// STATE SLOTS - For saving/loading complete configurations
#define NUM_STATE_SLOTS 8
SavedSettings stateSlots[NUM_STATE_SLOTS];
int pendingSaveSlot = 0;       // Store which slot we're about to save to
bool confirmSelection = true;  // Track Y/N selection (true = Yes, false = No)
int currentStateSlot = -1;     // Track which state is currently loaded (-1 = none/initial state)

// EEPROM address for global settings (separate from state slots)
#define GLOBAL_EEPROM_ADDR (EEPROM_ADDR + sizeof(SavedSettings) * (NUM_STATE_SLOTS + 1))

// DISPLAY
// MAIN SCREEN DISPLAY CONFIG FOR EACH POT
struct PotDisplay {
  int circleX;
  int circleY;
  int textX;
  int textY;
};

// MAIN SCREEN POT DISPLAY CONFIGURATION
const PotDisplay potDisplays[N_POTS] = {
  { 16, 12, 10, 22 },    // A0
  { 48, 12, 42, 22 },    // A1
  { 80, 12, 74, 22 },    // A2
  { 112, 12, 106, 22 },  // A3
  { 16, 44, 10, 54 },    // A6
  { 48, 44, 42, 54 },    // A7
  { 80, 44, 74, 54 },    // A8
  { 112, 44, 106, 54 }   // A9
};

// CIRCLE POSITION MATRIX LOOK-UP TABLE - ASSIGN SCREEN DISPLAY
const struct {
  int x;
  int y;
} potCirclePositions[N_POTS] = {
  { 102, 8 },   // A0
  { 105, 8 },   // A1
  { 108, 8 },   // A2
  { 111, 8 },   // A3
  { 102, 11 },  // A6
  { 105, 11 },  // A7
  { 108, 11 },  // A8
  { 111, 11 }   // A9
};

// DOT POSITION MATRIX LOOK-UP TABLE - ASSIGN SCREEN DISPLAY
const int POT_MATRIX_POSITIONS[N_POTS][2] = {
  { 102, 7 },   // A0
  { 105, 7 },   // A1
  { 108, 7 },   // A2
  { 111, 7 },   // A3
  { 102, 10 },  // A6
  { 105, 10 },  // A7
  { 108, 10 },  // A8
  { 111, 10 }   // A9
};

// SCROLLING CONFIG
const int MAX_VISIBLE_MESSAGES = 4;  // Number of messages visible at once
int scrollOffset = 0;                // Current scroll position

// DISPLAY BITMAPS
// MAIN_SCREEN
static const unsigned char PROGMEM image_mainScreenInnerLines_bits[] = { 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x10, 0x7f, 0xff, 0xff, 0xf9, 0x3f, 0xff, 0xff, 0xf2, 0x7f, 0xff, 0xff, 0xfc, 0x9f, 0xff, 0xff, 0xe0, 0x80, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00 };
// ASSIGN SCREEN
static const unsigned char PROGMEM image_ctrlMessageIndicator_bits[] = { 0x20, 0x60, 0xe0, 0x60, 0x20 };
static const unsigned char PROGMEM image_potMatrixGrid_bits[] = { 0x92, 0x40, 0x00, 0x00, 0x00, 0x00, 0x92, 0x40 };
static const unsigned char PROGMEM image_valueIndicator_bits[] = { 0xfc };


// ***** //
// SETUP
// ***** //
void setup() {

  // BEGIN SERIAL MONITORING
  // SERIAL1 IS FOR MIDI OUTPPUT
  Serial.begin(9600);
  Serial1.begin(31250);
  MIDI.begin();
  Serial1.setTX(1);  // SET MIDI OUTPUT PIN

  // POWER LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // INITIALISE
  initController();
}

// *************************** //
// MAIN
// - BUTTONS BEING READ
// - POTENTIOMETERS BEING READ
// - MIDI OUTPUT
// - SCREEN UPDATES
// *************************** //
void loop() {

  // BUTTON HANDLING
  readButtons();

  // Process all potentiometers
  for (int i = 0; i < N_POTS; i++) {
    potReading[i] = analogRead(POT_PIN[i]);
    responsivePot[i].update(potReading[i]);
    potState[i] = responsivePot[i].getValue();
    currentMidiValue[i] = map(potState[i], 0, 1023, 0, 127);

    int potVar = abs(potState[i] - potPState[i]);
    unsigned long currentTime = millis();

    if (potVar > potThreshold) {
      pPotTime[i] = currentTime;
    }

    potTimer[i] = currentTime - pPotTime[i];

    if (potTimer[i] < POT_TIMEOUT) {
      if (currentMidiValue[i] != potPState[i]) {
        // Only send MIDI messages when in main screen
        if (currentScreen == MAIN_SCREEN) {
          // Check if temporary override is active
          if (tempOverrideActive) {
            // Store temporary values but don't update the original messages
            tempMidiValues[i] = currentMidiValue[i];
            // Send all messages for this pot with temporary values
            for (int j = 0; j < messageCount[i]; j++) {
              // Apply range and direction to the temporary value
              int finalValue = mapMidiValueWithParams(currentMidiValue[i],
                                                      potMessages[i][j].minValue,
                                                      potMessages[i][j].maxValue,
                                                      potMessages[i][j].inverted);
              MIDI.sendControlChange(potMessages[i][j].cc, finalValue, potMessages[i][j].channel + 1);
            }
          } else {
            // Normal operation
            if (catchUpActive[i]) {
              // DEBUG: Print catch-up info
              Serial.print("CATCHUP Pot ");
              Serial.print(i);
              Serial.print(": potState=");
              Serial.print(potState[i]);
              Serial.print(", startPotPos=");
              Serial.print(catchUpStartPotPos[i]);
              Serial.print(", startValue=");
              Serial.print(catchUpStartValue[i]);

              // Calculate how far the pot has moved from the catch-up start position
              int potMovement = potState[i] - catchUpStartPotPos[i];
              Serial.print(", movement=");
              Serial.print(potMovement);

              // Use a smaller threshold for more responsive catch-up
              if (abs(potMovement) > 20) {  // Reduced from 50 to 20 for better responsiveness
                // Calculate scaled MIDI value
                int scaledMidiValue;

                if (potMovement > 0) {
                  // Moving forward from catch-up position
                  // Scale from catch-up start value to 127 based on pot movement
                  scaledMidiValue = map(potState[i], catchUpStartPotPos[i], 1023, catchUpStartValue[i], 127);
                } else {
                  // Moving backward from catch-up position
                  // Scale from catch-up start value to 0 based on pot movement
                  scaledMidiValue = map(potState[i], catchUpStartPotPos[i], 0, catchUpStartValue[i], 0);
                }

                // Clamp the value to 0-127 range
                scaledMidiValue = constrain(scaledMidiValue, 0, 127);

                // Update all messages for this pot with range and direction applied
                for (int j = 0; j < messageCount[i]; j++) {
                  // Apply range and direction to the scaled value
                  int finalValue = mapMidiValueWithParams(scaledMidiValue,
                                                          potMessages[i][j].minValue,
                                                          potMessages[i][j].maxValue,
                                                          potMessages[i][j].inverted);
                  potMessages[i][j].value = finalValue;
                  MIDI.sendControlChange(potMessages[i][j].cc, finalValue, potMessages[i][j].channel + 1);
                }

                Serial.print(", scaledValue=");
                Serial.print(scaledMidiValue);
                Serial.print(", finalValue=");
                Serial.println(potMessages[i][0].value);

                // If we've reached the end of the scaled range, disable catch-up
                if (scaledMidiValue == 0 || scaledMidiValue == 127) {
                  catchUpActive[i] = false;
                  Serial.print("Catch-up COMPLETE for pot ");
                  Serial.print(i);
                  Serial.print(" - Final value: ");
                  Serial.println(potMessages[i][0].value);
                }
              } else {
                // Still within the catch-up dead zone - send the stored value with range/direction
                for (int j = 0; j < messageCount[i]; j++) {
                  // Apply range and direction to the stored value
                  int finalValue = mapMidiValueWithParams(potMessages[i][j].value,
                                                          potMessages[i][j].minValue,
                                                          potMessages[i][j].maxValue,
                                                          potMessages[i][j].inverted);
                  MIDI.sendControlChange(potMessages[i][j].cc, finalValue, potMessages[i][j].channel + 1);
                }
                Serial.println(" (in dead zone)");
              }
            } else {
              // Normal operation - update and send all messages with range and direction
              for (int j = 0; j < messageCount[i]; j++) {
                // Apply range and direction to the current MIDI value
                int finalValue = mapMidiValueWithParams(currentMidiValue[i],
                                                        potMessages[i][j].minValue,
                                                        potMessages[i][j].maxValue,
                                                        potMessages[i][j].inverted);
                potMessages[i][j].value = finalValue;
                MIDI.sendControlChange(potMessages[i][j].cc, finalValue, potMessages[i][j].channel + 1);
              }
            }
          }
        }

        Serial.print("A");
        Serial.print(POT_PIN[i] - A0);
        Serial.print(": ");
        Serial.print(currentMidiValue[i]);
        if (catchUpActive[i]) {
          Serial.print(" (Catch-up active)");
        }
        Serial.print(" -> ");
        if (messageCount[i] > 0) {
          Serial.print(potMessages[i][0].value);
        } else {
          Serial.print("0");
        }
        Serial.println("/127");
      }
      potPState[i] = currentMidiValue[i];
    }

    // Check if pot has returned to normal position and disable catch-up
    // This should only happen after the pot has moved significantly
    if (!tempOverrideActive && catchUpActive[i]) {
      // Check if the current pot position would normally send the stored MIDI value
      int expectedMidiForPot = map(potState[i], 0, 1023, 0, 127);
      int storedMidiValue = potMessages[i][0].value;

      // If pot position matches stored value within a reasonable range
      if (abs(expectedMidiForPot - storedMidiValue) < 5) {
        catchUpActive[i] = false;
        Serial.print("Catch-up DISABLED for pot ");
        Serial.print(i);
        Serial.print(" - Pot aligned with stored value: ");
        Serial.print(expectedMidiForPot);
        Serial.print(" vs ");
        Serial.println(storedMidiValue);
      }
    }
  }

  // Draw the appropriate screen
  // Draw the appropriate screen
  if (currentScreen == MAIN_SCREEN) {
    drawMainScreen();
  } else if (currentScreen == MENU_SCREEN) {
    drawMenuScreen();
  } else if (currentScreen == ASSIGN_SCREEN) {
    drawAssignScreen();
  } else if (currentScreen == STATES_SCREEN) {
    drawStatesMenuScreen();
  } else if (currentScreen == SETTINGS_SCREEN) {
    drawSettingsMenuScreen();
  } else if (currentScreen == DISPLAY_SETTINGS_SCREEN) {
    drawDisplaySettings();
  } else if (currentScreen == ABOUT_SCREEN) {
    drawAboutScreen();
  } else if (currentScreen == RESET_SCREEN) {
    drawConfirmResetPopup(); 
  } else if (currentScreen == SAVE_STATES_SCREEN) {
    drawSaveStates();
  } else if (currentScreen == LOAD_STATES_SCREEN) {
    drawLoadStates();
  } else if (currentScreen == CLEAR_STATES_SCREEN) {
    drawClearStates();
  } else if (currentScreen == CONFIRM_SAVE_SCREEN) {
    drawConfirmSavePopup();
  } else if (currentScreen == CONFIRM_ASSIGN_SAVE_SCREEN) {
    drawConfirmAssignSavePopup();
  } else if (currentScreen == CONFIRM_RESET_SCREEN) {
    drawConfirmResetPopup();
  }
}

// *************** //
// BUTTON HANDLING
// *************** //
void readButtons() {

  // TRACK BUTTONS HELD
  assignButtonHeld = (buttonState[0] == HIGH);
  enterButtonHeld = (buttonState[1] == HIGH);

  // TRACKING ADD/REMOVE OPERATION
  static bool inAddRemoveOperation = false;

  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(BUTTON_PINS[i]);

    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonState[i]) {
        if (reading == HIGH) {

          // BUTTON PRESS CHECK
          buttonState[i] = HIGH;
          Serial.print("Button pressed: ");
          Serial.println(BUTTON_NAMES[i]);

          // *********************** //
          // SINGLE PRESS ACTION BUTTON HANDLING
          // *********************** //
          switch (i) {

            case 0:  // ASSIGN BUTTON
              if (currentScreen == MAIN_SCREEN) {
                // Single press from MAIN_SCREEN goes to MENU_SCREEN
                currentScreen = MENU_SCREEN;
                selectedMenuItem = 0;  // Reset to first menu item
                Serial.println("ASSIGN pressed - switching to MENU_SCREEN");
              } else if (currentScreen == MENU_SCREEN) {
                // From MENU_SCREEN, ASSIGN button goes back to MAIN_SCREEN
                currentScreen = MAIN_SCREEN;
                Serial.println("ASSIGN pressed - returning to MAIN_SCREEN");
              } else if (currentScreen == ASSIGN_SCREEN && buttonState[1] == LOW && buttonState[2] == LOW && buttonState[3] == LOW) {
                // Single press on ASSIGN_SCREEN now shows confirmation popup
                currentScreen = CONFIRM_ASSIGN_SAVE_SCREEN;
                confirmAssignSave = true;  // Default to Yes
                Serial.println("ASSIGN pressed - showing save confirmation");
              } else if (currentScreen == CONFIRM_ASSIGN_SAVE_SCREEN) {
                // On the confirm screen, ASSIGN button cancels and returns to ASSIGN_SCREEN
                currentScreen = ASSIGN_SCREEN;
                Serial.println("ASSIGN pressed - returning to ASSIGN_SCREEN");
              } else if (currentScreen == STATES_SCREEN) {
                // From STATES_SCREEN, ASSIGN button goes back to MENU_SCREEN
                currentScreen = MENU_SCREEN;
                selectedMenuItem = 1;  // Keep States selected in menu
                Serial.println("ASSIGN pressed - returning to MENU_SCREEN from STATES_SCREEN");
              } else if (currentScreen == SETTINGS_SCREEN) {
                // From SETTINGS_SCREEN, ASSIGN button goes back to MENU_SCREEN
                currentScreen = MENU_SCREEN;
                selectedMenuItem = 2;  // Keep Settings selected in menu
                Serial.println("ASSIGN pressed - returning to MENU_SCREEN from SETTINGS_SCREEN");
              } else if (currentScreen == DISPLAY_SETTINGS_SCREEN) {
                if (editingDisplaySetting) {
                  // Exit edit mode without saving changes - revert to original value
                  editingDisplaySetting = false;
                  displayInverted = originalDisplayInverted;  // Revert to saved state
                  Serial.print("ASSIGN pressed - exiting edit mode without saving, reverted to: ");
                  Serial.println(displayInverted ? "YES" : "NO");
                } else {
                  // From DISPLAY_SETTINGS_SCREEN, ASSIGN button goes back to SETTINGS_SCREEN
                  currentScreen = SETTINGS_SCREEN;
                  selectedMenuItem = 0;  // Keep Display selected in settings menu
                  Serial.println("ASSIGN pressed - returning to SETTINGS_SCREEN from DISPLAY_SETTINGS_SCREEN");
                }
              } else if (currentScreen == ABOUT_SCREEN) {
                // From ABOUT_SCREEN, ASSIGN button goes back to SETTINGS_SCREEN
                currentScreen = SETTINGS_SCREEN;
                selectedMenuItem = 1;  // Keep About selected in settings menu
                Serial.println("ASSIGN pressed - returning to SETTINGS_SCREEN from ABOUT_SCREEN");
              } else if (currentScreen == RESET_SCREEN) {
                // From RESET_SCREEN, ASSIGN button goes back to SETTINGS_SCREEN
                currentScreen = SETTINGS_SCREEN;
                selectedMenuItem = 2;  // Keep Factory Reset selected in settings menu
                Serial.println("ASSIGN pressed - returning to SETTINGS_SCREEN from RESET_SCREEN");
              } else if (currentScreen == CONFIRM_RESET_SCREEN) {
                // Cancel reset and return to SETTINGS_SCREEN
                currentScreen = SETTINGS_SCREEN;
                selectedMenuItem = 2;  // Keep Factory Reset selected
                Serial.println("ASSIGN pressed - Factory reset cancelled, returning to SETTINGS_SCREEN");
              } else if (currentScreen == SAVE_STATES_SCREEN) {
                // From SAVE_STATES_SCREEN, ASSIGN button goes back to STATES_SCREEN
                currentScreen = STATES_SCREEN;
                selectedMenuItem = 0;  // Reset to first menu item
                Serial.println("ASSIGN pressed - returning to STATES_SCREEN from SAVE_STATES_SCREEN");
              } else if (currentScreen == LOAD_STATES_SCREEN) {
                // From LOAD_STATES_SCREEN, ASSIGN button goes back to STATES_SCREEN
                currentScreen = STATES_SCREEN;
                selectedMenuItem = 0;
                Serial.println("ASSIGN pressed - returning to STATES_SCREEN from LOAD_STATES_SCREEN");
              } else if (currentScreen == CLEAR_STATES_SCREEN) {
                // From CLEAR_STATES_SCREEN, ASSIGN button goes back to STATES_SCREEN
                currentScreen = STATES_SCREEN;
                selectedMenuItem = 0;
                Serial.println("ASSIGN pressed - returning to STATES_SCREEN from CLEAR_STATES_SCREEN");
              } else if (currentScreen == CONFIRM_SAVE_SCREEN) {
                // Cancel save and return to SAVE_STATES_SCREEN
                currentScreen = SAVE_STATES_SCREEN;
                Serial.println("ASSIGN pressed - Save cancelled, returning to SAVE_STATES_SCREEN");
              }
              break;

            case 1:  // ENTER BUTTON SINGLE PRESS
              // RESET ADD/REMOVE STATE ON ENTER PRESS
              inAddRemoveOperation = false;

              // MENU SCREEN - CONFIRM SELECTION
              if (currentScreen == MENU_SCREEN) {
                switch (selectedMenuItem) {
                  case 0:  // CC assign
                    currentScreen = ASSIGN_SCREEN;
                    selectedPot = 0;
                    selectedMessage = 0;
                    scrollOffset = 0;
                    // Reset assign screen navigation states
                    assignEditMode = ASSIGN_POT_SELECT;
                    assignEditingMode = false;
                    Serial.println("ENTER pressed - switching to ASSIGN_SCREEN");
                    break;
                  case 1:                           // States
                    currentScreen = STATES_SCREEN;  // Switch to States screen
                    selectedMenuItem = 0;           // Reset selection for States menu
                    Serial.println("ENTER pressed - switching to STATES_SCREEN");
                    break;
                  case 2:                             // Settings
                    currentScreen = SETTINGS_SCREEN;  // Switch to Settings screen
                    selectedMenuItem = 0;             // Reset selection for Settings menu
                    Serial.println("ENTER pressed - switching to SETTINGS_SCREEN");
                    break;
                }
              }

              // STATES SCREEN - CONFIRM SELECTION
              else if (currentScreen == STATES_SCREEN) {
                switch (selectedMenuItem) {
                  case 0:  // Save state
                    currentScreen = SAVE_STATES_SCREEN;
                    selectedMenuItem = 0;  // Reset to first state slot
                    Serial.println("ENTER pressed - switching to SAVE_STATES_SCREEN");
                    break;
                  case 1:  // Load state
                    currentScreen = LOAD_STATES_SCREEN;
                    selectedMenuItem = 0;  // Reset to first state slot
                    Serial.println("ENTER pressed - switching to LOAD_STATES_SCREEN");
                    break;
                  case 2:  // Clear state
                    currentScreen = CLEAR_STATES_SCREEN;
                    selectedMenuItem = 0;  // Reset to first state slot
                    Serial.println("ENTER pressed - switching to CLEAR_STATES_SCREEN");
                    break;
                }
              }

              // SETTINGS SCREEN - CONFIRM SELECTION
              else if (currentScreen == SETTINGS_SCREEN) {
                switch (selectedMenuItem) {
                  case 0:  // Display
                    currentScreen = DISPLAY_SETTINGS_SCREEN;
                    selectedMenuItem = 0;                       // Reset selection for Display settings
                    editingDisplaySetting = false;              // Ensure not in edit mode when entering
                    originalDisplayInverted = displayInverted;  // Sync original with current
                    Serial.print("ENTER pressed - switching to DISPLAY_SETTINGS_SCREEN, current invert: ");
                    Serial.println(displayInverted ? "YES" : "NO");
                    break;
                  case 1:  // About
                    currentScreen = ABOUT_SCREEN;
                    selectedMenuItem = 0;  // Reset selection for About screen
                    Serial.println("ENTER pressed - switching to ABOUT_SCREEN");
                    break;
                  case 2:  // Factory Reset
                    currentScreen = CONFIRM_RESET_SCREEN;
                    confirmSelection = true;  // Default to Yes
                    Serial.println("ENTER pressed - switching to CONFIRM_RESET_SCREEN");
                    break;
                }
              }

              // DISPLAY SETTINGS SCREEN - CONFIRM SELECTION
              else if (currentScreen == DISPLAY_SETTINGS_SCREEN) {
                Serial.print("ENTER on DISPLAY_SETTINGS_SCREEN, selectedMenuItem=");
                Serial.print(selectedMenuItem);
                Serial.print(", editingDisplaySetting=");
                Serial.print(editingDisplaySetting);
                Serial.print(", displayInverted=");
                Serial.println(displayInverted ? "YES" : "NO");

                switch (selectedMenuItem) {
                  case 0:  // Invert Display
                    if (!editingDisplaySetting) {
                      // Store the current setting before entering edit mode
                      originalDisplayInverted = displayInverted;
                      // Enter edit mode
                      editingDisplaySetting = true;
                      editFlashTimer = millis();
                      flashState = true;
                      Serial.println("ENTER pressed - EDIT MODE ACTIVATED");
                      Serial.print("Original value stored as: ");
                      Serial.println(originalDisplayInverted ? "YES" : "NO");
                    } else {
                      // Exit edit mode and save setting
                      editingDisplaySetting = false;
                      Serial.println("ENTER pressed - Display invert setting saved");
                      // Save to global EEPROM
                      saveGlobalSettingsToEEPROM();
                    }
                    break;
                }
              }

              // SAVE STATES SCREEN - CONFIRM SELECTION
              else if (currentScreen == SAVE_STATES_SCREEN) {
                Serial.print("ENTER pressed - Preparing to save to State #");
                Serial.println(selectedMenuItem + 1);

                // Store the slot we're about to save to
                pendingSaveSlot = selectedMenuItem;
                confirmSelection = true;  // Default to Yes
                currentScreen = CONFIRM_SAVE_SCREEN;
                Serial.println("Switching to CONFIRM_SAVE_SCREEN");
              }

              // LOAD STATES SCREEN - CONFIRM SELECTION
              else if (currentScreen == LOAD_STATES_SCREEN) {
                Serial.print("ENTER pressed - Loading State #");
                Serial.println(selectedMenuItem + 1);

                // Load settings from the selected state slot
                loadStateFromSlot(selectedMenuItem);

                // Return to STATES_SCREEN after loading
                currentScreen = STATES_SCREEN;
                selectedMenuItem = 0;
                Serial.println("Returning to STATES_SCREEN");
              }

              // CLEAR STATES SCREEN - CONFIRM SELECTION
              else if (currentScreen == CLEAR_STATES_SCREEN) {
                Serial.print("ENTER pressed - Clearing State #");
                Serial.println(selectedMenuItem + 1);

                // Clear the selected state slot
                clearStateFromSlot(selectedMenuItem);

                // Return to STATES_SCREEN after clearing
                currentScreen = STATES_SCREEN;
                selectedMenuItem = 0;
                Serial.println("Returning to STATES_SCREEN");
              }

              // CONFIRM SAVE SCREEN - CONFIRM SELECTION
              else if (currentScreen == CONFIRM_SAVE_SCREEN) {
                if (confirmSelection) {
                  // User selected YES - save the state
                  Serial.print("CONFIRM - Saving state to slot ");
                  Serial.println(pendingSaveSlot + 1);
                  saveStateToSlot(pendingSaveSlot);
                } else {
                  // User selected NO - don't save
                  Serial.println("CONFIRM - Save cancelled");
                }

                // Return to SAVE_STATES_SCREEN
                currentScreen = SAVE_STATES_SCREEN;
                Serial.println("Returning to SAVE_STATES_SCREEN");
              }

              // CONFIRM ASSIGN SAVE SCREEN - CONFIRM SELECTION
              else if (currentScreen == CONFIRM_ASSIGN_SAVE_SCREEN) {
                if (confirmAssignSave) {
                  // User selected YES - save settings to current state
                  Serial.println("CONFIRM ASSIGN - Saving settings to current state");

                  // Save to the currently loaded state slot, or slot 0 if none loaded
                  int saveSlot = (currentStateSlot >= 0) ? currentStateSlot : 0;
                  saveStateToSlot(saveSlot);
                  
                  // Update currentStateSlot if it was -1
                  if (currentStateSlot < 0) {
                    currentStateSlot = saveSlot;
                    saveGlobalSettingsToEEPROM();
                  }

                  // Visual feedback - brief display flash
                  display.invertDisplay(true);
                  delay(100);
                  display.invertDisplay(false);

                  // Return to MAIN_SCREEN after successful save
                  currentScreen = MAIN_SCREEN;
                  Serial.println("Returning to MAIN_SCREEN");
                } else {
                  // User selected NO - don't save, return to ASSIGN_SCREEN
                  Serial.println("CONFIRM ASSIGN - Save cancelled");
                  currentScreen = ASSIGN_SCREEN;
                  Serial.println("Returning to ASSIGN_SCREEN");
                }
              }

              // CONFIRM RESET SCREEN - CONFIRM SELECTION
              else if (currentScreen == CONFIRM_RESET_SCREEN) {
                if (confirmSelection) {
                  // User selected YES - perform factory reset
                  Serial.println("CONFIRM - Performing factory reset");
                  factoryReset();
                  // Return to MAIN_SCREEN after reset
                  currentScreen = MAIN_SCREEN;
                  Serial.println("Returning to MAIN_SCREEN");
                } else {
                  // User selected NO - cancel reset
                  Serial.println("CONFIRM - Factory reset cancelled");
                  currentScreen = SETTINGS_SCREEN;
                  selectedMenuItem = 2;  // Keep Factory Reset selected
                  Serial.println("Returning to SETTINGS_SCREEN");
                }
              }

              // ABOUT SCREEN - No selections to confirm, but you could add functionality here
              else if (currentScreen == ABOUT_SCREEN) {
                // About screen is informational only, no actions needed
                Serial.println("ENTER pressed on ABOUT_SCREEN - no action");
              }

              // RESET SCREEN - No selections to confirm
              else if (currentScreen == RESET_SCREEN) {
                // Reset screen is just informational, no actions needed
                Serial.println("ENTER pressed on RESET_SCREEN - no action");
              }

              // ASSIGN SCREEN - Handle ENTER press for navigation and editing
              else if (currentScreen == ASSIGN_SCREEN) {
                if (assignEditingMode) {
                  // Exit edit mode and cycle to next parameter or exit
                  switch(assignEditMode) {
                    case ASSIGN_EDIT_CHANNEL:
                      assignEditMode = ASSIGN_EDIT_CC;
                      Serial.println("Now editing CC");
                      break;
                    case ASSIGN_EDIT_CC:
                      assignEditMode = ASSIGN_EDIT_INVERT;
                      Serial.println("Now editing Direction");
                      break;
                    case ASSIGN_EDIT_INVERT:
                      assignEditMode = ASSIGN_EDIT_MIN;
                      Serial.println("Now editing Min Value");
                      break;
                    case ASSIGN_EDIT_MIN:
                      assignEditMode = ASSIGN_EDIT_MAX;
                      Serial.println("Now editing Max Value");
                      break;
                    case ASSIGN_EDIT_MAX:
                      assignEditMode = ASSIGN_MESSAGE_SELECT;
                      assignEditingMode = false;
                      Serial.println("Exited edit mode");
                      break;
                    default:
                      // Handle any other cases
                      assignEditMode = ASSIGN_MESSAGE_SELECT;
                      assignEditingMode = false;
                      break;
                  }
                } else {
                  // Enter edit mode based on current selection
                  if (assignEditMode == ASSIGN_POT_SELECT) {
                    // Enter message selection mode
                    assignEditMode = ASSIGN_MESSAGE_SELECT;
                    Serial.println("Entered message selection mode");
                  } else if (assignEditMode == ASSIGN_MESSAGE_SELECT && messageCount[selectedPot] > 0) {
                    // Enter edit mode starting with channel
                    assignEditingMode = true;
                    assignEditMode = ASSIGN_EDIT_CHANNEL;
                    Serial.println("Entered edit mode - editing Channel");
                  }
                }
              }

              // Check if we're in MAIN_SCREEN and this is the start of a press
              if (currentScreen == MAIN_SCREEN && !tempOverrideActive) {
                // Store the current time for potential override activation
                enterPressTime = millis();
                Serial.println("ENTER pressed in MAIN_SCREEN - waiting to activate temporary override");
              }
              break;

            case 2:  // PREV BUTTON SINGLE PRESS
              prevButtonPressed = true;

              // *********************** //
              // MENU SCREEN NAVIGATION
              // *********************** //
              if (currentScreen == MENU_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 3) % 3;  // 3 menu items
                Serial.print("Menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("CC assign"); break;
                  case 1: Serial.println("States"); break;
                  case 2: Serial.println("Settings"); break;
                }
              }

              // *********************** //
              // STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 3) % 3;  // 3 states menu items
                Serial.print("States menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("Save state"); break;
                  case 1: Serial.println("Load state"); break;
                  case 2: Serial.println("Clear state"); break;
                }
              }

              // *********************** //
              // SETTINGS SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == SETTINGS_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 3) % 3;  // Now 3 settings menu items
                Serial.print("Settings menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("Display"); break;
                  case 1: Serial.println("About"); break;
                  case 2: Serial.println("Factory Reset"); break;
                }
              }

              // *********************** //
              // DISPLAY SETTINGS SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == DISPLAY_SETTINGS_SCREEN) {
                Serial.print("PREV on DISPLAY_SETTINGS_SCREEN, editingDisplaySetting=");
                Serial.print(editingDisplaySetting);
                Serial.print(", current displayInverted=");
                Serial.println(displayInverted ? "YES" : "NO");

                if (editingDisplaySetting) {
                  // Force selectedMenuItem to 0 so the logic stays on this line
                  selectedMenuItem = 0;

                  // Toggle the actual boolean that moves the box
                  bool oldValue = displayInverted;
                  displayInverted = !displayInverted;

                  Serial.print("PREV toggled displayInverted from ");
                  Serial.print(oldValue ? "YES" : "NO");
                  Serial.print(" to ");
                  Serial.println(displayInverted ? "YES" : "NO");
                } else {
                  // Not in edit mode - pressing PREV should enter edit mode
                  editingDisplaySetting = true;
                  selectedMenuItem = 0;
                  editFlashTimer = millis();
                  flashState = true;
                  Serial.println("PREV pressed - entering edit mode");
                }
              }

              // *********************** //
              // SAVE STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == SAVE_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 8) % 8;  // 8 state slots
                Serial.print("Save state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // LOAD STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == LOAD_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 8) % 8;  // 8 state slots
                Serial.print("Load state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // CLEAR STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CLEAR_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem - 1 + 8) % 8;  // 8 state slots
                Serial.print("Clear state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // CONFIRM SAVE SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_SAVE_SCREEN) {
                // Toggle between Y and N
                confirmSelection = !confirmSelection;
                Serial.print("Confirm selection toggled to: ");
                Serial.println(confirmSelection ? "YES" : "NO");
              }

              // *********************** //
              // CONFIRM ASSIGN SAVE SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_ASSIGN_SAVE_SCREEN) {
                // Toggle between Y and N
                confirmAssignSave = !confirmAssignSave;
                Serial.print("Confirm assign save toggled to: ");
                Serial.println(confirmAssignSave ? "YES" : "NO");
              }

              // *********************** //
              // CONFIRM RESET SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_RESET_SCREEN) {
                // Toggle between Y and N
                confirmSelection = !confirmSelection;
                Serial.print("Confirm selection toggled to: ");
                Serial.println(confirmSelection ? "YES" : "NO");
              }

              // *********************** //
              // ASSIGN SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == ASSIGN_SCREEN) {
                if (assignEditingMode) {
                  // Edit mode - modify values
                  switch(assignEditMode) {
                    case ASSIGN_EDIT_CHANNEL:
                      potMessages[selectedPot][selectedMessage].channel = 
                        (potMessages[selectedPot][selectedMessage].channel - 1 + 16) % 16;
                      Serial.print("Channel set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].channel + 1);
                      break;
                    case ASSIGN_EDIT_CC:
                      potMessages[selectedPot][selectedMessage].cc = 
                        (potMessages[selectedPot][selectedMessage].cc - 1 + 128) % 128;
                      Serial.print("CC set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].cc);
                      break;
                    case ASSIGN_EDIT_INVERT:
                      potMessages[selectedPot][selectedMessage].inverted = 
                        !potMessages[selectedPot][selectedMessage].inverted;
                      Serial.print("Direction set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].inverted ? "Inverted" : "Normal");
                      break;
                    case ASSIGN_EDIT_MIN:
                      if (potMessages[selectedPot][selectedMessage].minValue > 0) {
                        potMessages[selectedPot][selectedMessage].minValue--;
                        if (potMessages[selectedPot][selectedMessage].minValue > 
                            potMessages[selectedPot][selectedMessage].maxValue) {
                          potMessages[selectedPot][selectedMessage].maxValue = 
                            potMessages[selectedPot][selectedMessage].minValue;
                        }
                      }
                      Serial.print("Min value set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].minValue);
                      break;
                    case ASSIGN_EDIT_MAX:
                      if (potMessages[selectedPot][selectedMessage].maxValue < 127) {
                        potMessages[selectedPot][selectedMessage].maxValue++;
                        if (potMessages[selectedPot][selectedMessage].maxValue < 
                            potMessages[selectedPot][selectedMessage].minValue) {
                          potMessages[selectedPot][selectedMessage].minValue = 
                            potMessages[selectedPot][selectedMessage].maxValue;
                        }
                      }
                      Serial.print("Max value set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].maxValue);
                      break;
                    default:
                      break;
                  }
                } else {
                  // Selection mode - navigate between pots or messages
                  if (assignEditMode == ASSIGN_POT_SELECT) {
                    selectedPot = (selectedPot - 1 + N_POTS) % N_POTS;
                    selectedMessage = 0;
                    scrollOffset = 0;
                    Serial.print("Selected Pot ");
                    Serial.println(selectedPot + 1);
                  } else if (assignEditMode == ASSIGN_MESSAGE_SELECT && messageCount[selectedPot] > 0) {
                    selectedMessage = (selectedMessage - 1 + messageCount[selectedPot]) % messageCount[selectedPot];
                    Serial.print("Selected Message ");
                    Serial.println(selectedMessage);
                  }
                }
              }
              break;

            case 3:  // NEXT BUTTON
              nextButtonPressed = true;

              // *********************** //
              // MENU SCREEN NAVIGATION
              // *********************** //
              if (currentScreen == MENU_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 3;  // 3 menu items
                Serial.print("Menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("CC assign"); break;
                  case 1: Serial.println("States"); break;
                  case 2: Serial.println("Settings"); break;
                }
              }

              // *********************** //
              // STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 3;  // 3 states menu items
                Serial.print("States menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("Save state"); break;
                  case 1: Serial.println("Load state"); break;
                  case 2: Serial.println("Clear state"); break;
                }
              }

              // *********************** //
              // SETTINGS SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == SETTINGS_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 3;  // Now 3 settings menu items
                Serial.print("Settings menu selection: ");
                switch (selectedMenuItem) {
                  case 0: Serial.println("Display"); break;
                  case 1: Serial.println("About"); break;
                  case 2: Serial.println("Factory Reset"); break;
                }
              }

              // *********************** //
              // DISPLAY SETTINGS SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == DISPLAY_SETTINGS_SCREEN) {
                if (editingDisplaySetting) {
                  // Force selectedMenuItem to 0 so the logic stays on this line
                  selectedMenuItem = 0;

                  // Toggle the actual boolean that moves the box
                  displayInverted = !displayInverted;

                  Serial.print("NEXT toggled displayInverted to: ");
                  Serial.println(displayInverted ? "YES" : "NO");
                } else {
                  // If not editing, make the first press enter edit mode
                  editingDisplaySetting = true;
                  selectedMenuItem = 0;
                  editFlashTimer = millis();
                  flashState = true;
                  Serial.println("NEXT pressed - entering edit mode");
                }
              }

              // *********************** //
              // SAVE STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == SAVE_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 8;  // 8 state slots
                Serial.print("Save state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // LOAD STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == LOAD_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 8;  // 8 state slots
                Serial.print("Load state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // CLEAR STATES SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CLEAR_STATES_SCREEN) {
                selectedMenuItem = (selectedMenuItem + 1) % 8;  // 8 state slots
                Serial.print("Clear state selection: State #");
                Serial.println(selectedMenuItem + 1);
              }

              // *********************** //
              // CONFIRM SAVE SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_SAVE_SCREEN) {
                // Toggle between Y and N
                confirmSelection = !confirmSelection;
                Serial.print("Confirm selection toggled to: ");
                Serial.println(confirmSelection ? "YES" : "NO");
              }

              // *********************** //
              // CONFIRM ASSIGN SAVE SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_ASSIGN_SAVE_SCREEN) {
                // Toggle between Y and N
                confirmAssignSave = !confirmAssignSave;
                Serial.print("Confirm assign save toggled to: ");
                Serial.println(confirmAssignSave ? "YES" : "NO");
              }

              // *********************** //
              // CONFIRM RESET SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == CONFIRM_RESET_SCREEN) {
                // Toggle between Y and N
                confirmSelection = !confirmSelection;
                Serial.print("Confirm selection toggled to: ");
                Serial.println(confirmSelection ? "YES" : "NO");
              }

              // *********************** //
              // ASSIGN SCREEN NAVIGATION
              // *********************** //
              else if (currentScreen == ASSIGN_SCREEN) {
                if (assignEditingMode) {
                  // Edit mode - modify values
                  switch(assignEditMode) {
                    case ASSIGN_EDIT_CHANNEL:
                      potMessages[selectedPot][selectedMessage].channel = 
                        (potMessages[selectedPot][selectedMessage].channel + 1) % 16;
                      Serial.print("Channel set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].channel + 1);
                      break;
                    case ASSIGN_EDIT_CC:
                      potMessages[selectedPot][selectedMessage].cc = 
                        (potMessages[selectedPot][selectedMessage].cc + 1) % 128;
                      Serial.print("CC set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].cc);
                      break;
                    case ASSIGN_EDIT_INVERT:
                      potMessages[selectedPot][selectedMessage].inverted = 
                        !potMessages[selectedPot][selectedMessage].inverted;
                      Serial.print("Direction set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].inverted ? "Inverted" : "Normal");
                      break;
                    case ASSIGN_EDIT_MIN:
                      if (potMessages[selectedPot][selectedMessage].minValue < 127) {
                        potMessages[selectedPot][selectedMessage].minValue++;
                        if (potMessages[selectedPot][selectedMessage].minValue > 
                            potMessages[selectedPot][selectedMessage].maxValue) {
                          potMessages[selectedPot][selectedMessage].maxValue = 
                            potMessages[selectedPot][selectedMessage].minValue;
                        }
                      }
                      Serial.print("Min value set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].minValue);
                      break;
                    case ASSIGN_EDIT_MAX:
                      if (potMessages[selectedPot][selectedMessage].maxValue > 0) {
                        potMessages[selectedPot][selectedMessage].maxValue--;
                        if (potMessages[selectedPot][selectedMessage].maxValue < 
                            potMessages[selectedPot][selectedMessage].minValue) {
                          potMessages[selectedPot][selectedMessage].minValue = 
                            potMessages[selectedPot][selectedMessage].maxValue;
                        }
                      }
                      Serial.print("Max value set to: ");
                      Serial.println(potMessages[selectedPot][selectedMessage].maxValue);
                      break;
                    default:
                      break;
                  }
                } else {
                  // Selection mode - navigate between pots or messages
                  if (assignEditMode == ASSIGN_POT_SELECT) {
                    selectedPot = (selectedPot + 1) % N_POTS;
                    selectedMessage = 0;
                    scrollOffset = 0;
                    Serial.print("Selected Pot ");
                    Serial.println(selectedPot + 1);
                  } else if (assignEditMode == ASSIGN_MESSAGE_SELECT && messageCount[selectedPot] > 0) {
                    selectedMessage = (selectedMessage + 1) % messageCount[selectedPot];
                    Serial.print("Selected Message ");
                    Serial.println(selectedMessage);
                  }
                }
              }
              break;
          }
        } else {

          // *********************** //
          // BUTTON RELEASE CHECK
          // *********************** //

          buttonState[i] = LOW;

          // *********************** //
          // HANDLE BUTTON RELEASES
          // *********************** //
          switch (i) {
            case 0:  // ASSIGN BUTTON RELEASE
              // Toggle between Channel/CC editing on ASSIGN_SCREEN (legacy behavior removed)
              // This functionality is now handled by the new navigation system
              assignButtonHeld = false;
              break;

            case 1:  // ENTER BUTTON RELEASE
              // RESET ADD/REMOVE STATE ON ENTER RELEASE
              inAddRemoveOperation = false;

              // Handle temporary override deactivation if active
              if (currentScreen == MAIN_SCREEN && tempOverrideActive) {
                deactivateTempOverride();
              }

              // Reset the press time
              enterPressTime = 0;
              break;

            case 2:  // PREV BUTTON RELEASE
              // ON ASSIGN SCREEN AND *ONLY* ENTER IS HELD
              // RELEASE OF NEXT/PREV NAVIGATES BETWEEN MIDI CONTROL MESSAGES (legacy behavior)
              // This is now handled by the new navigation system, but keep for compatibility
              if (prevButtonPressed && enterButtonHeld && currentScreen == ASSIGN_SCREEN && !inAddRemoveOperation) {
                if (messageCount[selectedPot] > 0) {
                  selectedMessage = (selectedMessage - 1 + messageCount[selectedPot]) % messageCount[selectedPot];
                  Serial.print("Selected Message ");
                  Serial.println(selectedMessage);
                }
              }
              prevButtonPressed = false;
              break;

            case 3:  // NEXT BUTTON RELEASE
              // ON ASSIGN SCREEN AND *ONLY* ENTER IS HELD
              // RELEASE OF NEXT/PREV NAVIGATES BETWEEN MIDI CONTROL MESSAGES (legacy behavior)
              // This is now handled by the new navigation system, but keep for compatibility
              if (nextButtonPressed && enterButtonHeld && currentScreen == ASSIGN_SCREEN && !inAddRemoveOperation) {
                if (messageCount[selectedPot] > 0) {
                  selectedMessage = (selectedMessage + 1) % messageCount[selectedPot];
                  Serial.print("Selected Message ");
                  Serial.println(selectedMessage);
                }
              }
              nextButtonPressed = false;
              break;
          }
        }
      }
    }
    lastButtonState[i] = reading;
  }

  // *********************** //
  // TEMPORARY MIDI OVERRIDE ACTIVATION
  // *********************** //
  // In MAIN_SCREEN, holding ENTER for a short time activates temporary override
  if (currentScreen == MAIN_SCREEN && buttonState[1] == HIGH && buttonState[0] == LOW && buttonState[2] == LOW && buttonState[3] == LOW) {
    if (!tempOverrideActive && enterPressTime > 0) {
      if (millis() - enterPressTime >= tempOverrideHoldTime) {
        activateTempOverride();
      }
    }
  }

  // *********************** //
  // MIDI MESSAGE MANAGEMENT
  // *********************** //

  // *********************** //
  // ADD NEW MIDI CONTROL TO POT
  // *********************** //
  // ON THE ASSIGN SCREEN AND HOLDING THE ENTER BUTTON AND THE NEXT BUTTON
  // ADDS NEW MIDI CONTROL MESSAGE TO SELECTED POT
  if (currentScreen == ASSIGN_SCREEN && buttonState[1] == HIGH && buttonState[3] == HIGH && buttonState[0] == LOW && buttonState[2] == LOW) {
    if (!enterNextHeld) {
      enterNextHeld = true;
      enterNextHoldStart = millis();
      Serial.println("ENTER+NEXT combination detected - start hold timer");
    } else if (millis() - enterNextHoldStart >= 1500) {
      addMidiControl();
      Serial.println("ENTER+NEXT held for 1.5s - adding new MIDI control");
      enterNextHeld = false;
      inAddRemoveOperation = true;  // SET FLAG TO INDICATE IN ADD/REMOVE MIDI OPERATION
    }
  } else {
    if (enterNextHeld) {
      Serial.println("ENTER+NEXT combination released");
      enterNextHeld = false;
    }
  }

  // *********************** //
  // REMOVE NEW MIDI CONTROL TO POT
  // *********************** //
  // ON THE ASSIGN SCREEN AND HOLDING THE ENTER BUTTON AND THE PREV BUTTON
  // REMOVES SELECTED MIDI CONTROL MESSAGE FROM SELECTED POT
  if (currentScreen == ASSIGN_SCREEN && buttonState[1] == HIGH && buttonState[2] == HIGH && buttonState[0] == LOW && buttonState[3] == LOW) {
    if (!enterPrevHeld) {
      enterPrevHeld = true;
      enterPrevHoldStart = millis();
      Serial.println("ENTER+PREV combination detected - start hold timer");
    } else if (millis() - enterPrevHoldStart >= 1500) {
      removeMidiControl();
      Serial.println("ENTER+PREV held for 1.5s - removing MIDI control");
      enterPrevHeld = false;
      inAddRemoveOperation = true;  // SET FLAG TO INDICATE IN ADD/REMOVE MIDI OPERATION
    }
  } else {
    if (enterPrevHeld) {
      Serial.println("ENTER+PREV combination released");
      enterPrevHeld = false;
    }
  }

  // *********************** //
  // RESET ALL MIDI CONTROL SETTINGS
  // *********************** //
  // ON THE ASSIGN SCREEN AND HOLDING THE NEXT BUTTON AND THE PREV BUTTON
  // RESETS ALL MIDI CONTROL MESSAGES BACK TO THE DEFAULT SETTINGS
  if (currentScreen == ASSIGN_SCREEN && buttonState[2] == HIGH && buttonState[3] == HIGH && buttonState[0] == LOW && buttonState[1] == LOW) {
    if (!prevNextHeld) {
      prevNextHeld = true;
      prevNextHoldStart = millis();
      Serial.println("PREV+NEXT combination detected - start hold timer");
    } else if (millis() - prevNextHoldStart >= prevNextHoldDuration) {

      // RESET
      //resetToDefaultSettings();
      Serial.println("PREV+NEXT held for 5s - reset to default settings");
      prevNextHeld = false;
    }
  } else {
    if (prevNextHeld) {
      Serial.println("PREV+NEXT combination released");
      prevNextHeld = false;
    }
  }
}

// *********************** //
// TEMPORARY OVERRIDE FUNCTIONS
// *********************** //
// *********************** //
// ACTIVATE TEMPORARY OVERRIDE
// *********************** //
void activateTempOverride() {
  if (!tempOverrideActive) {
    tempOverrideActive = true;

    // Store original MIDI values for all pots
    for (int i = 0; i < N_POTS; i++) {
      originalMidiValues[i] = potMessages[i][0].value;  // Store the stored MIDI value, not current
      tempMidiValues[i] = currentMidiValue[i];          // Initialize temp values
    }

    Serial.println("Temporary MIDI override ACTIVATED");
    Serial.println("All pot changes are temporary until ENTER is released");

    // VISUAL FEEDBACK FOR ACTIVATING
    if (!displayInverted) {
      display.invertDisplay(true);
    } else {
      display.invertDisplay(false);
    }

    delay(100);  // DEBOUNCE
  }
}

// *********************** //
// DEACTIVATE TEMPORARY OVERRIDE
// *********************** //
void deactivateTempOverride() {
  if (tempOverrideActive) {
    tempOverrideActive = false;

    // Send original MIDI values to restore state
    for (int i = 0; i < N_POTS; i++) {
      // Send all messages for this pot with original values
      for (int j = 0; j < messageCount[i]; j++) {
        MIDI.sendControlChange(potMessages[i][j].cc, potMessages[i][j].value, potMessages[i][j].channel + 1);
      }

      // Store current potentiometer position for catch-up
      postOverridePotPositions[i] = potState[i];       // Store the raw pot value (0-1023)
      catchUpStartValue[i] = potMessages[i][0].value;  // Store the MIDI value we're returning to
      catchUpStartPotPos[i] = potState[i];             // Store the pot position where catch-up starts
      catchUpActive[i] = true;                         // Enable catch-up for this pot

      Serial.print("Pot ");
      Serial.print(i);
      Serial.print(": Catch-up enabled. Stored MIDI value=");
      Serial.print(potMessages[i][0].value);
      Serial.print(", Current pot position=");
      Serial.print(potState[i]);
      Serial.print(" (MIDI=");
      Serial.print(currentMidiValue[i]);
      Serial.println(")");
    }

    Serial.println("Temporary MIDI override DEACTIVATED");
    Serial.println("All MIDI values restored to original state");
    Serial.println("Catch-up mode activated for all pots");

    // Restore to the saved display setting using the current displayInverted value
    if (displayInverted) {
      display.invertDisplay(true);
    } else {
      display.invertDisplay(false);
    }

    // Clear only the temporary override values, NOT catch-up values
    for (int i = 0; i < N_POTS; i++) {
      originalMidiValues[i] = 0;  // Clear original values
      tempMidiValues[i] = 0;      // Clear temp values
    }

    // Force screen refresh to show stored values immediately
    if (currentScreen == MAIN_SCREEN) {
      drawMainScreen();
    }
  }
}

// *********************** //
// RESET CATCHUP
// *********************** //
void resetCatchUp(int potIndex) {
  catchUpActive[potIndex] = false;
  postOverridePotPositions[potIndex] = 0;
  catchUpStartValue[potIndex] = 0;
  catchUpStartPotPos[potIndex] = 0;
}

// *********************** //
// MIDI FUNCTIONS
// *********************** //
// *********************** //
// ADD MIDI CONTROL FNC
// *********************** //
void addMidiControl() {
  if (messageCount[selectedPot] < MAX_MESSAGES_PER_POT) {
    potMessages[selectedPot][messageCount[selectedPot]].channel = 0;
    potMessages[selectedPot][messageCount[selectedPot]].cc = 1;
    potMessages[selectedPot][messageCount[selectedPot]].inverted = false;
    potMessages[selectedPot][messageCount[selectedPot]].minValue = 0;
    potMessages[selectedPot][messageCount[selectedPot]].maxValue = 127;
    potMessages[selectedPot][messageCount[selectedPot]].value = currentMidiValue[selectedPot];
    messageCount[selectedPot]++;
    scrollOffset = max(0, messageCount[selectedPot] - MAX_VISIBLE_MESSAGES);

    Serial.print("Added new MIDI message to pot ");
    Serial.print(selectedPot);
    Serial.print(". Total messages: ");
    Serial.println(messageCount[selectedPot]);
  } else {
    Serial.println("Maximum messages reached for this pot");
  }
}

// *********************** //
// REMOVE MIDI CONTROL FNC
// *********************** //
void removeMidiControl() {
  if (messageCount[selectedPot] > 1) {  // DON'T REMOVE LAST MESSAGE
    // SHIFT ALL MESSAGES DOWN ONE BASED ON WHICH WAS REMOVED
    for (int i = selectedMessage; i < messageCount[selectedPot] - 1; i++) {
      potMessages[selectedPot][i] = potMessages[selectedPot][i + 1];
    }
    messageCount[selectedPot]--;
    if (selectedMessage >= messageCount[selectedPot]) {
      selectedMessage = messageCount[selectedPot] - 1;
    }
    scrollOffset = min(scrollOffset, max(0, messageCount[selectedPot] - MAX_VISIBLE_MESSAGES));

    // ADJUST SELECTED MESSAGE IF NEEDED
    if (selectedMessage >= messageCount[selectedPot]) {
      selectedMessage = messageCount[selectedPot] - 1;
    }

    Serial.print("Removed MIDI message from pot ");
    Serial.print(selectedPot);
    Serial.print(". Total messages: ");
    Serial.println(messageCount[selectedPot]);
  } else {
    Serial.println("Cannot remove the last MIDI message");
  }
}


// *********************** //
// DISPLAY FUNCTIONS
// *********************** //
// *********************** //
// DRAW MAIN_SCREEN
// *********************** //
void drawMainScreen() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.fillRect(0, 0, 128, 64, bgColor);

  // UI
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);
  display.drawBitmap(2, 2, image_mainScreenInnerLines_bits, 124, 60, txtColor);

  // POT INFO
  display.setTextColor(txtColor);
  display.setTextWrap(false);

  // Draw all pot displays
  for (int i = 0; i < N_POTS; i++) {
    drawPotDisplay(i);
  }

  display.display();
}

// *********************** //
// DRAW POTENTIOMETERS ON MAIN_SCREEN
// *********************** //
void drawPotDisplay(int potIndex) {
  const int maxRadius = 5;
  const PotDisplay& disp = potDisplays[potIndex];

  // Set colors based on the inversion variable
  int fgColor = displayInverted ? 1 : 0;  // Foreground color (circles, text) - opposite of bg
  int bgColor = displayInverted ? 0 : 1;  // Background color (fill)

  // Determine which value to display:
  // - During temp override: show current physical value (tempMidiValues[potIndex])
  // - During catch-up: show the scaled MIDI value being sent
  // - Normal mode: show stored MIDI value (potMessages[potIndex][0].value)
  int displayValue;

  if (tempOverrideActive) {
    // During temporary override, show the temporary values
    displayValue = tempMidiValues[potIndex];
  } else if (catchUpActive[potIndex]) {
    // During catch-up, calculate the scaled value for display
    int potMovement = potState[potIndex] - catchUpStartPotPos[potIndex];
    if (abs(potMovement) > 20) {  // Match the threshold from main loop
      if (potMovement > 0) {
        displayValue = map(potState[potIndex], catchUpStartPotPos[potIndex], 1023, catchUpStartValue[potIndex], 127);
      } else {
        displayValue = map(potState[potIndex], catchUpStartPotPos[potIndex], 0, catchUpStartValue[potIndex], 0);
      }
      displayValue = constrain(displayValue, 0, 127);
    } else {
      // Still in dead zone, show stored value
      displayValue = (messageCount[potIndex] > 0) ? potMessages[potIndex][0].value : currentMidiValue[potIndex];
    }
  } else {
    // Normal mode - show stored MIDI value
    displayValue = (messageCount[potIndex] > 0) ? potMessages[potIndex][0].value : currentMidiValue[potIndex];
  }

  // CALCULATE CURRENT RADIUS BASED ON DISPLAY VALUE
  int currentRadius = map(displayValue, 0, 127, 0, maxRadius);

  // DRAW EXPANDING INNER CIRCLE TO REPRESENT MIDI VALUE
  if (currentRadius > 0) {
    display.fillCircle(disp.circleX, (disp.circleY - 1), currentRadius, fgColor);
  }

  // DRAW OUTER CIRCLES
  display.drawCircle(disp.circleX, (disp.circleY - 1), 7, fgColor);

  // DISPLAY MIDI VALUE
  // CENTERED BELOW THE CIRCLES
  String valueStr = String(displayValue);

  // CALCULATE TEXT WIDTH TO CENTRE TEXT
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  display.getTextBounds(valueStr, 0, 0, &x1, &y1, &textWidth, &textHeight);

  // CENTRE TEXT HORIZONTALLY
  int textX = disp.circleX - (textWidth / 2);

  display.setFont(NULL);
  display.setTextColor(fgColor);
  display.setCursor(textX, (disp.textY - 1));
  display.print(valueStr);
}

// *********************** //
// DRAW ASSIGN_SCREEN
// *********************** //
void drawAssignScreen() {
  display.clearDisplay();

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;
  int txtColor = displayInverted ? 1 : 0;

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(6, 11);
  display.print("MIDI assign");

  // DISPLAY CURRENTLY SELECTED POTENTIOMETER WITH SELECTION BOX
  display.setCursor(77, 11);
  display.print("Pot ");

  // Draw selection box around pot number if in pot select mode
  if (assignEditMode == ASSIGN_POT_SELECT && !assignEditingMode) {
    // Flash the selection box
    if ((millis() / 300) % 2 == 0) {
      display.fillRoundRect(90, 5, 7, 10, 1, txtColor);
      display.setTextColor(bgColor);
      display.setCursor(92, 11);
      display.print(selectedPot + 1);
      display.setTextColor(txtColor);
    } else {
      display.drawRoundRect(90, 5, 7, 10, 1, txtColor);
      display.setCursor(92, 11);
      display.print(selectedPot + 1);
    }
  } else {
    display.setCursor(92, 11);
    display.print(selectedPot + 1);
  }

  // CALCULATE SCROLL OFFSET
  if (selectedMessage < scrollOffset) {
    scrollOffset = selectedMessage;
  } else if (selectedMessage >= scrollOffset + MAX_VISIBLE_MESSAGES) {
    scrollOffset = selectedMessage - MAX_VISIBLE_MESSAGES + 1;
  }

  // DISPLAY MIDI MESSAGES UP TO MAX_VISIBLE_MESSAGES
  for (int i = 0; i < MAX_VISIBLE_MESSAGES; i++) {
    int messageIndex = i + scrollOffset;
    if (messageIndex >= messageCount[selectedPot]) {
      break;
    }

    int yPos = 26 + (i * 10);

    if (yPos <= 56) {
      // Draw the bracket indicator for all messages
      display.drawRoundRect(3, yPos - 3, 122, 9, 1, txtColor);
      
      // Show selection indicator for message selection (filled bracket)
      if (assignEditMode == ASSIGN_MESSAGE_SELECT && !assignEditingMode && messageIndex == selectedMessage) {
        if ((millis() / 300) % 2 == 0) {
          display.fillRoundRect(4, yPos - 2, 120, 7, 1, txtColor);
          display.setTextColor(bgColor);
        } else {
          display.setTextColor(txtColor);
        }
      } else {
        display.setTextColor(txtColor);
      }

      display.setCursor(6, yPos);

      // Display message parameters in the new format: "Ch 1 | CC 7 | 0-127 | NOR"
      display.print("Ch ");
      display.print(potMessages[selectedPot][messageIndex].channel + 1);
      display.print(" | CC ");
      display.print(potMessages[selectedPot][messageIndex].cc);
      display.print(" | ");
      display.print(potMessages[selectedPot][messageIndex].minValue);
      display.print("-");
      display.print(potMessages[selectedPot][messageIndex].maxValue);
      display.print(" | ");
      
      // Show direction (NOR or INV)
      if (potMessages[selectedPot][messageIndex].inverted) {
        display.print("INV");
      } else {
        display.print("NOR");
      }

      // Show edit indicator for selected message when in edit mode
      if (messageIndex == selectedMessage && assignEditingMode) {
        // Determine what's being edited based on assignEditMode
        int editX = 0;
        int editWidth = 0;
        int editStart = 0;
        int editEnd = 0;

        switch (assignEditMode) {
          case ASSIGN_EDIT_CHANNEL:
            // "Ch X" - position after "Ch " (3 chars) + number (1-2 chars)
            editStart = 3;
            editEnd = 7;
            editX = 6 + (editStart * 6); // Approximate character width
            editWidth = (editEnd - editStart) * 6;
            break;
          case ASSIGN_EDIT_CC:
            // "| CC XXX" - find the CC position
            editStart = 12;
            editEnd = 18;
            editX = 6 + (editStart * 6);
            editWidth = (editEnd - editStart) * 6;
            break;
          case ASSIGN_EDIT_MIN:
            // Min value position (after "| ")
            editStart = 20;
            editEnd = 23;
            editX = 6 + (editStart * 6);
            editWidth = (editEnd - editStart) * 6;
            break;
          case ASSIGN_EDIT_MAX:
            // Max value position (after "-")
            editStart = 24;
            editEnd = 27;
            editX = 6 + (editStart * 6);
            editWidth = (editEnd - editStart) * 6;
            break;
          case ASSIGN_EDIT_INVERT:
            // Direction (NOR/INV) position
            editStart = 32;
            editEnd = 38;
            editX = 6 + (editStart * 6);
            editWidth = (editEnd - editStart) * 6;
            break;
        }

        if (editX > 0 && (millis() / 200) % 2 == 0) {
          display.fillRect(editX, yPos - 2, editWidth, 7, txtColor);
          display.setTextColor(bgColor);
          // Redraw the entire line with edited value highlighted
          display.setCursor(6, yPos);
          display.print("Ch ");
          display.print(potMessages[selectedPot][selectedMessage].channel + 1);
          display.print(" | CC ");
          display.print(potMessages[selectedPot][selectedMessage].cc);
          display.print(" | ");
          display.print(potMessages[selectedPot][selectedMessage].minValue);
          display.print("-");
          display.print(potMessages[selectedPot][selectedMessage].maxValue);
          display.print(" | ");
          if (potMessages[selectedPot][selectedMessage].inverted) {
            display.print("INV");
          } else {
            display.print("NOR");
          }
          display.setTextColor(txtColor);
        }
      }
    }
  }

  // SHOW MIDI VALUE AND EDIT INFO AT BOTTOM
  display.setCursor(64, 56);
  display.print("Val ");
  display.print(currentMidiValue[selectedPot]);
  display.print("/127");

  // Show edit instructions based on mode
  display.setCursor(8, 56);
  if (assignEditingMode) {
    switch (assignEditMode) {
      case ASSIGN_EDIT_CHANNEL:
        display.print("Edit Ch:");
        display.print(potMessages[selectedPot][selectedMessage].channel + 1);
        break;
      case ASSIGN_EDIT_CC:
        display.print("Edit CC:");
        display.print(potMessages[selectedPot][selectedMessage].cc);
        break;
      case ASSIGN_EDIT_INVERT:
        display.print("Dir:");
        display.print(potMessages[selectedPot][selectedMessage].inverted ? "INV" : "NOR");
        break;
      case ASSIGN_EDIT_MIN:
        display.print("Min:");
        display.print(potMessages[selectedPot][selectedMessage].minValue);
        if ((millis() / 200) % 2 == 0) {
          display.fillRect(40, 52, 20, 9, txtColor);
          display.setTextColor(bgColor);
          display.setCursor(42, 56);
          display.print(potMessages[selectedPot][selectedMessage].minValue);
          display.setTextColor(txtColor);
        }
        break;
      case ASSIGN_EDIT_MAX:
        display.print("Max:");
        display.print(potMessages[selectedPot][selectedMessage].maxValue);
        if ((millis() / 200) % 2 == 0) {
          display.fillRect(40, 52, 20, 9, txtColor);
          display.setTextColor(bgColor);
          display.setCursor(42, 56);
          display.print(potMessages[selectedPot][selectedMessage].maxValue);
          display.setTextColor(txtColor);
        }
        break;
    }
  } else {
    display.print("ENT:Edit  ASN:Save");
  }

  // SCROLL BAR
  if (messageCount[selectedPot] > MAX_VISIBLE_MESSAGES) {
    display.drawLine(121, 20, 121, 59, txtColor);
    int scrollBarHeight = 59 - 21;
    int thumbHeight = max(5, scrollBarHeight * MAX_VISIBLE_MESSAGES / messageCount[selectedPot]);
    int thumbPosition = 21 + (scrollOffset * (scrollBarHeight - thumbHeight) / max(1, (messageCount[selectedPot] - MAX_VISIBLE_MESSAGES)));
    display.fillRoundRect(119, thumbPosition, 5, thumbHeight, 2, txtColor);
    display.fillRect(120, (thumbPosition + 1), 3, (thumbHeight - 2), bgColor);
  }

  // POT INDICATOR GRID
  display.drawBitmap(105, 8, image_potMatrixGrid_bits, 10, 4, txtColor);
  display.drawRoundRect(101, 5, 18, 10, 1, txtColor);
  display.drawCircle(potCirclePositions[selectedPot].x + 3, potCirclePositions[selectedPot].y, 1, txtColor);

  display.display();
}

// *********************** //
// DRAW NAVIGATION MENU    //
// *********************** //
void drawMenuScreen() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Menu");

  // MENU OPTIONS
  display.setCursor(5, 25);
  display.println("- CC assign");
  display.setCursor(5, 35);
  display.println("- States");
  display.setCursor(5, 45);
  display.println("- Settings");

  // Draw the bitmap indicator based on selection
  int indicatorY = 21 + (selectedMenuItem * 10);
  display.drawBitmap(55, indicatorY, image_ctrlMessageIndicator_bits, 5, 5, txtColor);

  display.display();
}

// *********************** //
// DRAW STATES MENU        //
// *********************** //
void drawStatesMenuScreen() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  // Title
  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("States");

  // Current State Indicator in top right
  display.setCursor(67, 11);
  display.print("Current State:");

  // Display the current active state
  display.setCursor(115, 11);
  if (currentStateSlot >= 0 && currentStateSlot < 8) {
    display.print("#");
    display.print(currentStateSlot + 1);
  } else {
    display.print("--");  // Display -- if no state is loaded
  }

  // MENU OPTIONS
  display.setCursor(5, 25);
  display.println("- Save state");
  display.setCursor(5, 35);
  display.println("- Load state");
  display.setCursor(5, 45);
  display.println("- Clear state");

  // Draw the bitmap indicator based on selection
  int indicatorY = 21 + (selectedMenuItem * 10);
  display.drawBitmap(55, indicatorY, image_ctrlMessageIndicator_bits, 5, 5, txtColor);

  display.display();
}

// *********************** //
// DRAW SAVE STATES MENU   //
// *********************** //
void drawSaveStates() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Save State");

  // STATES OPTIONS - Two columns of 4
  // Column 1 (States #1-4)
  display.setCursor(5, 25);
  display.println("- State #1");
  display.setCursor(5, 35);
  display.println("- State #2");
  display.setCursor(5, 45);
  display.println("- State #3");
  display.setCursor(5, 55);
  display.println("- State #4");

  // Column 2 (States #5-8)
  display.setCursor(69, 25);
  display.println("- State #5");
  display.setCursor(69, 35);
  display.println("- State #6");
  display.setCursor(69, 45);
  display.println("- State #7");
  display.setCursor(69, 55);
  display.println("- State #8");

  // Draw selection indicator (highlighted bar)
  int row = selectedMenuItem % 4;  // 0-3 for rows
  int col = selectedMenuItem / 4;  // 0 for first column, 1 for second column

  int indicatorX = (col == 0) ? 3 : 67;
  int indicatorY = 21 + (row * 10);
  int indicatorWidth = 58;
  int indicatorHeight = 9;

  // Draw inverted selection bar (uses opposite colors for contrast)
  display.fillRoundRect(indicatorX, indicatorY - 4, indicatorWidth, indicatorHeight, 1, txtColor);

  // Redraw the selected text in background color on the bar
  display.setTextColor(bgColor);
  display.setCursor(indicatorX + 2, indicatorY + 2);
  if (col == 0) {
    display.print("State #");
    display.print(row + 1);
  } else {
    display.print("State #");
    display.print(row + 5);
  }

  // Reset text color for other elements (though we're done)
  display.setTextColor(txtColor);

  display.display();
}

// *********************** //
// DRAW LOAD STATES MENU   //
// *********************** //
void drawLoadStates() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Load State");

  // STATES OPTIONS - Two columns of 4
  // Column 1 (States #1-4)
  display.setCursor(5, 25);
  display.println("- State #1");
  display.setCursor(5, 35);
  display.println("- State #2");
  display.setCursor(5, 45);
  display.println("- State #3");
  display.setCursor(5, 55);
  display.println("- State #4");

  // Column 2 (States #5-8)
  display.setCursor(69, 25);
  display.println("- State #5");
  display.setCursor(69, 35);
  display.println("- State #6");
  display.setCursor(69, 45);
  display.println("- State #7");
  display.setCursor(69, 55);
  display.println("- State #8");

  // Draw selection indicator (highlighted bar)
  int row = selectedMenuItem % 4;  // 0-3 for rows
  int col = selectedMenuItem / 4;  // 0 for first column, 1 for second column

  int indicatorX = (col == 0) ? 3 : 67;
  int indicatorY = 21 + (row * 10);
  int indicatorWidth = 58;
  int indicatorHeight = 9;

  // Draw inverted selection bar (uses opposite colors for contrast)
  display.fillRoundRect(indicatorX, indicatorY - 4, indicatorWidth, indicatorHeight, 1, txtColor);

  // Redraw the selected text in background color on the bar
  display.setTextColor(bgColor);
  display.setCursor(indicatorX + 2, indicatorY + 2);
  if (col == 0) {
    display.print("State #");
    display.print(row + 1);
  } else {
    display.print("State #");
    display.print(row + 5);
  }

  // Reset text color (though we're done drawing)
  display.setTextColor(txtColor);

  display.display();
}

// *********************** //
// DRAW CLEAR STATES MENU   //
// *********************** //
void drawClearStates() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Clear State");

  // STATES OPTIONS - Two columns of 4
  // Column 1 (States #1-4)
  display.setCursor(5, 25);
  display.println("- State #1");
  display.setCursor(5, 35);
  display.println("- State #2");
  display.setCursor(5, 45);
  display.println("- State #3");
  display.setCursor(5, 55);
  display.println("- State #4");

  // Column 2 (States #5-8)
  display.setCursor(69, 25);
  display.println("- State #5");
  display.setCursor(69, 35);
  display.println("- State #6");
  display.setCursor(69, 45);
  display.println("- State #7");
  display.setCursor(69, 55);
  display.println("- State #8");

  // Draw selection indicator (highlighted bar)
  int row = selectedMenuItem % 4;  // 0-3 for rows
  int col = selectedMenuItem / 4;  // 0 for first column, 1 for second column

  int indicatorX = (col == 0) ? 3 : 67;
  int indicatorY = 21 + (row * 10);
  int indicatorWidth = 58;
  int indicatorHeight = 9;

  // Draw inverted selection bar (uses opposite colors for contrast)
  display.fillRoundRect(indicatorX, indicatorY - 4, indicatorWidth, indicatorHeight, 1, txtColor);

  // Redraw the selected text in background color on the bar
  display.setTextColor(bgColor);
  display.setCursor(indicatorX + 2, indicatorY + 2);
  if (col == 0) {
    display.print("State #");
    display.print(row + 1);
  } else {
    display.print("State #");
    display.print(row + 5);
  }

  // Reset text color (though we're done drawing)
  display.setTextColor(txtColor);

  display.display();
}

// *********************** //
// DRAW CONFIRM SAVE POPUP //
// *********************** //
void drawConfirmSavePopup() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal
  int boxColor = txtColor;

  display.setFont(&Picopixel);

  // Background
  display.fillRect(0, 0, 128, 64, bgColor);

  // Popup border
  display.drawRoundRect(10, 12, 108, 40, 4, boxColor);

  // Warning text
  display.setTextColor(txtColor);
  display.setTextSize(1);
  display.setCursor(30, 22);
  display.print("Overwrite State #");
  display.print(pendingSaveSlot + 1);
  display.print("?");

  // Y/N with box around selected option
  display.setCursor(45, 40);
  display.print("Y");
  display.setCursor(75, 40);
  display.print("N");

  // Determine box position and width based on selection
  int boxX;
  int boxWidth;
  int boxY = 34;
  int boxHeight = 9;

  if (confirmSelection) {
    boxX = 43;     // Box over Y
    boxWidth = 7;  // Width 7 for Y
  } else {
    boxX = 73;     // Box over N
    boxWidth = 8;  // Width 8 for N
  }

  // Draw the box around selected Y or N with flashing effect
  if ((millis() / 300) % 2 == 0) {
    // Draw filled box when flashing
    display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw the selected letter in background color on the box
    display.setTextColor(bgColor);
    if (confirmSelection) {
      display.setCursor(45, 40);
      display.print("Y");
      display.setTextColor(txtColor);
      display.setCursor(75, 40);
      display.print("N");
    } else {
      display.setCursor(75, 40);
      display.print("N");
      display.setTextColor(txtColor);
      display.setCursor(45, 40);
      display.print("Y");
    }
  } else {
    // Draw outline box when not flashing
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw both letters in text color
    display.setTextColor(txtColor);
    display.setCursor(45, 40);
    display.print("Y");
    display.setCursor(75, 40);
    display.print("N");
  }

  // Instructions
  display.setTextColor(txtColor);
  display.setCursor(21, 60);
  display.setTextSize(1);
  display.print("ENT: Confirm  ASN: Cancel");

  display.display();
}

// *********************** //
// DRAW CONFIRM ASSIGN SAVE POPUP //
// *********************** //
void drawConfirmAssignSavePopup() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal
  int boxColor = txtColor;

  display.setFont(&Picopixel);

  // Background
  display.fillRect(0, 0, 128, 64, bgColor);

  // Popup border
  display.drawRoundRect(10, 12, 108, 40, 4, boxColor);

  // Determine which slot we're saving to
  int saveSlot = (currentStateSlot >= 0) ? currentStateSlot : 0;

  // Warning text
  display.setTextColor(txtColor);
  display.setTextSize(1);
  display.setCursor(38, 22);
  display.print("Apply Changes?");

  // Y/N with box around selected option
  display.setCursor(45, 47);
  display.print("Y");
  display.setCursor(75, 47);
  display.print("N");

  // Determine box position and width based on selection
  int boxX;
  int boxWidth;
  int boxY = 41;
  int boxHeight = 9;

  if (confirmAssignSave) {
    boxX = 43;     // Box over Y
    boxWidth = 7;  // Width 7 for Y
  } else {
    boxX = 73;     // Box over N
    boxWidth = 8;  // Width 8 for N
  }

  // Draw the box around selected Y or N with flashing effect
  if ((millis() / 300) % 2 == 0) {
    // Draw filled box when flashing
    display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw the selected letter in background color on the box
    display.setTextColor(bgColor);
    if (confirmAssignSave) {
      display.setCursor(45, 47);
      display.print("Y");
      display.setTextColor(txtColor);
      display.setCursor(75, 47);
      display.print("N");
    } else {
      display.setCursor(75, 47);
      display.print("N");
      display.setTextColor(txtColor);
      display.setCursor(45, 47);
      display.print("Y");
    }
  } else {
    // Draw outline box when not flashing
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw both letters in text color
    display.setTextColor(txtColor);
    display.setCursor(45, 47);
    display.print("Y");
    display.setCursor(75, 47);
    display.print("N");
  }

  // Instructions
  display.setTextColor(txtColor);
  display.setCursor(21, 60);
  display.setTextSize(1);
  display.print("ENT: Confirm  ASN: Cancel");

  display.display();
}

// *********************** //
// DRAW SETTINGS MENU      //
// *********************** //
void drawSettingsMenuScreen() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Settings");

  // MENU OPTIONS - Now 3 items
  display.setCursor(5, 25);
  display.println("- Display");
  display.setCursor(5, 35);
  display.println("- About");
  display.setCursor(5, 45);
  display.println("- Factory Reset");

  // Draw the bitmap indicator based on selection
  int indicatorY = 21 + (selectedMenuItem * 10);
  display.drawBitmap(65, indicatorY, image_ctrlMessageIndicator_bits, 5, 5, txtColor);

  display.display();
}
// *********************** //
// DRAW DISPLAY SETTINGS   //
// *********************** //
void drawDisplaySettings() {

  display.clearDisplay();  // CLEAR
  display.setFont(&Picopixel);

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal
  int boxColor = txtColor;                 // Box border uses text color

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);

  display.setTextColor(txtColor);
  display.setCursor(6, 11);
  display.println("Display settings");

  // MENU OPTIONS
  display.setCursor(5, 25);
  display.println("- Invert Display");

  // Display Y and N
  display.setCursor(79, 25);
  display.print("Y");
  display.setCursor(91, 25);
  display.print("N");

  display.setCursor(85, 25);
  display.print("/");

  // Determine which box to draw and where
  int boxX;
  int boxY = 19;
  int boxWidth;
  int boxHeight = 9;

  if (displayInverted) {
    boxX = 77;  // Box over Y
    boxWidth = 7;
  } else {
    boxX = 89;  // Box over N
    boxWidth = 8;
  }

  // Draw the box around selected Y or N
  if (editingDisplaySetting) {
    // Flash the box when in edit mode
    if (millis() - editFlashTimer > 300) {  // Flash every 300ms
      flashState = !flashState;
      editFlashTimer = millis();
    }

    if (flashState) {
      // Draw filled box when flashing
      display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
      // Draw the selected letter in contrasting color
      display.setTextColor(bgColor);  // Switch to background color for the selected letter
      if (displayInverted) {
        display.setCursor(79, 25);
        display.print("Y");
        display.setTextColor(txtColor);
        display.setCursor(91, 25);
        display.print("N");
      } else {
        display.setCursor(91, 25);
        display.print("N");
        display.setTextColor(txtColor);
        display.setCursor(79, 25);
        display.print("Y");
      }
    } else {
      // Draw outline box
      display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
      // Draw both letters in text color
      display.setTextColor(txtColor);
      display.setCursor(79, 25);
      display.print("Y");
      display.setCursor(91, 25);
      display.print("N");
    }
  } else {
    // Normal mode - just draw the outline box around selected option
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw both letters in text color
    display.setTextColor(txtColor);
    display.setCursor(79, 25);
    display.print("Y");
    display.setCursor(91, 25);
    display.print("N");
  }

  // Reset text color for the bitmap indicator
  display.setTextColor(txtColor);

  // Draw the bitmap indicator based on selection
  int indicatorY = 21 + (selectedMenuItem * 10);
  display.drawBitmap(100, indicatorY, image_ctrlMessageIndicator_bits, 5, 5, txtColor);

  display.display();
}

// *********************** //
// DRAW ABOUT SCREEN       //
// *********************** //
void drawAboutScreen() {
  display.clearDisplay();

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal

  display.setFont(&Picopixel);

  // DRAW UI
  display.fillRect(0, 0, 128, 64, bgColor);
  display.drawRoundRect(1, 1, 126, 62, 3, txtColor);
  display.drawRoundRect(1, 2, 126, 60, 4, txtColor);

  display.setTextColor(txtColor);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(46, 14);
  display.print("MC8P");

  display.setTextSize(1);
  display.setCursor(33, 35);
  display.print("8 pot open-source");

  display.setCursor(41, 25);
  display.print("firmware v2.0");

  display.setCursor(34, 57);
  display.print(".axs instruments");

  display.setCursor(39, 43);
  display.print("MIDI controller");

  display.display();
}

// *********************** //
// DRAW CONFIRM RESET POPUP //
// *********************** //
void drawConfirmResetPopup() {

  display.clearDisplay();  // CLEAR

  // Set colors based on the inversion variable
  int bgColor = displayInverted ? 0 : 1;   // Black if inverted, White if normal
  int txtColor = displayInverted ? 1 : 0;  // White if inverted, Black if normal
  int boxColor = txtColor;

  display.setFont(&Picopixel);

  // Background
  display.fillRect(0, 0, 128, 64, bgColor);

  // Popup border
  display.drawRoundRect(10, 12, 108, 40, 4, boxColor);

  // Warning text
  display.setTextColor(txtColor);
  display.setTextSize(1);
  display.setCursor(35, 22);
  display.print("Factory Reset?");
  display.setCursor(20, 32);
  display.print("All settings will be");
  display.setCursor(20, 42);
  display.print("lost!");

  // Y/N with box around selected option
  display.setCursor(45, 45);
  display.print("Y");
  display.setCursor(75, 45);
  display.print("N");

  // Determine box position and width based on selection
  int boxX;
  int boxWidth;
  int boxY = 39;
  int boxHeight = 9;

  if (confirmSelection) {
    boxX = 43;     // Box over Y
    boxWidth = 7;  // Width 7 for Y
  } else {
    boxX = 73;     // Box over N
    boxWidth = 8;  // Width 8 for N
  }

  // Draw the box around selected Y or N with flashing effect
  if ((millis() / 300) % 2 == 0) {
    // Draw filled box when flashing
    display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw the selected letter in background color on the box
    display.setTextColor(bgColor);
    if (confirmSelection) {
      display.setCursor(45, 45);
      display.print("Y");
      display.setTextColor(txtColor);
      display.setCursor(75, 45);
      display.print("N");
    } else {
      display.setCursor(75, 45);
      display.print("N");
      display.setTextColor(txtColor);
      display.setCursor(45, 45);
      display.print("Y");
    }
  } else {
    // Draw outline box when not flashing
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 1, boxColor);
    // Draw both letters in text color
    display.setTextColor(txtColor);
    display.setCursor(45, 45);
    display.print("Y");
    display.setCursor(75, 45);
    display.print("N");
  }

  display.display();
}
// *********************** //
// EEPROM FUNCTIONS
// *********************** //

// *********************** //
// FACTORY RESET FUNCTION
// *********************** //
void factoryReset() {
  Serial.println("=== FACTORY RESET INITIATED ===");

  // Reset all pots to default configuration
  for (int i = 0; i < N_POTS; i++) {
    messageCount[i] = 1;                            // 1 message per pot
    potMessages[i][0].channel = i;                  // Channel 1-8 (0-7 in code)
    potMessages[i][0].cc = 7;                       // CC 7
    potMessages[i][0].inverted = false;             // Normal direction
    potMessages[i][0].minValue = 0;                 // Min value 0
    potMessages[i][0].maxValue = 127;               // Max value 127
    potMessages[i][0].value = currentMidiValue[i];  // Current pot position

    // Clear any additional messages
    for (int j = 1; j < MAX_MESSAGES_PER_POT; j++) {
      potMessages[i][j].channel = 0;
      potMessages[i][j].cc = 0;
      potMessages[i][j].inverted = false;
      potMessages[i][j].minValue = 0;
      potMessages[i][j].maxValue = 127;
      potMessages[i][j].value = 0;
    }
  }

  // Reset display settings
  displayInverted = false;
  if (displayInverted) {
    display.invertDisplay(false);
  }

  // Reset current state slot
  currentStateSlot = -1;

  // Save factory reset settings to EEPROM
  saveGlobalSettingsToEEPROM();
  saveStateToSlot(0);  // Save to slot 0

  // Clear all other state slots
  for (int slot = 1; slot < NUM_STATE_SLOTS; slot++) {
    clearStateFromSlot(slot);
  }

  // Reset navigation states
  selectedPot = 0;
  selectedMessage = 0;
  scrollOffset = 0;
  assignEditMode = ASSIGN_POT_SELECT;
  assignEditingMode = false;

  Serial.println("Factory reset completed!");
  Serial.println("All pots reset to: Channel 1-8, CC7, Range 0-127, Normal direction");
  Serial.println("=== FACTORY RESET COMPLETE ===");

  // Visual feedback
  for (int i = 0; i < 3; i++) {
    display.invertDisplay(true);
    delay(100);
    display.invertDisplay(false);
    delay(100);
  }
}

// *********************** //
// SAVE GLOBAL SETTINGS TO EEPROM
// *********************** //
void saveGlobalSettingsToEEPROM() {
  GlobalSettings settings;
  settings.signature = EEPROM_SIGNATURE;
  settings.version = GLOBAL_SETTINGS_VERSION;
  settings.displayInverted = displayInverted;
  settings.lastLoadedState = currentStateSlot;

  EEPROM.put(GLOBAL_EEPROM_ADDR, settings);

  Serial.println("Global settings saved to EEPROM");
  Serial.print("Display invert saved as: ");
  Serial.println(displayInverted ? "YES" : "NO");
  Serial.print("Last loaded state saved as: ");
  Serial.println(currentStateSlot);
}

// *********************** //
// LOAD GLOBAL SETTINGS FROM EEPROM
// *********************** //
void loadGlobalSettingsFromEEPROM() {
  GlobalSettings settings;
  EEPROM.get(GLOBAL_EEPROM_ADDR, settings);

  if (settings.signature == EEPROM_SIGNATURE && settings.version == GLOBAL_SETTINGS_VERSION) {
    displayInverted = settings.displayInverted;
    currentStateSlot = settings.lastLoadedState;

    // Apply display setting
    if (displayInverted) {
      display.invertDisplay(true);
    } else {
      display.invertDisplay(false);
    }

    Serial.println("Global settings loaded from EEPROM");
    Serial.print("Display invert loaded as: ");
    Serial.println(displayInverted ? "YES" : "NO");
    Serial.print("Last loaded state loaded as: ");
    Serial.println(currentStateSlot);
  } else {
    Serial.println("No valid global settings found, using defaults");
    displayInverted = false;
    currentStateSlot = -1;
  }
}

// *********************** //
// SAVE SETTINGS TO EEPROM (for current state)
// *********************** //
void saveSettingsToEEPROM() {
  // Only save to the currently loaded state slot
  if (currentStateSlot >= 0 && currentStateSlot < NUM_STATE_SLOTS) {
    saveStateToSlot(currentStateSlot);
    Serial.print("Settings saved to current state slot ");
    Serial.println(currentStateSlot + 1);
  } else {
    // If no state is loaded, save to slot 0
    Serial.println("No state loaded, saving to slot 0");
    saveStateToSlot(0);
    currentStateSlot = 0;
    saveGlobalSettingsToEEPROM();
  }
}

// *********************** //
// LOAD SETTINGS FROM EEPROM (load MIDI config)
// *********************** //
void loadSettingsFromEEPROM() {
  // First load global settings
  loadGlobalSettingsFromEEPROM();

  // Then try to load the last used state if one was saved
  if (currentStateSlot >= 0 && currentStateSlot < NUM_STATE_SLOTS) {
    loadStateFromSlot(currentStateSlot);
  } else {
    // Load default state (slot 0) if no last loaded state
    loadStateFromSlot(0);
  }
}

// *********************** //
// SAVE STATE TO SLOT
// *********************** //
void saveStateToSlot(int slot) {
  if (slot < 0 || slot >= NUM_STATE_SLOTS) return;

  SavedSettings currentState;
  currentState.signature = EEPROM_SIGNATURE;
  currentState.version = SETTINGS_VERSION;

  for (int i = 0; i < N_POTS; i++) {
    currentState.messageCount[i] = messageCount[i];
    for (int j = 0; j < MAX_MESSAGES_PER_POT; j++) {
      currentState.potMessages[i][j].channel = potMessages[i][j].channel;
      currentState.potMessages[i][j].cc = potMessages[i][j].cc;
      currentState.potMessages[i][j].inverted = potMessages[i][j].inverted;
      currentState.potMessages[i][j].minValue = potMessages[i][j].minValue;
      currentState.potMessages[i][j].maxValue = potMessages[i][j].maxValue;
      currentState.potMessages[i][j].value = potMessages[i][j].value;
    }
  }

  int slotAddress = EEPROM_ADDR + (slot * sizeof(SavedSettings));
  EEPROM.put(slotAddress, currentState);
  saveGlobalSettingsToEEPROM();

  Serial.print("State saved to slot ");
  Serial.println(slot + 1);
}

// *********************** //
// LOAD STATE FROM SLOT
// *********************** //
void loadStateFromSlot(int slot) {
  if (slot < 0 || slot >= NUM_STATE_SLOTS) return;

  int slotAddress = EEPROM_ADDR + (slot * sizeof(SavedSettings));
  SavedSettings loadedState;
  EEPROM.get(slotAddress, loadedState);

  if (loadedState.signature == EEPROM_SIGNATURE) {
    for (int i = 0; i < N_POTS; i++) {
      messageCount[i] = loadedState.messageCount[i];
      for (int j = 0; j < MAX_MESSAGES_PER_POT; j++) {
        potMessages[i][j].channel = loadedState.potMessages[i][j].channel;
        potMessages[i][j].cc = loadedState.potMessages[i][j].cc;
        potMessages[i][j].inverted = loadedState.potMessages[i][j].inverted;
        potMessages[i][j].minValue = loadedState.potMessages[i][j].minValue;
        potMessages[i][j].maxValue = loadedState.potMessages[i][j].maxValue;
        potMessages[i][j].value = loadedState.potMessages[i][j].value;
      }
    }

    currentStateSlot = slot;
    saveGlobalSettingsToEEPROM();

    Serial.print("State loaded from slot ");
    Serial.println(slot + 1);
  } else {
    Serial.print("No valid state found in slot ");
    Serial.println(slot + 1);
  }
}

// *********************** //
// CLEAR STATE FROM SLOT
// *********************** //
void clearStateFromSlot(int slot) {
  if (slot < 0 || slot >= NUM_STATE_SLOTS) return;

  // Calculate EEPROM address for this slot
  int slotAddress = EEPROM_ADDR + (slot * sizeof(SavedSettings));

  // Create empty state with invalid signature
  SavedSettings emptyState;
  emptyState.signature = 0;  // Invalid signature
  emptyState.version = SETTINGS_VERSION;

  // Set default MIDI values
  for (int i = 0; i < N_POTS; i++) {
    emptyState.messageCount[i] = 1;  // Default to 1 message
    for (int j = 0; j < MAX_MESSAGES_PER_POT; j++) {
      emptyState.potMessages[i][j].channel = 0;
      emptyState.potMessages[i][j].cc = 0;
      emptyState.potMessages[i][j].value = 0;
    }
  }

  // Write to EEPROM
  EEPROM.put(slotAddress, emptyState);

  // If we cleared the currently loaded state, reset currentStateSlot
  if (currentStateSlot == slot) {
    currentStateSlot = -1;
    saveGlobalSettingsToEEPROM();
  }

  Serial.print("State cleared from slot ");
  Serial.println(slot + 1);
}

// *********************** //
// INITIALISE
// *********************** //
void initController() {

  // INIT BUTTONS
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
  }

  // INIT OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();  // CLEAR DISPLAY

  // INIT DISPLAY
  display.fillRect(0, 0, 128, 64, 1);          // WHITE BACKGROUND
  display.drawRoundRect(1, 1, 126, 62, 3, 0);  // OUTER BOX
  display.drawRoundRect(17, 38, 94, 8, 3, 0);  // LOADING BAR BORDER
  display.fillRoundRect(19, 40, 4, 4, 1, 0);   // LOADING BAR INNER

  // TEXT
  // NAME
  display.setTextColor(0);
  display.setTextSize(3);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(29, 26);
  display.print("M C 8 P");
  // FIRMWARE VERSION
  display.setTextSize(1);
  display.setCursor(58, 55);
  display.print("v");
  display.print(FIRMWARE_VERSION);

  // DISPLAY
  display.display();

  // INIT POTS
  for (int i = 0; i < N_POTS; i++) {
    responsivePot[i].setAnalogResolution(1023);
  }

  // INITIALIZE TEMP OVERRIDE VALUES TO 0
  for (int i = 0; i < N_POTS; i++) {
    originalMidiValues[i] = 0;
    tempMidiValues[i] = 0;
    postOverridePotPositions[i] = 0;
    catchUpStartValue[i] = 0;
    catchUpStartPotPos[i] = 0;
    catchUpActive[i] = false;  // Make sure catch-up starts as disabled
  }

  // LOADING BAR UPDATE
  display.fillRoundRect(19, 40, 30, 4, 1, 0);
  display.display();  // REFRESH
  delay(250);         // SMALL DELAY

  // Load global settings first
  Serial.println("=== BOOTUP DEBUG ===");
  Serial.println("Loading global settings...");
  loadGlobalSettingsFromEEPROM();

  // Then load settings from EEPROM (which will load the appropriate state)
  Serial.println("Loading MIDI settings...");
  loadSettingsFromEEPROM();

  // DEBUG: Print loaded MIDI values for each pot
  Serial.println("=== LOADED MIDI VALUES ===");
  for (int i = 0; i < N_POTS; i++) {
    Serial.print("Pot ");
    Serial.print(i);
    Serial.print(" (Pin A");
    Serial.print(POT_PIN[i] - A0);
    Serial.print("): ");
    Serial.print("messageCount=");
    Serial.print(messageCount[i]);
    if (messageCount[i] > 0) {
      Serial.print(", channel=");
      Serial.print(potMessages[i][0].channel + 1);
      Serial.print(", cc=");
      Serial.print(potMessages[i][0].cc);
      Serial.print(", inverted=");
      Serial.print(potMessages[i][0].inverted ? "YES" : "NO");
      Serial.print(", min=");
      Serial.print(potMessages[i][0].minValue);
      Serial.print(", max=");
      Serial.print(potMessages[i][0].maxValue);
      Serial.print(", value=");
      Serial.println(potMessages[i][0].value);
    } else {
      Serial.println(" (no messages)");
    }
  }
  Serial.println("=== END DEBUG ===");

  // LOADING BAR UPDATE
  display.fillRoundRect(19, 40, 60, 4, 1, 0);  // LOADING BAR INNER
  display.display();                           // REFRESH
  delay(250);                                  // SMALL DELAY

  // INIT MIDI MESSAGES // WILL USE DEFAULT IF NONE ARE SAVED
  bool settingsLoaded = false;
  for (int i = 0; i < N_POTS; i++) {
    if (messageCount[i] > 0) {
      settingsLoaded = true;
      break;
    }
  }

  if (!settingsLoaded) {
    Serial.println("No valid settings loaded, using defaults");
    for (int i = 0; i < N_POTS; i++) {
      messageCount[i] = 1;  // Each pot starts with 1 message
      potMessages[i][0].channel = potChannels[i];
      potMessages[i][0].cc = potCCs[i];
      potMessages[i][0].inverted = false;
      potMessages[i][0].minValue = 0;
      potMessages[i][0].maxValue = 127;
      potMessages[i][0].value = 0;  // Initialize stored MIDI value to 0

      Serial.print("Default for Pot ");
      Serial.print(i);
      Serial.print(": value=");
      Serial.print(potMessages[i][0].value);
      Serial.print(", channel=");
      Serial.print(potMessages[i][0].channel);
      Serial.print(", cc=");
      Serial.print(potMessages[i][0].cc);
      Serial.print(", inverted=");
      Serial.print(potMessages[i][0].inverted ? "YES" : "NO");
      Serial.print(", min=");
      Serial.print(potMessages[i][0].minValue);
      Serial.print(", max=");
      Serial.println(potMessages[i][0].maxValue);
    }
  }

  // Read initial pot positions and update MIDI values
  Serial.println("=== READING INITIAL POT POSITIONS ===");
  for (int i = 0; i < N_POTS; i++) {
    // Read initial pot value
    potReading[i] = analogRead(POT_PIN[i]);
    responsivePot[i].update(potReading[i]);
    potState[i] = responsivePot[i].getValue();
    currentMidiValue[i] = map(potState[i], 0, 1023, 0, 127);

    Serial.print("Pot ");
    Serial.print(i);
    Serial.print(": raw=");
    Serial.print(potReading[i]);
    Serial.print(", filtered=");
    Serial.print(potState[i]);
    Serial.print(", MIDI=");
    Serial.print(currentMidiValue[i]);

    // Update stored MIDI values with current pot position
    if (messageCount[i] > 0) {
      // Apply range and direction to the initial value
      int finalValue = mapMidiValueWithParams(currentMidiValue[i],
                                              potMessages[i][0].minValue,
                                              potMessages[i][0].maxValue,
                                              potMessages[i][0].inverted);
      potMessages[i][0].value = finalValue;
      Serial.print(" (stored value set to ");
      Serial.print(finalValue);
      Serial.print(")");
    }
    Serial.println();
  }
  Serial.println("=== END POT READING ===");

  // LOADING BAR UPDATE
  display.fillRoundRect(19, 40, 90, 4, 1, 0);  // LOADING BAR INNER
  display.display();                           // REFRESH
  delay(1000);                                 // SMALL DELAY

  // Send initial MIDI values
  Serial.println("=== SENDING INITIAL MIDI VALUES ===");
  for (int i = 0; i < N_POTS; i++) {
    for (int j = 0; j < messageCount[i]; j++) {
      // Apply range and direction to the initial values
      int finalValue = mapMidiValueWithParams(potMessages[i][j].value,
                                              potMessages[i][j].minValue,
                                              potMessages[i][j].maxValue,
                                              potMessages[i][j].inverted);
      MIDI.sendControlChange(potMessages[i][j].cc, finalValue, potMessages[i][j].channel + 1);
      Serial.print("Sent MIDI: Pot ");
      Serial.print(i);
      Serial.print(", Message ");
      Serial.print(j);
      Serial.print(": Ch=");
      Serial.print(potMessages[i][j].channel + 1);
      Serial.print(", CC=");
      Serial.print(potMessages[i][j].cc);
      Serial.print(", Raw Value=");
      Serial.print(potMessages[i][j].value);
      Serial.print(", Final Value=");
      Serial.print(finalValue);
      Serial.print(", Range=[");
      Serial.print(potMessages[i][j].minValue);
      Serial.print("-");
      Serial.print(potMessages[i][j].maxValue);
      Serial.print("], Dir=");
      Serial.println(potMessages[i][j].inverted ? "INV" : "NOR");
    }
  }
  Serial.println("=== BOOTUP COMPLETE ===");
}

// Helper function to set default settings
void useDefaultSettings() {
  displayInverted = false;
  currentStateSlot = -1;  // No state loaded

  // Set default MIDI messages
  for (int i = 0; i < N_POTS; i++) {
    messageCount[i] = 1;
    potMessages[i][0].channel = potChannels[i];
    potMessages[i][0].cc = potCCs[i];
    potMessages[i][0].value = 0;
  }
}