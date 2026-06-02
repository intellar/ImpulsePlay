#ifndef IMU_HANDLER_H
#define IMU_HANDLER_H

#include <Arduino.h>

// Inclure la définition de la structure bmi160_dev pour pouvoir la déclarer en extern
extern "C" {
#include "bmi160.h"
}

// Déclaration externe du périphérique BMI160 pour qu'il soit accessible
// par d'autres fichiers si nécessaire (par exemple, pour le logging dans main.cpp).
extern struct bmi160_dev bmi160dev;

/**
 * @brief Initialise le capteur IMU BMI160.
 * @return true si l'initialisation est réussie, false sinon.
 */
bool init_imu();

/**
 * @brief Lit les données brutes de l'IMU, applique les filtres et calcule l'orientation.
 * @param final_x Vecteur de l'axe X orienté.
 * @param final_y Vecteur de l'axe Y orienté.
 * @param final_z Vecteur de l'axe Z orienté.
 * @param gravity_vec Vecteur de gravité mesuré par l'accéléromètre.
 * @param gyro_rad_s Vitesses de rotation en radians par seconde.
 */
void process_imu_data(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], float gyro_rad_s[3]);

/**
 * @brief Réinitialise l'orientation de référence de l'IMU.
 */
void start_axis_calibration();

#endif // IMU_HANDLER_H