
// ********************* //
// MC8P midiController   //
// firmware version 1.0  //
// ********************* //
// controller by         //
// instruments.axs       //
// ********************* //

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <MIDI.h>
#include <ResponsiveAnalogRead.h>
#include <Fonts/Picopixel.h>
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

// STRUCTURE TO HOLD MULTIPLE MIDI MESSAGES PER POT
struct MidiMessage {
  byte channel;
  byte cc;
  int value;
};

// BUTTON CONFIG
const int NUM_BUTTONS = 4;
const int BUTTON_PINS[NUM_BUTTONS] = { 5, 6, 7, 8 };
const char* BUTTON_NAMES[NUM_BUTTONS] = { "ASSIGN", "ENTER", "PREV", "NEXT" };
int buttonState[NUM_BUTTONS] = { 0 };
int lastButtonState[NUM_BUTTONS] = { 0 };
unsigned long lastDebounceTime[NUM_BUTTONS] = { 0 };
const unsigned long debounceDelay = 10;

// SCREEN STATE MANAGEMENT
enum ScreenState { MAIN_SCREEN,
                   ASSIGN_SCREEN };
ScreenState currentScreen = MAIN_SCREEN;

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

const int MAX_MESSAGES_PER_POT = 10;                    // Maximum number of messages per pot
MidiMessage potMessages[N_POTS][MAX_MESSAGES_PER_POT];  // Array to store messages
byte messageCount[N_POTS] = { 0 };                      // Track how many messages each pot has

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
int selectedPot = 0;  // Default to pot 0 (A0) initially
unsigned long lastPotSwitchTime = 0;
const unsigned long POT_SWITCH_DELAY = 200;  // ms delay between pot switches
bool editingChannel = true;                  // Tracks whether we're editing channel or CC
int valueIndicatorPos = 20;                  // X position of value indicator (starts under channel)

int selectedMessage = 0;  //

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
static const unsigned char PROGMEM image_mainScreenInnerLines_bits[] = { 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x10, 0x7f, 0xff, 0xff, 0xf9, 0x3f, 0xff, 0xff, 0xf2, 0x7f, 0xff, 0xff, 0xfc, 0x9f, 0xff, 0xff, 0xe0, 0x80, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00 };
// ASSIGN SCREEN
static const unsigned char PROGMEM image_ctrlMessageIndicator_bits[] = { 0x20, 0x60, 0xe0, 0x60, 0x20 };
static const unsigned char PROGMEM image_potMatrixGrid_bits[] = { 0x92, 0x40, 0x00, 0x00, 0x00, 0x00, 0x92, 0x40 };
static const unsigned char PROGMEM image_valueIndicator_bits[] = { 0xfc };
static const unsigned char PROGMEM image_assignScreenUITop_bits[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x00, 0x10, 0x00, 0x00, 0x80, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xa0, 0x08, 0x80, 0x2f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x10, 0x41, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xd2, 0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xd0, 0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x10, 0x41, 0xc0, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xa0, 0x08, 0x80, 0x2f, 0xff, 0xff, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x00, 0x10, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00 };

