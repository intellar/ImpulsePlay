#include "ui_handler.h"
#include "display_handler.h" // Pour current_mode
#include "config.h"          // Pour la couleur transparente
#include "battery_handler.h" // Pour lire la tension

// Déclarations externes pour les objets globaux
extern TFT_eSPI tft;

void draw_buttons(TFT_eSprite* canvas) {
    // Dimensions réduites pour de meilleures performances
    const int16_t button_w = 60;
    const int16_t button_h = 30;
    const int16_t button_x = 5;
    const int16_t button_y = tft.height() - button_h - 5; // Marge de 5px en bas
    const char* button_text = (current_mode == MODE_EYES) ? "BALL" : "EYES";
    canvas->drawRect(button_x, button_y, button_w, button_h, TFT_DARKGREY);
    canvas->setCursor(button_x + 10, button_y + 7);
    canvas->setTextSize(2);
    canvas->setTextColor(TFT_DARKGREY);
    canvas->print(button_text);
}

void draw_ui(TFT_eSprite* canvas) {
    // --- Calcul du FPS ---
    static unsigned long last_fps_time = 0;
    static int frame_count = 0;
    static float fps = 0.0;

    frame_count++;
    unsigned long current_time = millis();
    if (current_time - last_fps_time >= 1000) {
        fps = frame_count;
        frame_count = 0;
        last_fps_time = current_time;
    }

    // --- Dessin de l'interface sur le canvas fourni ---
    // draw_buttons(canvas); // Dessine le bouton

    canvas->setTextSize(1);
    canvas->setTextColor(TFT_WHITE);

    canvas->setCursor(5, 1);
    float tension = read_battery_voltage();
    float pourcentage = calculate_battery_percentage(tension);
    canvas->printf("%.2fV (%.0f%%)", tension, pourcentage);

    canvas->setCursor(100, 1);
    canvas->printf("%.0f FPS", fps);

    if (current_game_state != MENU) {
        canvas->fillCircle(tft.width() - 25, 30, 12, TFT_DARKGREY);
        canvas->fillCircle(tft.width() - 25, 30, 8, TFT_BLACK);
    }
}