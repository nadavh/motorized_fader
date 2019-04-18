#include "ButtonPanel.h"


ButtonPanel::ButtonPanel(uint8_t i2c_addr)
{
    m_i2c_addr = i2c_addr;
    m_buttons_bitmap = 0;
    m_prev_buttons_bitmap = 0;
    m_lights_bitmap = 0;
    m_lights_enabled = true;
    m_lights_changed = false;
}

// Init GPIOs
void ButtonPanel::initHardware()
{
    Wire.begin();
    i2cSetIoDirection(GPIO_SIDE_LIGHTS, 0x0); /* All outputs */
    i2cSetIoDirection(GPIO_SIDE_BUTTONS, 0xFF); /* All inputs */
    i2cSetPullup(GPIO_SIDE_BUTTONS, 0xFF); /* All inputs use pullup */
}

// Query button states.
void ButtonPanel::queryButtons()
{
    m_prev_buttons_bitmap = m_buttons_bitmap;
    m_buttons_bitmap = ~ i2cRead(GPIO_SIDE_BUTTONS); // inputs are reversed (pullup resistor)
}

// Update lights, based of previously called markLightOn()/markLightsEnabled().
void ButtonPanel::updateLights()
{
    if (m_lights_changed) {
        m_lights_changed = false;
        i2cWrite(GPIO_SIDE_LIGHTS, m_lights_enabled ? m_lights_bitmap : 0);
    }
}

// Mark light on/off. Note: updateLights() is required to apply changes.
void ButtonPanel::markLightOn(uint8_t button_id, bool on)
{
    if (button_id > BUTTON_PANEL_MAX_BUTTON_ID) {
        Serial.printf("Invalid button_id: %u\n", button_id);
        return;
    }

    uint8_t light_id = BUTTON_PANEL_MAX_BUTTON_ID - button_id;
    bool prev_on = bitRead(m_lights_bitmap, light_id);
    if (prev_on != on) {
        bitWrite(m_lights_bitmap, light_id, on);
        if (m_lights_enabled)
            m_lights_changed = true;
    }
}

// Mark all lights Enabled/disabled. Note: updateLights() is required to apply changes.
void ButtonPanel::markLightsEnabled(bool enabled)
{
    if (m_lights_enabled != enabled) {
        m_lights_enabled = enabled;
        m_lights_changed = true;
    }
}

// Get last queried button state (queryButtons() is required for update).
struct ButtonState ButtonPanel::getButtonState(uint8_t button_id)
{
    struct ButtonState button_state;
    bool prev_pressed = bitRead(m_prev_buttons_bitmap, button_id);
    button_state.pressed = bitRead(m_buttons_bitmap, button_id);
    button_state.changed = (button_state.pressed != prev_pressed);
    return button_state;
}

// Update i2c register.
void ButtonPanel::i2cWriteReg(uint8_t reg, uint8_t data)
{
    Wire.beginTransmission(m_i2c_addr);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();   
}

// Set IO direction (input/output) of GPIO pins on one of the controller sides.
// io_pins_input is a byte sized bitmap (Bit 0 = output, 1 = input).
void ButtonPanel::i2cSetIoDirection(enum GpioSide side, uint8_t io_pins_input)
{
    i2cWriteReg((side == GPIO_SIDE_LIGHTS ? 0x00 : 0x01), io_pins_input);
}

// Enable/disable pullup for input pins on one of the controller sides.
// io_pins_pullup is a byte sized bitmap (Bit 0 = pullup-disabled, 1 = pullup-enabled).
void ButtonPanel::i2cSetPullup(enum GpioSide side, uint8_t io_pins_pullup)
{
    i2cWriteReg((side == GPIO_SIDE_LIGHTS ? 0x0C : 0x0D), io_pins_pullup);
}

// Set the states of output pins on one of the controller sides.
// io_pin_data is a byte sized bitmap.
void ButtonPanel::i2cWrite(enum GpioSide side, uint8_t io_pin_data)
{
    i2cWriteReg((side == GPIO_SIDE_LIGHTS ? 0x12 : 0x13), io_pin_data);
}

// Get the states of input pins on one of the controller sides.
// Returns a byte sized bitmap.
uint8_t ButtonPanel::i2cRead(enum GpioSide side)
{
    Wire.beginTransmission(m_i2c_addr);
    Wire.write((uint8_t) side == GPIO_SIDE_LIGHTS ? 0x12 : 0x13);
    Wire.endTransmission();
    Wire.requestFrom(m_i2c_addr, 1);
    return Wire.read();
}
