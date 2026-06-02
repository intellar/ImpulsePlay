#include "battery_handler.h"
#include "esp_adc_cal.h"

// --- Définitions et variables internes au module Batterie ---
#define BATT_ADC_PIN 2
#define BATT_DIVIDER_FACTOR 2.0
const float V_MAX_CHARGE = 3.79;
const float V_MIN_SAFE = 3.20;

#define ADC_SAMPLES 64
static const adc_channel_t channel = ADC_CHANNEL_2;
static const adc_atten_t atten = ADC_ATTEN_DB_12; // Mis à jour pour éviter l'avertissement de dépréciation
static esp_adc_cal_characteristics_t *adc_chars;

void init_battery_adc() {
    Serial.println("Configuration de l'ADC pour la batterie...");
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten((adc1_channel_t)channel, atten);
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, atten, ADC_WIDTH_BIT_12, 0, adc_chars);
}

float read_battery_voltage() {
    uint32_t adc_reading = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)channel);
    }
    adc_reading /= ADC_SAMPLES;

    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    float V_lue = voltage_mv / 1000.0f;
    float V_batterie = V_lue * BATT_DIVIDER_FACTOR;

    if (V_batterie < 1.0) {
        return 0.0;
    }
    return V_batterie;
}

float calculate_battery_percentage(float V_batterie) {
    if (V_batterie > V_MAX_CHARGE) return 100.0;
    if (V_batterie < V_MIN_SAFE) return 0.0;

    float V_range = V_MAX_CHARGE - V_MIN_SAFE;
    float pourcentage = (V_batterie - V_MIN_SAFE) / V_range * 100.0;

    return constrain(pourcentage, 0.0, 100.0);
}