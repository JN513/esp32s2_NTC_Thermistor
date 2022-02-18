#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"

#include "driver/adc.h"
#include <esp_adc_cal.h>

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

const char *TAG = "Thermistor";

const double beta = 3950.0;
const double ro = 100000.0;
const double to = 298.15;

const double r1 = 10000.0;
const double vcc = 3.3;
const double adcmax = 8191.0;

const int numAmos = 5;

static const adc_channel_t channel = ADC_CHANNEL_0;
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static esp_adc_cal_characteristics_t *adc_chars;


struct Adc_return {
    u_int32_t adc_reading;
    u_int32_t voltage;
};

static void check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

struct Adc_return adc_read(){
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)channel);
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    //printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);

    struct Adc_return ret = {
        .adc_reading = adc_reading,
        .voltage = voltage
    };

    return ret;
}

void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    //Characterize ADC
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    double rx = ro * exp(-beta / to);

    while (1) {
        u_int32_t soma = 0;
        u_int32_t soma_volt = 0;

        for (int i = 0; i < numAmos; i++) {
            struct Adc_return ret = adc_read();

            soma += ret.adc_reading;
            soma_volt += ret.voltage;

            vTaskDelay(1);
        }

        double v_med = (double)soma_volt / (double)numAmos;
        v_med /= 1000.000;

        printf("voltagem: %lf\n", v_med);

        //ressitencia do resistor
        double rt = (vcc * r1) / v_med - r1;
        printf("reistencia: %lf\n", rt);

        //temperatura

        // kelvin
        double t_kelvin = beta / log(rt / rx);

        // celsius
        double t_celsius = t_kelvin - 273.00;

        // fireheit
        double t_fahrenheit = t_celsius * 1.8 + 32.0;

        printf("Temperatura: %.2lf ºC, %.2lf ºF, %.2lf ºK\n", t_celsius, t_fahrenheit, t_kelvin);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
