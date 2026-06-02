#ifndef BATTERY_HANDLER_H
#define BATTERY_HANDLER_H

#include <Arduino.h>

/**
 * @brief Configure l'ADC pour la lecture de la tension de la batterie.
 */
void init_battery_adc();

/**
 * @brief Lit la tension de la batterie.
 * @return La tension en Volts.
 */
float read_battery_voltage();

/**
 * @brief Calcule le pourcentage de charge de la batterie.
 * @return Le pourcentage de charge (0-100).
 */
float calculate_battery_percentage(float voltage);

#endif // BATTERY_HANDLER_H