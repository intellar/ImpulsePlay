#include "display_handler.h"
#include "imu_handler.h"
#include "SmoothedValue.h" // Assurez-vous que ce fichier est dans le dossier 'include'
#include "ball_animation.h"
#include "eyes_animation.h" // Pour init_eyes_animation()
#include "config.h" // Pour la couleur transparente

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite eyes_spr = TFT_eSprite(&tft);
// Small UI sprites to reduce flicker
TFT_eSprite ui_top_bar_spr = TFT_eSprite(&tft);
TFT_eSprite ui_button_spr = TFT_eSprite(&tft);

static char calibration_target_axis = ' ';
static bool ok_button_was_pressed = false; // Pour gérer le clic sur OK
static bool axis_calibrated[3] = {false, false, false}; // [X, Y, Z]
bool is_calibrating_axis = false;
static char active_calibration_axis = ' ';

void init_display() {
    Serial.println("Initialisation de l'écran TFT...");
    tft.init();
    tft.setRotation(1); // 1 pour une orientation paysage (horizontale)
    Serial.println("Écran TFT initialisé.");

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.println("Systeme OK");
    tft.println("Touchez pour dessiner");
    
    // Créer les sprites globaux
    eyes_spr.createSprite(tft.width(), 100);
    // Créer les sprites pour l'UI
    ui_top_bar_spr.createSprite(tft.width(), 12); // Juste assez haut pour le texte
    ui_button_spr.createSprite(60, 30); // Taille exacte du bouton

    // Initialiser les modules d'animation qui en ont besoin
    init_eyes_animation();
}

void clear_screen_and_reset() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.println("Systeme OK");
    tft.println("Touchez pour dessiner");
    axis_calibrated[0] = false;
    axis_calibrated[1] = false;
    axis_calibrated[2] = false;
}

void handle_touch() {
    uint16_t x, y;

    // Variables statiques pour suivre l'état du geste de lancer
    static bool is_dragging = false;
    // Historique des 2 derniers points pour calculer la vitesse du geste
    static int dragged_ball_id = -1; // ID de la balle en cours de manipulation
    static int32_t touch_history_x[2]; // Utiliser un type signé pour gérer les déplacements négatifs
    static int32_t touch_history_y[2];
    static unsigned long touch_history_time[2];
    static int history_idx = 0;

    static unsigned long last_touch_release_time = 0;
    const unsigned long DEBOUNCE_DELAY = 200; // 200ms

    bool touched = tft.getTouch(&x, &y, 100);

    if (touched) {
        if (millis() - last_touch_release_time < DEBOUNCE_DELAY) return;

        // Touch handling priority: menu, gear icon, then game gestures

        // Priorité 1 : Gérer les clics dans le menu s'il est ouvert
        if (current_game_state == MENU) {
            if (!is_dragging) { // New touch only
                is_dragging = true; // Marquer ce contact comme "en cours"
                if (y > tft.height()/2 - 60 && y < tft.height()/2 - 20) { // Clic sur "Mode Jeu"
                    current_game_mode = MODE_GAME; reset_game();
                } else if (y > tft.height()/2 - 20 && y < tft.height()/2 + 20) { // Clic sur "Mode Simulation"
                    current_game_mode = MODE_SIMULATION; reset_game();
                } else if (y > tft.height()/2 + 20 && y < tft.height()/2 + 60) { // Clic sur "Reset High Score"
                    reset_high_score();
                }
            }
            return; // On a géré le clic, on ne fait rien d'autre.
        }

        // Priorité 2 : Gérer le clic sur le bouton Menu pour l'ouvrir/fermer
        const int16_t menu_btn_size = 60;
        if (x > tft.width() - menu_btn_size && y < menu_btn_size) {
            current_game_state = (current_game_state == PLAYING) ? MENU : PLAYING;
            last_touch_release_time = millis(); // Debounce
            is_dragging = false; // Annuler tout drag en cours
            return; // On a géré le clic, on ne fait rien d'autre.
        }

        // Priorité 3 : Si on arrive ici, on gère la logique du jeu (MODE_BALL)
        // (Le code ci-dessous ne s'exécute que si on n'a pas cliqué sur le menu)

        const int16_t button_h = 30; // Cette partie est commentée mais on garde la logique
        const int16_t button_y_start = tft.height() - button_h - 5;
        const int16_t button_x_start = 5;
        const int16_t button_x_end = button_x_start + 60;

        // // Si le toucher est sur le bouton, on change de mode
        // if (x >= button_x_start && x <= button_x_end && y >= button_y_start) {
        //     current_mode = (current_mode == MODE_EYES) ? MODE_BALL : MODE_EYES;
        //     Serial.printf("Changement de mode vers: %s\n", (current_mode == MODE_EYES) ? "EYES" : "BALL");
        //     tft.fillScreen(TFT_BLACK); // Effacer l'écran au changement de mode
        //     delay(200); // Délai pour éviter les doubles détections
        //     is_dragging = false; // Annuler tout drag en cours
        //     return;
        // }

        // Si on est en mode balle et qu'on ne touche pas le bouton
        if (current_mode == MODE_BALL) {
            if (current_game_state == GAME_OVER) {
                reset_game();
                return; // On ne fait rien d'autre ce tour-ci
            }

            if (!is_dragging) { // Premier contact
                is_dragging = true;
                dragged_ball_id = set_ball_position(x, y, -1, nullptr);
                // Note: la logique de 'ball_was_created' a été déplacée dans arm_bomb avec un délai,
                // donc on peut simplement tenter d'armer.
                arm_bomb(dragged_ball_id);

                // Initialiser l'historique pour le calcul de la vitesse
                touch_history_x[0] = touch_history_x[1] = x;
                touch_history_y[0] = touch_history_y[1] = y;
                touch_history_time[0] = touch_history_time[1] = millis();
            } else { // En train de glisser
                // Mettre à jour l'historique des positions
                history_idx = (history_idx + 1) % 2;
                touch_history_x[history_idx] = x;
                touch_history_y[history_idx] = y;
                touch_history_time[history_idx] = millis();
            }
            // La balle suit le doigt en continu (en réinitialisant sa position)
            if (dragged_ball_id != -1) {
                set_ball_position(x, y, dragged_ball_id);
            }
        }
    } else { // Le doigt n'est plus sur l'écran
        if (is_dragging) { // On vient de relâcher
            is_dragging = false;

            last_touch_release_time = millis();
            
            // Vérifier si la balle a été relâchée dans la zone de suppression (coin haut gauche)
            const int16_t delete_zone_size = 50;
            if (dragged_ball_id != -1 && touch_history_x[history_idx] < delete_zone_size && touch_history_y[history_idx] < delete_zone_size) {
                deactivate_ball(dragged_ball_id, false); // false = pas d'explosion
            } else {
                // Sinon, calculer la vitesse du geste pour lancer la balle
                int prev_idx = (history_idx + 1) % 2;
                float dt_ms = touch_history_time[history_idx] - touch_history_time[prev_idx];
                if (dragged_ball_id != -1 && dt_ms > 0 && dt_ms < 100) { // Geste rapide
                    float dt_s = dt_ms / 1000.0f;
                    float vx = (touch_history_x[history_idx] - touch_history_x[prev_idx]) / dt_s;
                    float vy = (touch_history_y[history_idx] - touch_history_y[prev_idx]) / dt_s;

                    // Appliquer la vélocité calculée à la balle
                    set_ball_velocity(dragged_ball_id, vx, vy);
                }
            }
            dragged_ball_id = -1; // Oublier la balle
        }
    }
}