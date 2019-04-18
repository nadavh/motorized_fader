#if !defined(button_panel_H)
#define button_panel_H

#include <Arduino.h>
#include <Wire.h>

#define BUTTON_PANEL_MAX_BUTTON_ID 7

// Selects between sides (8 pins each) of the GPIO chip
enum GpioSide {GPIO_SIDE_LIGHTS=0, GPIO_SIDE_BUTTONS};

struct ButtonState {
    bool pressed;
    bool changed;
};

class ButtonPanel {
public:
    ButtonPanel(uint8_t i2c_addr);

    // get/set internal structures without querying/updating GPIOs:
    void markLightOn(uint8_t button_id, bool on);
    void markLightsEnabled(bool enabled);
    struct ButtonState getButtonState(uint8_t button_id);

    // init/query/update GPIOs:
    void initHardware();
    void queryButtons();
    void updateLights();

private:
    uint8_t m_i2c_addr;
    uint8_t m_buttons_bitmap;
    uint8_t m_prev_buttons_bitmap;
    uint8_t m_lights_bitmap;
    bool m_lights_enabled;
    bool m_lights_changed;
    void i2cWriteReg(uint8_t reg, uint8_t data);
    void i2cSetIoDirection(enum GpioSide side, uint8_t io_pins_input);
    void i2cSetPullup(enum GpioSide side, uint8_t io_pins_pullup);
    void i2cWrite(enum GpioSide side, uint8_t io_pin_data);
    uint8_t i2cRead(enum GpioSide side);
};

#endif // button_panel_H
