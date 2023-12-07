#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include <SPI.h>
#include <LoRa.h>
#include "ClosedCube_HDC1080.h"


/*

sda: 21
scl: 22
moisture: 34
light: 35
battery: 25
ar: 26

*/
ClosedCube_HDC1080 hdc1080;

#define moisture 34
#define light 35
#define clockPin 18
#define dataPin 21

#define misoPin 23
#define mosiPin 19
#define nssPin 5
#define resetPin 14
#define dio0Pin 13
#define dio1Pin 12

// For Pulse Counter
#define WAIT_TIME 500
#define MIN -21300*WAIT_TIME/1000
#define MAX -740*WAIT_TIME/1000
int16_t count = 0x10;

int counter = 0;

double readings[4] = { 0, 0, 0, 0 };

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);

    pinMode(moisture, INPUT);
    // pinMode(light, INPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(misoPin, INPUT);
    pinMode(mosiPin, OUTPUT);
    pinMode(GPIO_NUM_26, OUTPUT);
    digitalWrite(GPIO_NUM_26, HIGH);
    hdc1080.begin(0x40);

    pcnt_unit_t unit = PCNT_UNIT_0;

    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control
        .pulse_gpio_num = moisture,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        .pos_mode = PCNT_COUNT_DIS,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_INC,   // Keep the counter value on the negative edge
        .unit = unit,
        .channel = PCNT_CHANNEL_0,
    };
    pcnt_unit_config(&pcnt_config);


    while (!Serial)
        Serial.println("LoRa Sender");

    // setup LoRa transceiver moduleu
    LoRa.setPins(SS, resetPin, dio0Pin);

    // replace the LoRa.begin(---E-) argument with your location's frequency
    // 433E6 for Asia
    // 866E6 for Europe
    // 915E6 for North America
    // LoRa.setSPIFrequency(1E6);
    // SPI.begin(SCK, MISO, MOSI, nssPin);
    while (!LoRa.begin(915E6)) {
        Serial.println(".");
        delay(500);
    }
    // Change sync word (0xF3) to match the receiver
    // The sync word assures you don't get LoRa messages from other LoRa transceivers
    // ranges from 0-0xFF
    LoRa.setSyncWord(0xF3);
    // LoRa.setTxPower(20);
    Serial.println("LoRa Initializing OK! (sender)");
}
void getReadings()
{
    digitalWrite(GPIO_NUM_26, HIGH);

    readings[2] = hdc1080.readTemperature();// d    * 1.8 + 32;
    readings[3] = hdc1080.readHumidity();


    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
    delay(WAIT_TIME);
    pcnt_get_counter_value(PCNT_UNIT_0, &count);
    pcnt_counter_pause(PCNT_UNIT_0);
    Serial.println(count);
    double moisture_val = (((double)(count - MIN) / (MAX - MIN)) * 100);

    // int light_val = (int)(100 * analogRead(light) / 4095);
    readings[0] = moisture_val;
    // readings[1] = light_val;

    digitalWrite(GPIO_NUM_26, LOW);
    delay(100);
};

void loop()
{
    getReadings();
    delay(500);
    String packetInfo = "Sending packet: " + String(counter) + ": " + String(readings[0]) + "%M " + String(readings[2]) + "F " + readings[3] + "%H " + readings[1] + "%L";
    Serial.println(packetInfo);

    // sends packet over radio
    LoRa.beginPacket();
    LoRa.print(packetInfo);
    LoRa.endPacket();

    counter++;
    delay(500);
}

