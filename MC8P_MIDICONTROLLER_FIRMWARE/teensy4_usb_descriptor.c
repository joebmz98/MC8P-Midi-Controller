// ********************* //
// MC8P midiController   //
// firmware version 1.0  //
// ********************* //
// controller by         //
// instruments.axs       //
// ********************* //
//
// file: teensy4_usb_descriptor.c
//
// This file overrides the default Teensy 4.0 USB descriptor's product name
// and manufacturer name.
//
// For more details see:
//   teensy/avr/cores/teensy4/usb_names.h
//   teensy/avr/cores/teensy4/usb_desc.c
//   teensy/avr/cores/teensy4/usb_desc.h
//
#include <avr/pgmspace.h>
#include <usb_names.h> // teensy/avr/cores/teensy4/usb_names.h

// Updated product name: "MC8P MIDI Controller" (19 characters)
#define PRODUCT_NAME        {'M','C','8','P',' ','M','I','D','I',' ','C','o','n','t','r','o','l','l','e','r'}
#define PRODUCT_NAME_LEN    20

// Updated manufacturer name: "instruments.axs" (14 characters)
#define MANUFACTURER_NAME   {'i','n','s','t','r','u','m','e','n','t','s','.','a','x','s'}
#define MANUFACTURER_NAME_LEN 15

PROGMEM extern struct usb_string_descriptor_struct usb_string_manufacturer_name = {
    2 + MANUFACTURER_NAME_LEN * 2,  // Descriptor size (header + Unicode string)
    3,                              // Descriptor type (string)
    MANUFACTURER_NAME               // Manufacturer name
};

PROGMEM extern struct usb_string_descriptor_struct usb_string_product_name = {
    2 + PRODUCT_NAME_LEN * 2,       // Descriptor size
    3,                              // Descriptor type (string)
    PRODUCT_NAME                    // Product name
};