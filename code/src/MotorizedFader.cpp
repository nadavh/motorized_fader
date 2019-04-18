#include <Arduino.h>
#include <CapacitiveSensor.h>
#include <TimerOne.h>
#include "ButtonPanel.h"

#define PIN_LED 11
#define PIN_SPEED 15
#define PIN_INPUT 21
#define PIN_MOVE_UP 19
#define PIN_MOVE_DOWN 20
#define PIN_TOUCH_SEND 18
#define PIN_TOUCH_RECEIVE 17

#define I2C_ADDR_FADER 0x20
#define I2C_ADDR_CC 0x21

#define MIDI_PULLS_PER_LOOP 5
#define MIDI_CHANNEL 1
#define MIDI_CC_FIRST_FADER 1
#define NUM_VIRT_FADERS 8
#define MIDI_CC_FIRST_BUTTON (MIDI_CC_FIRST_FADER + NUM_VIRT_FADERS)
#define NUM_CC_BUTTONS 8
#define MIDI_VAL_MIN 0
#define MIDI_VAL_MAX 127
#define isCcOn(value) (value >= 64)

#define MIN_FADER_SPEED 450
#define MAX_FADER_SPEED 1023

struct VirtFader {
    unsigned int id;
    unsigned int midi_cc;
    unsigned int local_location;
    unsigned int host_location;
};

struct CcButton {
    unsigned int id;
    unsigned int midi_cc;
};

struct VirtFader virt_faders[NUM_VIRT_FADERS];
struct VirtFader *active_virt_fader = NULL;
struct CcButton cc_buttons[NUM_CC_BUTTONS];

void readMidiCallback(byte channel, byte control, byte value);
void setFaderMovement(int amount);
void readMidi();
void handleFaderSelectionButtons();
void handleActiveFader();
void handleCcButtons();

ButtonPanel fader_button_panel = ButtonPanel(I2C_ADDR_FADER);
ButtonPanel cc_button_panel = ButtonPanel(I2C_ADDR_CC);
CapacitiveSensor touch_sensor = CapacitiveSensor(PIN_TOUCH_SEND, PIN_TOUCH_RECEIVE);

void setup()
{
    int i;

    // Button panels and related structs
    fader_button_panel.initHardware();
    for (i = 0; i < NUM_VIRT_FADERS; i++) {
        virt_faders[i].id = i;
        virt_faders[i].midi_cc = MIDI_CC_FIRST_FADER + i;
        virt_faders[i].local_location = 0;
        virt_faders[i].host_location = 0;
    }
    active_virt_fader = &virt_faders[0];

    cc_button_panel.initHardware();
    for (i = 0; i < NUM_CC_BUTTONS; i++) {
        cc_buttons[i].id = i;
        cc_buttons[i].midi_cc = MIDI_CC_FIRST_BUTTON + i;
    }

    // Fader
    pinMode(PIN_SPEED, OUTPUT);
    pinMode(PIN_MOVE_UP, OUTPUT);
    digitalWrite(PIN_MOVE_UP, HIGH);
    pinMode(PIN_MOVE_DOWN, OUTPUT);
    digitalWrite(PIN_MOVE_DOWN, HIGH);
    pinMode(PIN_LED, OUTPUT);
    Timer1.initialize(40);  // 40 us = 25 kHz

    // MIDI
    usbMIDI.setHandleControlChange(readMidiCallback);
}

void loop()
{
    readMidi();
    handleFaderSelectionButtons();
    handleActiveFader();
    handleCcButtons();
}

void readMidi()
{
    // Do multiple reads to handle a higher load
    for (int i = 0; i < MIDI_PULLS_PER_LOOP; i++)
        usbMIDI.read();
}

// Update fader and CC buttons based on host updates.
// It's a quick and dirty implementation that assumes consecutive CC assignments to fader and buttons.
void readMidiCallback(byte channel, byte control, byte value)
{
    unsigned int id;
    if (channel == MIDI_CHANNEL) {
        if (control >= MIDI_CC_FIRST_FADER && control < MIDI_CC_FIRST_FADER + NUM_VIRT_FADERS) {
            id = control - MIDI_CC_FIRST_FADER;
            virt_faders[id].host_location = value;
            Serial.printf("Received fader %u (cc %d) value: %d\n", id, control, value);
        } else if (control >= MIDI_CC_FIRST_BUTTON && control < MIDI_CC_FIRST_BUTTON + NUM_CC_BUTTONS) {
            id = control - MIDI_CC_FIRST_BUTTON;
            cc_button_panel.markLightOn(id, isCcOn(value));
            Serial.printf("Received button %u (cc %d) value: %d (lit %d)\n", id, control, value, isCcOn(value));
        }
    }
}

