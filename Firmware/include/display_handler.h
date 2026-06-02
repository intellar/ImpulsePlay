#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ball_animation.h" // Inclure les nouveaux en-têtes
#include "eyes_animation.h"
#include "ui_handler.h"

extern TFT_eSPI tft; // Rendre l'objet tft accessible globalement

// Définition des modes de l'application
enum AppMode { MODE_EYES, MODE_BALL };
extern AppMode current_mode; // Déclaration que la variable existe quelque part.
extern bool is_calibrating_axis; // Rendre visible pour main.cpp

/**
 * @brief Initialise l'écran TFT.
 */
void init_display();

/**
 * @brief Gère les interactions tactiles (non utilisé pour le moment avec LVGL).
 */
void handle_touch();

void clear_screen_and_reset();

// Déclaration de la fonction définie dans imu_handler.cpp pour qu'elle soit visible ici.
void start_axis_calibration();

#endif // DISPLAY_HANDLER_H