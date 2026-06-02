#include <Arduino.h>
#include "display_handler.h"
#include "imu_handler.h"
#include "battery_handler.h"

// --- Ajout pour le logging IMU ---
extern "C" {
#include "bmi160.h" // Pour accéder à la structure bmi160_sensor_data
}
// Définition (instanciation) de la variable globale.
AppMode current_mode; 

// --- Variables pour la mise à jour non bloquante ---
unsigned long last_battery_check = 0;
const int battery_check_interval = 5000; // 5 secondes

void setup() {
  Serial.begin(115200);
  delay(2000);

  init_display();
  init_battery_adc();
  init_imu();
  init_eyes_animation(); // Initialiser le module des yeux
  current_mode = MODE_BALL;
}

void loop() {
  handle_touch();

  // Traiter les données de l'IMU en permanence
  float final_x[3], final_y[3], final_z[3]; // Axes calibrés et orientés
  float gravity_vec[3]; // Pour stocker les valeurs de gravité
  float gyro_rad_s[3];  // Pour stocker les vitesses du gyroscope en rad/s

  process_imu_data(final_x, final_y, final_z, gravity_vec, gyro_rad_s);

  // --- Sélection du mode de rendu ---
  switch (current_mode) {
    case MODE_EYES:
      if (laser_target_active) {
        draw_laser_animation(final_x, final_y, final_z, gravity_vec, gyro_rad_s);
      } else {
        draw_imu_axes(final_x, final_y, final_z, gravity_vec, gyro_rad_s);
      }
      break;
    case MODE_BALL:
      draw_ball_animation(gravity_vec);
      break;
  }
}