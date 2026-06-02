#ifndef EYES_ANIMATION_H
#define EYES_ANIMATION_H

#include <TFT_eSPI.h>

// Déclaration externe pour les variables globales utilisées par l'animation
extern bool laser_target_active;

/**
 * @brief Initialise les ressources pour l'animation des yeux.
 */
void init_eyes_animation();

/**
 * @brief Dessine l'animation des yeux en fonction des données de l'IMU.
 */
void draw_eyes(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]);

/**
 * @brief Dessine l'animation du laser visant un point.
 */
void draw_laser_animation(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]);

/**
 * @brief Wrapper qui dessine les yeux et les affiche sur l'écran.
 */
void draw_imu_axes(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]);

#endif // EYES_ANIMATION_H