// EEPROM
// STRUCTURE TO SAVE TO EEPROM
struct SavedSettings {
  uint16_t signature;
  byte messageCount[N_POTS];
  MidiMessage potMessages[N_POTS][MAX_MESSAGES_PER_POT];
};

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
          // Send all messages for this pot
          for (int j = 0; j < messageCount[i]; j++) {
            potMessages[i][j].value = currentMidiValue[i];  // Update value for all messages
            MIDI.sendControlChange(potMessages[i][j].cc, potMessages[i][j].value, potMessages[i][j].channel + 1);
          }
        }

        Serial.print("A");
        Serial.print(POT_PIN[i] - A0);
        Serial.print(": ");
        Serial.print(currentMidiValue[i]);
        Serial.println("/127");
      }
      potPState[i] = currentMidiValue[i];
    }
  }

  // Draw the appropriate screen
  if (currentScreen == MAIN_SCREEN) {
    drawMainScreen();
  } else {
    drawAssignScreen();
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

            case 0:  // ASSIGN BUTTON SINGLE PRESS
              if (currentScreen == ASSIGN_SCREEN && buttonState[1] == LOW && buttonState[2] == LOW && buttonState[3] == LOW) {

                // SINGLE PRESS ASSIGN BUTTON ON ASSIGN_SCREEN
                // SWITCH BETWEEN EDITING MIDI CHANNEL NUM OR CC NUM
                editingChannel = !editingChannel;
                valueIndicatorPos = editingChannel ? 21 : 41;
                Serial.println(editingChannel ? "Now editing Channel" : "Now editing CC");
              }
              break;

            case 1:  // ENTER BUTTON SINGLE PRESS

              // RESET ADD/REMOVE STATE ON ENTER PRESS
              inAddRemoveOperation = false;  

              // NO MAIN FUNCTION

              break;

            case 2:  // PREV BUTTON SINGLE PRESS

              // *********************** //
              // ON ASSIGN SCREEN AND *ONLY* ASSIGN IS HELD
              // PRESSING THE NEXT/PREV BUTTONS CHANGES THE SELECTED POT THAT IS BEING EDITED
              // *********************** //
              prevButtonPressed = true;
              if (currentScreen == ASSIGN_SCREEN) {
                if (buttonState[0] == HIGH && !enterButtonHeld) {  
                  if (millis() - lastPotSwitchTime > POT_SWITCH_DELAY) {
                    selectedPot = (selectedPot - 1 + N_POTS) % N_POTS;
                    selectedMessage = 0;
                    lastPotSwitchTime = millis();
                    Serial.print("Selected Pot ");
                    Serial.println(selectedPot + 1);
                  }
                } 
                // *********************** //
                // ON ASSIGN SCREEN AND ASSIGN IS *NOT* HELD
                // PRESSING NEXT/PREV INCREMENTS EITHER THE MIDI CHANNEL OR CC ON THE SELECTED POT
                // *********************** //
                else if (!enterButtonHeld && !inAddRemoveOperation) {  
                  if (editingChannel) {
                    potMessages[selectedPot][selectedMessage].channel = (potMessages[selectedPot][selectedMessage].channel - 1 + 16) % 16;
                    Serial.print("Channel: ");
                    Serial.println(potMessages[selectedPot][selectedMessage].channel + 1);
                  } else {
                    potMessages[selectedPot][selectedMessage].cc = (potMessages[selectedPot][selectedMessage].cc - 1 + 128) % 128;
                    Serial.print("CC: ");
                    Serial.println(potMessages[selectedPot][selectedMessage].cc);
                  }
                }
              }
              break;

            case 3:  // NEXT BUTTON

            // *********************** //
            // ON ASSIGN SCREEN AND *ONLY* ASSIGN IS HELD
            // PRESSING THE NEXT/PREV BUTTONS CHANGES THE SELECTED POT THAT IS BEING EDITED
            // *********************** //
              nextButtonPressed = true;
              if (currentScreen == ASSIGN_SCREEN) {
                if (buttonState[0] == HIGH && !enterButtonHeld) {  
                  if (millis() - lastPotSwitchTime > POT_SWITCH_DELAY) {
                    selectedPot = (selectedPot + 1) % N_POTS;
                    selectedMessage = 0;
                    lastPotSwitchTime = millis();
                    Serial.print("Selected Pot ");
                    Serial.println(selectedPot + 1);
                  }
                } 
                // *********************** //
                // ON ASSIGN SCREEN AND ASSIGN IS *NOT* HELD
                // PRESSING NEXT/PREV INCREMENTS EITHER THE MIDI CHANNEL OR CC ON THE SELECTED POT
                // *********************** //
                else if (!enterButtonHeld && !inAddRemoveOperation) {  
                  if (editingChannel) {
                    potMessages[selectedPot][selectedMessage].channel = (potMessages[selectedPot][selectedMessage].channel + 1) % 16;
                    Serial.print("Channel: ");
                    Serial.println(potMessages[selectedPot][selectedMessage].channel + 1);
                  } else {
                    potMessages[selectedPot][selectedMessage].cc = (potMessages[selectedPot][selectedMessage].cc + 1) % 128;
                    Serial.print("CC: ");
                    Serial.println(potMessages[selectedPot][selectedMessage].cc);
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

            // ON ASSIGN SCREEN AND ASSIGN IS RELEASED
            // SWITCHES BETWEEN EDITING MIDI CHANNEL AND MIDI CC
              if (currentScreen == ASSIGN_SCREEN && assignButtonHeld) {
                if (millis() - assignButtonHoldStart < assignHoldDuration) {
                  editingChannel = !editingChannel;
                  valueIndicatorPos = editingChannel ? 21 : 41;
                  Serial.println(editingChannel ? "Now editing Channel" : "Now editing CC");
                }
              }
              assignButtonHeld = false;
              break;
            case 1:  // ENTER BUTTON RELEASE

              // RESET ADD/REMOVE STATE ON ENTER RELEASE
              inAddRemoveOperation = false;

              // NO MAIN FUNCTION

              break;
            case 2:  // PREV BUTTON RELEASE

              // ON ASSIGN SCREEN AND *ONLY* ENTER IS HELD
              // RELEASE OF NEXT/PREV NAVIGATES BETWEEN MIDI CONTROL MESSAGES
              if (prevButtonPressed && enterButtonHeld && currentScreen == ASSIGN_SCREEN && !inAddRemoveOperation) {
                selectedMessage = (selectedMessage - 1 + messageCount[selectedPot]) % messageCount[selectedPot];
                Serial.print("Selected Message ");
                Serial.println(selectedMessage);
              }
              prevButtonPressed = false;
              break;
            case 3:  // NEXT BUTTON RELEASE
            
              // ON ASSIGN SCREEN AND *ONLY* ENTER IS HELD
              // RELEASE OF NEXT/PREV NAVIGATES BETWEEN MIDI CONTROL MESSAGES
              if (nextButtonPressed && enterButtonHeld && currentScreen == ASSIGN_SCREEN && !inAddRemoveOperation) {
                selectedMessage = (selectedMessage + 1) % messageCount[selectedPot];
                Serial.print("Selected Message ");
                Serial.println(selectedMessage);
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
  // ASSIGN+ENTER ON ASSIGN SCREEN
  // HOLDING ASSIGN AND ENTER SAVES THE CURRENT SETTINGS TO EEPROM AFTER 2 SECONDS
  // *********************** //
  static bool savingInProgress = false; 
  static unsigned long saveStartTime = 0;

  if (currentScreen == ASSIGN_SCREEN) {
    // CHECK IF BOTH ASSIGN AND ENTER (BUTTON[0] & BUTTON[1]) ARE HELD
    if (buttonState[0] == HIGH && buttonState[1] == HIGH && buttonState[2] == LOW && buttonState[3] == LOW) {
      if (!savingInProgress) {
        // START TIMER
        savingInProgress = true;
        saveStartTime = millis();
        Serial.println("ASSIGN+ENTER held - timer started");
      }

      // VISUAL FEEDBACK AFTER 2 SECONDS
      if (millis() - saveStartTime >= 2000) {
        // Flash the display every 300ms while buttons are held
        if ((millis() / 150) % 2 == 0) {
          display.invertDisplay(true);
        } else {
          display.invertDisplay(false);
        }
      }
    } else if (savingInProgress) {
      // CHECK IF SAVING IS IN PROGRESS
      if (buttonState[0] == LOW || buttonState[1] == LOW) {
        // WERE BUTTONS HELD LONG ENOUGH?
        if (millis() - saveStartTime >= 2000) {
          // IF YES...
          // SAVE SETTINGS AND RETURN TO MAIN_SCREEN
          currentScreen = MAIN_SCREEN;
          saveSettingsToEEPROM();
          Serial.println("ASSIGN+ENTER released after 2s - settings saved, switching to MAIN_SCREEN");
        }
        // IF NO...
        // RESET SAVING STATE
        savingInProgress = false;
        display.invertDisplay(false);  // ENSURE DISPLAY IS NOT INVERTED
      }
    }
  }

  // *********************** //
  // SWITCH TO ASSIGN_SCREEN
  // *********************** //
  // HOLDING THE ASSIGN BUTTON ON THE MAIN_SCREEN FOR 2 SECONDS
  // SWITCHES THE SCREEN
  if (currentScreen == MAIN_SCREEN && buttonState[0] == HIGH && buttonState[1] == LOW && buttonState[2] == LOW && buttonState[3] == LOW) {
    if (!assignButtonHeld) {
      assignButtonHeld = true;
      assignButtonHoldStart = millis();
      Serial.println("ASSIGN button held - start timer for ASSIGN_SCREEN switch");
    } else if (millis() - assignButtonHoldStart >= assignHoldDuration) {
      if (digitalRead(BUTTON_PINS[0])) {  // Button is still HIGH
        // WAIT UNTIL BUTTON IS LOW BEFORE PROCEEDING WITH SWITCH
        // VISUAL FEEDBACK UNTIL BUTTON IS RELEASED
        display.invertDisplay(true);
        delay(150);
        display.invertDisplay(false);
        delay(150);
        return;
      }

      // SWITCH TO ASSIGN_SCREEN
      selectedPot = 0;
      editingChannel = true;
      valueIndicatorPos = 21;
      currentScreen = ASSIGN_SCREEN;
      assignButtonHeld = false;
      Serial.println("ASSIGN held for 1.5s and released - switching to ASSIGN_SCREEN");
    }
  } else if (buttonState[0] == LOW) {
    assignButtonHeld = false;
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

      // VISUAL FEEDBACK
      display.invertDisplay(true);
      delay(150);
      display.invertDisplay(false);
      delay(150);
      display.invertDisplay(true);
      delay(150);
      display.invertDisplay(false);

      // RESET
      resetToDefaultSettings();
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
// MIDI FUNCTIONS
// *********************** //
// *********************** //
// ADD MIDI CONTROL FNC
// *********************** //
void addMidiControl() {
  if (messageCount[selectedPot] < MAX_MESSAGES_PER_POT) {
    potMessages[selectedPot][messageCount[selectedPot]].channel = 0;  // MIDI CHANNEL 1
    potMessages[selectedPot][messageCount[selectedPot]].cc = 1;       // CC 1
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
// RESET MIDI CONTROL FNC
// *********************** //
void resetToDefaultSettings() {
  for (int i = 0; i < N_POTS; i++) {
    messageCount[i] = 1;                            // RESET TO 1 MESSAGE PER POT
    potMessages[i][0].channel = potChannels[i];     // DEFAULT CHANNEL
    potMessages[i][0].cc = potCCs[i];               // DEFAULT CC
    potMessages[i][0].value = currentMidiValue[i];  // KEEP CURRENT MIDI VALUE

    // CLEAR ANY ADDITIONAL MESSAGES
    for (int j = 1; j < MAX_MESSAGES_PER_POT; j++) {
      potMessages[i][j].channel = 0;
      potMessages[i][j].cc = 0;
      potMessages[i][j].value = 0;
    }
  }

  selectedMessage = 0;  // RESET TO FIRST MESSAGE
  Serial.println("Reset all pots to default settings");
}
// *********************** //
// DISPLAY FUNCTIONS
// *********************** //
// *********************** //
// DRAW MAIN_SCREEN
// *********************** //
void drawMainScreen() {
  display.clearDisplay();

  display.fillRect(0, 0, 128, 64, 1);  // WHITE BG

  // UI
  display.drawRoundRect(1, 1, 126, 62, 3, 0);                             // BORDER
  display.drawBitmap(2, 2, image_mainScreenInnerLines_bits, 124, 60, 0);  // UI INNER LINES

  // POT INFO
  display.setTextColor(0);
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

  // CALCULATE CURRENT RADIUS BASED ON MIDI VALUE
  int currentRadius = map(currentMidiValue[potIndex], 0, 127, 0, maxRadius);

  // DRAW EXPANDING INNER CIRCLE TO REPRESENT MIDI VALUE
  if (currentRadius > 0) {
    display.fillCircle(disp.circleX, (disp.circleY - 1), currentRadius, 0);
  }

  // DRAW OUTER CIRCLES
  display.drawCircle(disp.circleX, (disp.circleY - 1), 7, 0);

  // DISPLAY MIDI VALUE
  // CENTERED BELOW THE CIRCLES
  String valueStr = String(currentMidiValue[potIndex]);

  // CALCULATE TEXT WIDTH TO CENTRE TEXT
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  display.getTextBounds(valueStr, 0, 0, &x1, &y1, &textWidth, &textHeight);

  // CENTRE TEXT HORIZONTALLY
  int textX = disp.circleX - (textWidth / 2);

  display.setFont(NULL);
  display.setCursor(textX, (disp.textY - 1));
  display.print(valueStr);
}
// *********************** //
// DRAW ASSIGN_SCREEN
// *********************** //
void drawAssignScreen() {

  display.clearDisplay();

  // DRAW UI
  display.fillRect(0, 0, 128, 64, 1);                                 // WHITE BG
  display.drawRoundRect(1, 1, 126, 62, 3, 0);                         // BORDER
  display.drawBitmap(6, 4, image_assignScreenUITop_bits, 89, 13, 0);  // TOP UI

  display.setTextColor(0);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(8, 12);
  display.print("MIDI ASSIGN");

  // DISPLAY CURRENTLY SELECTED POTENTIOMETER
  display.setCursor(75, 12);
  display.print("Pot ");
  display.print(selectedPot + 1);  // DISPLAYS 1-8

  // CALCULATE SCROLL OFFSET TO KEEP CONTROL MESSAGES VISIBLE WHEN
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

    int yPos = 26 + (i * 10);  // CALCULATE Y POSITION FOR MESSAGE

    // ONLY DRAW WITHIN THE DEFINED AREA
    if (yPos <= 56) {
      display.setCursor(6, yPos);

      // DISPLAY MIDI CHANNEL
      display.print("- Ch ");
      display.print(potMessages[selectedPot][messageIndex].channel + 1);

      // DISPLAY CC OF POT
      display.print(" | CC ");
      display.print(potMessages[selectedPot][messageIndex].cc);

      // SELECTED CONTROL MESSAGE
      if (messageIndex == selectedMessage) {
        display.drawBitmap(60, yPos - 4, image_ctrlMessageIndicator_bits, 3, 5, 0);

        // CHANNEL/CC INDICATOR
        int indicatorY = yPos + 3;
        display.drawBitmap(valueIndicatorPos, indicatorY, image_valueIndicator_bits, 10, 1, 0);
      }
    }
  }

  // SHOW MIDI VALUE OF POTENTIOMETER
  display.setCursor(64, 26);
  display.print("   Value ");
  display.print(currentMidiValue[selectedPot]);
  display.print("/127");

  // SCROLL BAR - ONLY SHOW WHEN THERE ARE MORE THAN 4 MESSAGES ON THE POT
  if (messageCount[selectedPot] > MAX_VISIBLE_MESSAGES) {
    display.drawLine(121, 20, 121, 59, 0);  // SCROLL BAR LINE

    // CALCULATE SCROLL BAR THUMB SIZE
    int scrollBarHeight = 59 - 21;
    int thumbHeight = max(5, scrollBarHeight * MAX_VISIBLE_MESSAGES / messageCount[selectedPot]);
    int thumbPosition = 21 + (scrollOffset * (scrollBarHeight - thumbHeight) / max(1, (messageCount[selectedPot] - MAX_VISIBLE_MESSAGES)));

    display.fillRoundRect(119, thumbPosition, 5, thumbHeight, 2, 0);      // BLACK
    display.fillRect(120, (thumbPosition + 1), 3, (thumbHeight - 2), 1);  // WHITE
  }

  // POT INDICATOR GRID
  display.drawBitmap(102, 8, image_potMatrixGrid_bits, 10, 4, 0);
  display.drawRoundRect(98, 5, 18, 10, 1, 0);
  display.drawCircle(potCirclePositions[selectedPot].x, potCirclePositions[selectedPot].y, 1, 0);

  display.display();
}
// *********************** //
// EEPROM FUNCTIONS
// *********************** //
// *********************** //
// SAVE SETTINGS TO EEPROM
// *********************** //
void saveSettingsToEEPROM() {
  SavedSettings settings;
  settings.signature = EEPROM_SIGNATURE;

  // COPY CURRENT SETTINGS TO THE STRUCTURE
  for (int i = 0; i < N_POTS; i++) {
    settings.messageCount[i] = messageCount[i];
    for (int j = 0; j < MAX_MESSAGES_PER_POT; j++) {
      settings.potMessages[i][j] = potMessages[i][j];
    }
  }

  // WRITE TO EEPROM
  EEPROM.put(EEPROM_ADDR, settings);

  Serial.println("Settings saved to EEPROM");
}
// *********************** //
// LOAD SETTINGS FROM EEPROM
// *********************** //
void loadSettingsFromEEPROM() {
  SavedSettings settings;
  EEPROM.get(EEPROM_ADDR, settings);

  // VERFIY SIGNATURE
  if (settings.signature == EEPROM_SIGNATURE) {
    // COPY SETTINGS FROM EEPROM
    for (int i = 0; i < N_POTS; i++) {
      messageCount[i] = settings.messageCount[i];
      for (int j = 0; j < MAX_MESSAGES_PER_POT; j++) {
        potMessages[i][j] = settings.potMessages[i][j];
      }
    }
    Serial.println("Settings loaded from EEPROM");
  } else {
    Serial.println("No valid settings found in EEPROM, using defaults");
  }
}
// *********************** //
// INITIALISE
// *********************** //
// *********************** //
// INTIIALISE *ALL*
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
  display.print("v1.0");

  // DISPLAY
  display.display();

  // INIT POTS
  for (int i = 0; i < N_POTS; i++) {
    responsivePot[i].setAnalogResolution(1023);
  }

  // LOADING BAR UPDATE
  display.fillRoundRect(19, 40, 30, 4, 1, 0);
  ;                   // LOADING BAR INNER
  display.display();  // REFRESH
  delay(250);         // SMALL DELAY

  // LOAD SETTINGS SAVED TO EEPROM
  loadSettingsFromEEPROM();

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
    for (int i = 0; i < N_POTS; i++) {
      messageCount[i] = 1;  // Each pot starts with 1 message
      potMessages[i][0].channel = potChannels[i];
      potMessages[i][0].cc = potCCs[i];
      potMessages[i][0].value = 0;
    }
  }

  // LOADING BAR UPDATE
  display.fillRoundRect(19, 40, 90, 4, 1, 0);  // LOADING BAR INNER
  display.display();                           // REFRESH
  delay(1000);                                 // SMALL DELAY
}