// Select fader if button pressed.
// Toggle enable/disable lights if pressed again.
// Update LEDs according to selected fader.
void handleFaderSelectionButtons()
{
    static bool lights_enabled = true;
    struct ButtonState button_state;
    int button_idx;

    fader_button_panel.queryButtons();
    for (button_idx = 0; button_idx < NUM_VIRT_FADERS; button_idx++) {
        button_state = fader_button_panel.getButtonState(button_idx);
        if (button_state.changed && button_state.pressed) {
            if (active_virt_fader == &virt_faders[button_idx]) {
                Serial.println("Toggling dim lights");
                lights_enabled = !lights_enabled;
            } else {
                Serial.printf("Setting active fader to id: %d\n", button_idx);
                active_virt_fader = &virt_faders[button_idx];
                lights_enabled = true;
            }

            fader_button_panel.markLightsEnabled(lights_enabled);
            cc_button_panel.markLightsEnabled(lights_enabled);
        }

        fader_button_panel.markLightOn(button_idx, active_virt_fader == &virt_faders[button_idx]);
    }

    fader_button_panel.updateLights();
}

void handleActiveFader()
{
    unsigned int fader_location;
    bool fader_touched;

    // Query HW fader location
    fader_location = map(analogRead(0), 0, 1000, 0, MIDI_VAL_MAX);
    if (fader_location > MIDI_VAL_MAX)
        fader_location = MIDI_VAL_MAX;
    
    // Query HW fader touch
    fader_touched = (touch_sensor.capacitiveSensor(30) > 2000);
    digitalWrite(PIN_LED, fader_touched);

    // Select fader destenation
    if (fader_touched)
        active_virt_fader->local_location = fader_location;
    else
        active_virt_fader->local_location = active_virt_fader->host_location;

    // Send MIDI
    if (active_virt_fader->host_location != active_virt_fader->local_location) {
        active_virt_fader->host_location = active_virt_fader->local_location;
        Serial.printf("Sending fader %u (cc %d) value: %d\n",
                      active_virt_fader->id,
                      active_virt_fader->midi_cc,
                      active_virt_fader->local_location);
        usbMIDI.sendControlChange(active_virt_fader->midi_cc, active_virt_fader->local_location, MIDI_CHANNEL);
    }

    // Update fader movement
    setFaderMovement(active_virt_fader->local_location - fader_location);
}

void setFaderMovement(int amount)
{
    // Set speed
    int speed = amount * amount + MIN_FADER_SPEED;
    if (speed > MAX_FADER_SPEED)
        speed = MAX_FADER_SPEED;

    Timer1.pwm(PIN_SPEED, speed);

    // Set direction
    if (abs(amount) < 2)
        amount = 0;

    if (amount > 0) {
        digitalWrite(PIN_MOVE_UP, LOW);
        digitalWrite(PIN_MOVE_DOWN, HIGH);
    } else if (amount == 0) {
        digitalWrite(PIN_MOVE_UP, LOW);
        digitalWrite(PIN_MOVE_DOWN, LOW);
    } else {
        digitalWrite(PIN_MOVE_DOWN, LOW);
        digitalWrite(PIN_MOVE_UP, HIGH);
    }
}

void handleCcButtons()
{
    int button_idx;
    struct ButtonState button_state;
    struct CcButton *cc_button = NULL;

    // Send MIDI on button changes
    cc_button_panel.queryButtons();
    for (button_idx = 0; button_idx < NUM_CC_BUTTONS; button_idx++) {
        cc_button = &cc_buttons[button_idx];
        button_state = cc_button_panel.getButtonState(button_idx);
        if (button_state.changed) {
            Serial.printf("Sending button %u (cc %d) pressed: %d\n",
                          cc_button->id, cc_button->midi_cc, button_state.pressed);
            usbMIDI.sendControlChange(cc_button->midi_cc, (button_state.pressed ? 127 : 0), MIDI_CHANNEL);
        }
    }

    // Update LEDs according to MIDI reads
    cc_button_panel.updateLights();
}
