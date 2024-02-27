#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "ClosedCube_HDC1080.h"
#include "esp_wifi.h"
#include <driver/adc.h>
#include "sensor_pinouts.h"
#include <Wire.h>
#include <driver/adc.h>
#include <EEPROM.h>
// #include "pinouts_and_constants.h"

ClosedCube_HDC1080 hdc1080;

void measureSetup()
{
    digitalWrite(GPIO_NUM_26, HIGH);

    pcnt_unit_t unit = PCNT_UNIT_0;

    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control
        .pulse_gpio_num = moisture,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        .pos_mode = PCNT_COUNT_DIS,      // Count up on the positive edge
        .neg_mode = PCNT_COUNT_INC,      // Keep the counter value on the negative edge
        .unit = unit,
        .channel = PCNT_CHANNEL_0,
    };
    pcnt_unit_config(&pcnt_config);

    hdc1080.begin(0x40);
}
void getReadings()
{
    // Enable the Power Rail
    digitalWrite(power_rail, HIGH);

    // TODO: Do we need this delay (probably not)
    delay(100);

    // Moisture Reading
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
    // TODO: Take time here
    // FIXME: We need to delay but we also need to be taking the time for this
    delay(SOIL_PULSE_COUNT_DELAY);
    pcnt_get_counter_value(PCNT_UNIT_0, &moisture_count);
    pcnt_counter_pause(PCNT_UNIT_0);
    // TODO: Take time here
    // TODO: calculate difference
    hdc1080.begin(0x40);
    readings[0] = (double)moisture_count;

    // Light Reading
    // int light_val = (int)(100 * analogRead(light) / 4095);
    double light_val = (double)analogRead(light);
    readings[1] = light_val;

    // Temperature/Humidity Reading
    // TODO: Do we need to begin this every time
    delay(100);
    readings[2] = hdc1080.readTemperature() * 1.8 + 32;
    readings[3] = hdc1080.readHumidity();

    // Battery Reading
    int battery_level = 0;
    esp_err_t r = adc2_get_raw(ADC2_CHANNEL_8, ADC_WIDTH_12Bit, &battery_level);
    readings[4] = (battery_level / 4095.0) * 3.3;

    // Disable the Power Rail
    digitalWrite(power_rail, LOW);
};

// writes sensor readings to flash memory (120 bytes)
boolean saveReadings(timeval *tv_now)
{
    int base_addr = ((int)floor(tv_now->tv_sec / timer)) % 3;

    for (int i = 0; i < 5; i++)
    {
        EEPROM.writeDouble((i + base_addr * 5) * sizeof(double), readings[i]);
    }
    EEPROM.commit();
    return base_addr == 2;
}

// prints all of the 3 readings from memory
void printReadings()
{
    for (int numReadings = 0; numReadings < 3; numReadings++)
    {
        double tempReadings[5] = {0, 0, 0, 0, 0};
        for (int i = 0; i < 5; i++)
        {
            tempReadings[i] = EEPROM.readDouble((i + numReadings * 5) * sizeof(double));
        }
        String packetInfo = "Packet " + String(numReadings) + ": " + String(tempReadings[0]) + "%M " + String(tempReadings[2]) + "F " + tempReadings[3] + "%H " + tempReadings[1] + "%L " + tempReadings[4] + " mV";
        Serial.println(packetInfo);
    }
}

// clears all readings from memory (sets them to 0)
void clearReadings()
{
    for (int i = 0; i < 15; i++)
    {
        EEPROM.writeDouble(i * sizeof(double), 0);
    }
    EEPROM.commit();
}