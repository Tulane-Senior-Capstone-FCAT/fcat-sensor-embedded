#include "Arduino.h"
#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "rom/rtc.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5           /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR uint32_t _cal_32k = 1600000;

void debug_xtal_out_dac1()
{
    SET_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32N_MUX_SEL | RTC_IO_X32P_MUX_SEL);
    CLEAR_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32P_RDE | RTC_IO_X32P_RUE | RTC_IO_X32N_RUE | RTC_IO_X32N_RDE);
    CLEAR_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32N_MUX_SEL | RTC_IO_X32P_MUX_SEL);
    SET_PERI_REG_BITS(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_DAC_XTAL_32K, 1, RTC_IO_DAC_XTAL_32K_S);
    SET_PERI_REG_BITS(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_DRES_XTAL_32K, 3, RTC_IO_DRES_XTAL_32K_S);
    SET_PERI_REG_BITS(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_DBIAS_XTAL_32K, 0, RTC_IO_DBIAS_XTAL_32K_S);
    SET_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_XPD_XTAL_32K);
    REG_SET_BIT(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_MUX_SEL_M);
    REG_CLR_BIT(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_RDE_M | RTC_IO_PDAC1_RUE_M);
    REG_SET_FIELD(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_FUN_SEL, 1);
    REG_SET_FIELD(SENS_SAR_DAC_CTRL1_REG, SENS_DEBUG_BIT_SEL, 0);
    const uint8_t sel = 4; /* sel = 4 : 32k XTAL; sel = 5 : internal 150k RC */
    REG_SET_FIELD(RTC_IO_RTC_DEBUG_SEL_REG, RTC_IO_DEBUG_SEL0, sel);
}

const float factor = (1 << 19) * 1000.0f;

#define CALIBRATE_ONE(cali_clk) calibrate_one(cali_clk, #cali_clk)

static uint32_t calibrate_one(rtc_cal_sel_t cal_clk, const char *name)
{

    const uint32_t cal_count = 1000;
    uint32_t cali_val;
    printf("%s:\n", name);
    for (int i = 0; i < 5; ++i)
    {
        printf("calibrate (%d): ", i);
        cali_val = rtc_clk_cal(cal_clk, cal_count);
        printf("%.3f kHz\n", factor / (float)cali_val);
    }
    return cali_val;
}

float crystal_frequency()
{
    const uint32_t cal_count = 100;
    uint32_t cali_val = rtc_clk_cal(RTC_CAL_32K_XTAL, cal_count);
    float freq_32k = factor / (float)cali_val;
    return freq_32k;
}

void setExternalCrystalAsRTCSource()
{
    if (bootCount == 1)
    {
        Serial.println("First boot, bootstrap and enable 32k XTAL");
        rtc_clk_32k_enable(true);
        // rtc_clk_cpu_freq_set_xtal();
    }
    debug_xtal_out_dac1();

    float delta = 1;
    float freq_32k;
    uint32_t cal_32k;
    uint32_t startCal = millis();
    while (delta > 0.002 && millis() - startCal < 15000)
    {
        cal_32k = CALIBRATE_ONE(RTC_CAL_32K_XTAL);
        freq_32k = factor / (float)cal_32k;
        delta = abs(freq_32k - 32.768);
        printf("Waiting for 32kHz clock to be stable: %.3f kHz\n", freq_32k);
    }
    if (delta < 0.002)
    {
        rtc_clk_slow_freq_set(RTC_SLOW_FREQ_32K_XTAL);
        uint32_t rtc_clk_calibration = REG_READ(RTC_SLOW_CLK_CAL_REG);
        printf("Slow clock calibration: %u\n", rtc_clk_calibration);
        printf("32k calibration: %u\n", cal_32k);
        if ((rtc_clk_calibration > (cal_32k + 5)) || (rtc_clk_calibration < (cal_32k - 5)))
        {
            printf("Miscalibrated, setting calibration register to 32k calibration.\n");
            REG_WRITE(RTC_SLOW_CLK_CAL_REG, cal_32k);
            rtc_clk_calibration = REG_READ(RTC_SLOW_CLK_CAL_REG);
            if (rtc_clk_calibration != cal_32k)
                printf("ERROR Calibration write failure.\n");
        }

        if (cal_32k == 0)
            printf("32K XTAL OSC has not started up");
        else
            printf("done\n");

        if (rtc_clk_32k_enabled())
            Serial.println("OSC Enabled");
    }
    else
        Serial.println("OSC Not Enabled, using Internal 150KHz RC");
}

void setup()
{
    delay(500);
    Serial.begin(115200);

    // Increment boot number and print it every reboot
    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));

    setExternalCrystalAsRTCSource();

    //   First we configure the wake up source
    //   We set our ESP32 to wake up every 5 seconds
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                   " Seconds");

    // By default the ESP32 powers down all peripherals which are not needed to wake up again.
    delay(50);
    uint64_t ticks = rtc_time_get();
    Serial.printf("Going to sleep now, rtc ticks: %llu\n", ticks);
    Serial.flush();
    delay(50);
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
}

void loop()
{
    // put your main code here, to run repeatedly:
    // Left blank because we are just deepsleeping
}