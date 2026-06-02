#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <TFT_eSPI.h>

/**
 * @brief Dessine les boutons de l'interface.
 */
void draw_buttons();

/**
 * @brief Met à jour l'affichage de la tension de la batterie.
 */
void update_battery_display(float voltage, float percentage);

/**
 * @brief Dessine l'ensemble de l'interface utilisateur (UI) par-dessus l'animation en cours.
 * Cette fonction gère le rafraîchissement du FPS, des boutons et de la batterie.
 * @param canvas Le sprite sur lequel dessiner l'interface.
 */
void draw_ui(TFT_eSprite* canvas);

#endif // UI_HANDLER_H