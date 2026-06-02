#include "ball_animation.h"
#include "config.h" // Pour la couleur transparente
#include "ui_handler.h" // Pour pouvoir appeler draw_ui
#include "physics_engine.h"

#include <vector>  // Pour std::vector
#include "FS.h"
#include <LittleFS.h>
#include <stack>
// Déclarations externes pour les objets globaux
extern TFT_eSPI tft;

#define MAX_BALLS 12
#define MAX_PARTICLES 150 // Assez pour plusieurs explosions simultanées
static Ball balls[MAX_BALLS];
static Particle particles[MAX_PARTICLES];

const int INDICATOR_PUPIL_MASK_SIZE = 16;
float indicator_pupil_mask[INDICATOR_PUPIL_MASK_SIZE][INDICATOR_PUPIL_MASK_SIZE];

static const uint16_t ball_colors[] = {TFT_RED, TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_GREEN};
static uint16_t next_ball_color;

static unsigned long score = 0;
static unsigned long high_score = 0;

GameState current_game_state = PLAYING;
GameMode current_game_mode = MODE_GAME;
static unsigned long last_match_time = 0;
const unsigned long MATCH_TIMER_DURATION = 15000; // 15 s patience timer

static int screen_shake_frames = 0;
const int SHAKE_INTENSITY = 3;

/**
 * @brief Dessine une ellipse pleine avec rotation.
 * @note Implémentation locale (TFT_eSPI ne fournit pas fillRotatedEllipse).
 *       Rendu par balayage de scanlines : une seule racine carrée par ligne,
 *       donc un coût en O(hauteur) et non par pixel.
 * 
 * @param canvas Le sprite sur lequel dessiner.
 * @param x Centre X de l'ellipse.
 * @param y Centre Y de l'ellipse.
 * @param rx Rayon horizontal (avant rotation).
 * @param ry Rayon vertical (avant rotation).
 * @param angle_rad Angle de rotation en radians.
 * @param color Couleur de remplissage.
 */
void draw_rotated_ellipse(TFT_eSprite& canvas, int16_t x, int16_t y, int16_t rx, int16_t ry, float angle_rad, uint16_t color) {
    if (rx <= 0 || ry <= 0) return;

    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    // Pré-calcul des termes de l'équation quadratique de l'ellipse tournée.
    float A = (float)rx * rx * sin_a * sin_a + (float)ry * ry * cos_a * cos_a;
    float B = 2.0f * ((float)ry * ry - (float)rx * rx) * sin_a * cos_a;
    float C_base = (float)rx * rx * cos_a * cos_a + (float)ry * ry * sin_a * sin_a;
    float R_sq = (float)rx * rx * (float)ry * ry;

    // Demi-hauteur de la bounding box.
    float h_bound = sqrtf((float)rx * rx * sin_a * sin_a + (float)ry * ry * cos_a * cos_a);
    int hb = (int)floorf(h_bound);

    // Balayage par scanlines : une seule racine carrée par ligne (coût en O(hauteur)).
    for (int16_t j = -hb; j <= hb; j++) {
        float C = C_base * (float)j * (float)j - R_sq;
        float discriminant = B * B * (float)j * (float)j - 4.0f * A * C;
        if (discriminant < 0.0f) continue;

        float sqrt_discriminant = sqrtf(discriminant);
        float x1 = (-B * (float)j - sqrt_discriminant) / (2.0f * A);
        float x2 = (-B * (float)j + sqrt_discriminant) / (2.0f * A);

        int16_t start_x = (int16_t)lroundf(x1);
        int16_t end_x = (int16_t)lroundf(x2);
        if (start_x > end_x) { int16_t tmp = start_x; start_x = end_x; end_x = tmp; }

        canvas.drawFastHLine(x + start_x, y + j, end_x - start_x + 1, color);
    }
}

void load_high_score() {
    if (LittleFS.begin()) {
        if (LittleFS.exists("/highscore.txt")) {
            fs::File file = LittleFS.open("/highscore.txt", "r");
            if (file) {
                high_score = file.readString().toInt();
                file.close();
                Serial.printf("Meilleur score chargé : %lu\n", high_score);
            }
        }
        LittleFS.end();
    }
}

void save_high_score() {
    if (score > high_score) {
        high_score = score;
        if (LittleFS.begin()) {
            fs::File file = LittleFS.open("/highscore.txt", "w");
            if (file) {
                file.print(high_score);
                file.close();
                Serial.printf("Nouveau meilleur score sauvegardé : %lu\n", high_score);
            }
            LittleFS.end();
        }
    }
}
/**
 * @brief Pré-calcule le masque pour la pupille diffuse des mini-yeux.
 */
void precompute_indicator_pupil_mask() {
    const float center = (INDICATOR_PUPIL_MASK_SIZE - 1) / 2.0f;
    const float effect_radius = (INDICATOR_PUPIL_MASK_SIZE - 1) / 2.0f;

    for (int y = 0; y < INDICATOR_PUPIL_MASK_SIZE; ++y) {
        for (int x = 0; x < INDICATOR_PUPIL_MASK_SIZE; ++x) {
            const float dx = x - center;
            const float dy = y - center;
            const float distance = sqrt(dx * dx + dy * dy);

            if (distance >= effect_radius) {
                indicator_pupil_mask[y][x] = 0.0f; // Pas d'effet
            } else {
                float fade = 1.0f - (distance / effect_radius);
                indicator_pupil_mask[y][x] = fade * fade; // Falloff quadratique
            }
        }
    }
}

void reset_high_score() {
    high_score = 0;
    // On sauvegarde immédiatement la valeur 0
    if (LittleFS.begin()) {
        fs::File file = LittleFS.open("/highscore.txt", "w");
        if (file) {
            file.print(high_score);
            file.close();
            Serial.println("Meilleur score réinitialisé.");
        }
        LittleFS.end();
    }
}

void draw_menu(TFT_eSprite& canvas) {
    canvas.fillRect(0, 0, tft.width(), tft.height(), canvas.alphaBlend(180, TFT_BLACK, TFT_BLACK));

    canvas.setTextDatum(MC_DATUM);
    canvas.setTextSize(2);
    canvas.drawString(current_game_mode == MODE_GAME ? "> Mode Jeu" : "  Mode Jeu", tft.width() / 2, tft.height() / 2 - 40);
    canvas.drawString(current_game_mode == MODE_SIMULATION ? "> Mode Simulation" : "  Mode Simulation", tft.width() / 2, tft.height() / 2);
    canvas.drawString("  Reset High Score", tft.width() / 2, tft.height() / 2 + 40);
}

/**
 * @brief Dessine une pupille diffuse pour les mini-yeux.
 */
void draw_indicator_diffuse_pupil(TFT_eSprite& canvas, int16_t center_x, int16_t center_y, int16_t radius, uint16_t iris_color) {
    int16_t draw_size = radius * 2;
    int16_t start_x = center_x - radius;
    int16_t start_y = center_y - radius;

    uint8_t r_base = (iris_color & 0xF800) >> 8;
    uint8_t g_base = (iris_color & 0x07E0) >> 3;
    uint8_t b_base = (iris_color & 0x001F) << 3;

    for (int y = 0; y < draw_size; ++y) {
        for (int x = 0; x < draw_size; ++x) {
            int mask_x = map(x, 0, draw_size, 0, INDICATOR_PUPIL_MASK_SIZE - 1);
            int mask_y = map(y, 0, draw_size, 0, INDICATOR_PUPIL_MASK_SIZE - 1);

            float darkening_factor = 1.0f - indicator_pupil_mask[mask_y][mask_x];
            if (darkening_factor > 0.99f) continue;

            uint16_t final_color = canvas.color565(r_base * darkening_factor, g_base * darkening_factor, b_base * darkening_factor);
            canvas.drawPixel(start_x + x, start_y + y, final_color);
        }
    }
}

/**
 * @brief Dessine un indicateur en forme de mini-yeux pour la prochaine balle.
 * 
 * @param canvas Le sprite sur lequel dessiner.
 * @param x Centre X de l'indicateur.
 * @param y Centre Y de l'indicateur.
 * @param color Couleur des yeux (couleur de la prochaine balle).
 */
void draw_next_ball_indicator_eyes(TFT_eSprite& canvas, int16_t x, int16_t y, uint16_t color) {
    float patience_progress = 0.0f;
    if (current_game_state == PLAYING && current_game_mode == MODE_GAME && last_match_time > 0) {
        unsigned long time_since_match = millis() - last_match_time;
        patience_progress = (float)time_since_match / MATCH_TIMER_DURATION;
        patience_progress = constrain(patience_progress, 0.0f, 1.0f);
    }

    const float ANGER_START_THRESHOLD = 0.4f; // Anger effect starts after 40% of patience timer
    float angry_progress = 0.0f;
    if (patience_progress > ANGER_START_THRESHOLD) {
        // On remappe la progression pour que l'effet aille de 0 à 100% dans le temps restant
        angry_progress = (patience_progress - ANGER_START_THRESHOLD) / (1.0f - ANGER_START_THRESHOLD);
    }

    // La couleur de l'œil vire au rouge avec l'impatience
    uint16_t patient_color = canvas.alphaBlend(angry_progress * 255, TFT_RED, color);

    static enum { INDICATOR_IDLE, INDICATOR_BLINKING } indicator_state = INDICATOR_IDLE;
    static unsigned long indicator_last_blink_time = 0;
    static unsigned long indicator_next_blink = 4000;
    static unsigned long indicator_blink_start_time = 0;

    if (indicator_state == INDICATOR_IDLE && millis() - indicator_last_blink_time > indicator_next_blink) {
        indicator_state = INDICATOR_BLINKING;
        indicator_blink_start_time = millis();
    }

    // Dimensions fixes (légèrement agrandies)
    const int16_t base_eye_w = 24; // Encore plus grands
    const int16_t base_eye_h = 32;
    float growth_factor = 1.0f + angry_progress * 0.5f; // Les yeux grossissent avec l'impatience
    int16_t eye_w = base_eye_w * growth_factor;
    int16_t eye_h = base_eye_h * growth_factor;
    const int16_t base_pupil_w = 9; // Pupilles agrandies
    const int16_t base_pupil_h = 13; // Pupilles agrandies
    const int16_t spacing = 8;

    if (indicator_state == INDICATOR_BLINKING) {
        const unsigned long BLINK_DURATION = 200; // Clignotement rapide
        unsigned long elapsed = millis() - indicator_blink_start_time;
        if (elapsed < BLINK_DURATION) {
            float progress = (float)elapsed / BLINK_DURATION;
            eye_h *= (1.0f - sin(progress * PI)); // Appliquer le clignotement sur la taille actuelle
        } else {
            indicator_state = INDICATOR_IDLE;
            indicator_last_blink_time = millis();
            indicator_next_blink = random(3000, 10000); // Prochain clignotement
        }
    }
    
    int16_t eye_r = constrain(eye_h / 3, 2, 12);

    // Dessiner les deux yeux avec la couleur de la prochaine balle
    canvas.fillRoundRect(x - eye_w - spacing/2, y - eye_h/2, eye_w, eye_h, eye_r, patient_color);
    canvas.fillRoundRect(x + spacing/2, y - eye_h/2, eye_w, eye_h, eye_r, patient_color);

    if (eye_h > base_pupil_h) {
        draw_indicator_diffuse_pupil(canvas, x - base_eye_w/2 - spacing/2, y, base_pupil_w, patient_color);
        draw_indicator_diffuse_pupil(canvas, x + base_eye_w/2 + spacing/2, y, base_pupil_w, patient_color);
    }

    if (angry_progress > 0.01f) {
        int16_t frown_depth = eye_h * 0.8f * angry_progress; // La profondeur du froncement augmente avec l'impatience
        
        // Sourcil gauche
        int16_t left_eye_top = y - eye_h/2;
        int16_t left_eye_left = x - eye_w - spacing/2;
        canvas.fillTriangle(left_eye_left, left_eye_top, left_eye_left + eye_w, left_eye_top, left_eye_left + eye_w, left_eye_top + frown_depth, TFT_BLACK);

        // Sourcil droit
        int16_t right_eye_top = y - eye_h/2;
        int16_t right_eye_left = x + spacing/2;
        canvas.fillTriangle(right_eye_left, right_eye_top, right_eye_left + eye_w, right_eye_top, right_eye_left, right_eye_top + frown_depth, TFT_BLACK);
    }
}


void spawn_particles(float x, float y, uint16_t color) {
    int particles_to_spawn = 30; // Augmenté pour une explosion plus dense
    for (int i = 0; i < MAX_PARTICLES && particles_to_spawn > 0; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].px = x;
            particles[i].py = y;
            
            float angle = random(360) * DEG_TO_RAD;
            float speed = random(150, 400); // Vitesse d'explosion
            particles[i].vx = cos(angle) * speed;
            particles[i].vy = sin(angle) * speed;
            
            particles[i].lifespan = random(40, 70); // Durée de vie en frames
            particles[i].color = color;
            
            particles_to_spawn--;
        }
    }
    // Déclencher la secousse d'écran
    screen_shake_frames = 5; // Secouer pendant 5 frames
}

void reset_game() {
    // Réinitialiser toutes les balles
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
    }
    // Réinitialiser les particules
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].active = false;
    }
    // Réinitialiser le score et l'état du jeu
    score = 0;
    current_game_state = PLAYING;
    last_match_time = millis(); // Réinitialiser le minuteur de patience

    // Créer la première balle
    set_ball_position(tft.width() / 2.0f, tft.height() / 2.0f);
    Serial.println("Nouvelle partie !");
}

void draw_ball_animation(float gravity_vec[3]) {
    static bool initial_ball_created = false;
    if (!initial_ball_created) {
        // Choisir la première couleur "suivante"
        int num_colors = sizeof(ball_colors) / sizeof(uint16_t);
        last_match_time = millis(); // Démarrer le minuteur
        load_high_score();
        next_ball_color = ball_colors[random(num_colors)];
        precompute_indicator_pupil_mask(); // Pré-calculer le masque une seule fois
        set_ball_position(tft.width() / 2.0f, tft.height() / 2.0f);

        initial_ball_created = true;
    }

    // Si l'état est GAME OVER, on affiche l'écran de fin et on attend un contact pour recommencer
    if (current_game_state == GAME_OVER) {
        static TFT_eSprite canvas = TFT_eSprite(&tft);
        static bool canvas_created = false;
        if (!canvas_created) {
            canvas.createSprite(tft.width(), tft.height());
            canvas_created = true;
        }
        canvas.fillSprite(TFT_BLACK);
        canvas.setTextDatum(MC_DATUM);
        canvas.setTextColor(TFT_WHITE);
        canvas.setTextSize(4);
        canvas.drawString("GAME OVER", tft.width() / 2, tft.height() / 2 - 40);

        const int16_t go_eye_w = 100;
        const int16_t go_eye_h = 135;
        const int16_t go_spacing = 20;
        canvas.fillRoundRect(tft.width()/2 - go_spacing/2 - go_eye_w, tft.height()/2 - go_eye_h/2, go_eye_w, go_eye_h, 20, TFT_RED);
        canvas.fillRoundRect(tft.width()/2 + go_spacing/2, tft.height()/2 - go_eye_h/2, go_eye_w, go_eye_h, 20, TFT_RED);
        canvas.fillEllipse(tft.width()/2 - go_spacing/2 - go_eye_w/2, tft.height()/2, 25, 40, TFT_BLACK);
        canvas.fillEllipse(tft.width()/2 + go_spacing/2 + go_eye_w/2, tft.height()/2, 25, 40, TFT_BLACK);

        // Afficher le score par-dessus
        canvas.setTextColor(TFT_WHITE, TFT_BLACK); // Texte blanc sur fond noir pour la lisibilité
        canvas.setTextSize(2);
        canvas.drawString("Score: " + String(score), tft.width() / 2, tft.height() / 2 - 15);
        canvas.drawString("Meilleur: " + String(high_score), tft.width() / 2, tft.height() / 2 + 15);
        canvas.setTextSize(1);
        canvas.drawString("Touchez pour rejouer", tft.width() / 2, tft.height() - 20);
        canvas.pushSprite(0, 0);
        return;
    }

    // --- Boucle Physique à Pas de Temps Fixe ---
    physics_update_fixed(gravity_vec,
                         balls,
                         MAX_BALLS,
                         particles,
                         MAX_PARTICLES,
                         tft.width(),
                         tft.height(),
                         current_game_state == MENU);

    // ---Détection de Game Over par minuteur de patience ---
    if (current_game_mode == MODE_GAME) {
        if (current_game_state == PLAYING && millis() - last_match_time > MATCH_TIMER_DURATION) {
            current_game_state = GAME_OVER;
            save_high_score(); // Sauvegarder le score final
            Serial.println("GAME OVER - Temps écoulé !");
        }
    }
    
    // ---Logique de jeu avec score (Algorithme de recherche de groupes) ---
    bool to_deactivate[MAX_BALLS] = {false};
    bool visited[MAX_BALLS] = {false};
    unsigned long frame_score = 0;

    // 1. Détecter les groupes de couleur et calculer le score
    for (int i = 0; i < MAX_BALLS; i++) {
        if (current_game_mode == MODE_GAME && balls[i].active && !visited[i] && !balls[i].is_explosive) {
            std::vector<int> current_group;
            std::stack<int> to_visit;

            to_visit.push(i);
            visited[i] = true;

            while (!to_visit.empty()) {
                int current_idx = to_visit.top();
                to_visit.pop();
                current_group.push_back(current_idx);

                // Trouver les voisins de même couleur non visités
                for (int j = 0; j < MAX_BALLS; j++) {
                    if (balls[j].active && !visited[j] && !balls[j].is_explosive && balls[j].color == balls[i].color) {
                        float dx = balls[j].px - balls[current_idx].px;
                        float dy = balls[j].py - balls[current_idx].py;
                        if (dx * dx + dy * dy < (BALL_RADIUS * 2.1f) * (BALL_RADIUS * 2.1f)) {
                            to_visit.push(j);
                            visited[j] = true;
                        }
                    }
                }
            }

            // 2. Si le groupe est assez grand, le marquer pour suppression et calculer les points
            if (current_group.size() >= 3) {
                for (int ball_idx : current_group) {
                    to_deactivate[ball_idx] = true;
                }
                // Points de base pour 3 balles
                frame_score += 100;
                // Points bonus pour chaque balle supplémentaire
                if (current_group.size() > 3) {
                    frame_score += (current_group.size() - 3) * 100; // 100 points par balle en plus
                }
            }
        }
    }

    // 3. Gérer la bombe à retardement
    const unsigned long BOMB_FUSE_TIME = 2000; // 2 secondes de mèche
    if (current_game_mode == MODE_GAME) {
        for (int i = 0; i < MAX_BALLS; i++) {
            if (balls[i].active && balls[i].is_explosive && balls[i].armed_time > 0) {
                if (millis() - balls[i].armed_time > BOMB_FUSE_TIME) {
                    to_deactivate[i] = true; // Marquer la bombe pour explosion
                    
                    // Marquer toutes les balles dans le rayon et compter les points bonus
                    const float explosion_radius_sq = (BALL_RADIUS * 3.0f) * (BALL_RADIUS * 3.0f);
                    for (int j = 0; j < MAX_BALLS; j++) {
                        if (i == j || !balls[j].active) continue;
                        float dx = balls[j].px - balls[i].px;
                        float dy = balls[j].py - balls[i].py;
                        if ((dx * dx + dy * dy) < explosion_radius_sq) {
                            if (!to_deactivate[j]) { // Ne pas compter deux fois les points
                                to_deactivate[j] = true;
                                frame_score += 50; // 50 points bonus par balle détruite par la bombe
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 4. Désactiver toutes les balles marquées et ajouter le score
    score += frame_score;
    if (frame_score > 0) {
        last_match_time = millis(); // Réinitialiser le minuteur de patience !
        save_high_score(); // Sauvegarder si le score a changé
    }
    for (int i = 0; i < MAX_BALLS; i++) {
        if (to_deactivate[i]) {
            deactivate_ball(i, true); // true pour déclencher l'explosion
        }
    }

    // --- Rendu final (Double Buffering plein écran pour 0 scintillement) ---
    static TFT_eSprite canvas = TFT_eSprite(&tft);
    static bool canvas_created = false;
    if (!canvas_created) {
        canvas.createSprite(tft.width(), tft.height());
        canvas_created = true;
    }

    canvas.fillSprite(TFT_BLACK); 
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            // Dessiner des carrés 2x2 pour des particules plus visibles
            canvas.fillRect(particles[i].px, particles[i].py, 2, 2, particles[i].color);
        }
    }

    for (int i = 0; i < MAX_BALLS; i++) {
        if (!balls[i].active) continue;

        float squash_factor = 1.0f - constrain(balls[i].squash_v / 3000.0f, 0.0f, 0.3f);
        float stretch_factor = 1.0f / squash_factor;
        float radius_squash = BALL_RADIUS * squash_factor;
        float radius_stretch = BALL_RADIUS * stretch_factor;

        // Calcul de l'angle de rotation à partir de la normale de l'impact
        float angle_rad = atan2(balls[i].squash_ny, balls[i].squash_nx);

        // Rendu avec une ellipse pivotée
        uint16_t ball_color = balls[i].is_explosive ? TFT_WHITE : balls[i].color;
        draw_rotated_ellipse(canvas, (int16_t)balls[i].px, (int16_t)balls[i].py, (int16_t)radius_squash, (int16_t)radius_stretch, angle_rad, ball_color);

        // Rendu du reflet ou du noyau de la bombe
        if (balls[i].is_explosive) {
            float core_radius;
            if (balls[i].armed_time > 0) {
                // Le pouls s'accélère quand la bombe est armée
                float elapsed = millis() - balls[i].armed_time;
                float pulse_freq = 0.015f + (elapsed / BOMB_FUSE_TIME) * 0.08f; // Fréq augmente de 0.015 à ~0.095
                float pulse = (sin(millis() * pulse_freq) + 1.0f) / 2.0f;
                core_radius = BALL_RADIUS * 0.3f + pulse * (BALL_RADIUS * 0.3f);
            } else {
                core_radius = BALL_RADIUS * 0.3f; // Noyau stable si non armée
            }
            canvas.fillEllipse((int16_t)balls[i].px, (int16_t)balls[i].py, core_radius, core_radius, TFT_BLACK);
        } else {
            // Reflet blanc pour les balles normales
            canvas.fillEllipse((int16_t)(balls[i].px - BALL_RADIUS * 0.3f), (int16_t)(balls[i].py - BALL_RADIUS * 0.3f), (int16_t)(BALL_RADIUS * 0.2f), (int16_t)(BALL_RADIUS * 0.15f), TFT_WHITE);
        }
    }
    if (current_game_mode == MODE_GAME) {
        draw_next_ball_indicator_eyes(canvas, tft.width() - 35, 55, next_ball_color);

        canvas.setTextDatum(TR_DATUM);
        canvas.setTextColor(TFT_LIGHTGREY); // Couleur discrète pour le meilleur score
        canvas.setTextSize(1);
        canvas.drawString("HI " + String(high_score), tft.width() - 5, 5);

        canvas.setTextDatum(TC_DATUM);
        canvas.setTextColor(TFT_WHITE);
        canvas.setTextSize(2);
        canvas.drawNumber(score, tft.width() / 2, 5);
        canvas.setTextDatum(TL_DATUM); // Rétablir l'alignement par défaut
    }

    draw_ui(&canvas);

    if (current_game_state == MENU) {
        draw_menu(canvas);
    }

    if (screen_shake_frames > 0) {
        tft.setAddrWindow(0, 0, tft.width(), tft.height()); // Important pour setOffset
        tft.pushImage(random(-SHAKE_INTENSITY, SHAKE_INTENSITY + 1), random(-SHAKE_INTENSITY, SHAKE_INTENSITY + 1), tft.width(), tft.height(), (uint16_t*)canvas.getPointer());
        screen_shake_frames--;
    } else {
        canvas.pushSprite(0, 0);
    }
}


int set_ball_position(float x, float y, int ball_id /* = -1 */, bool* was_created /* = nullptr */) {
    const float grab_radius = 40.0f; // Rayon de saisie en pixels
    const float grab_radius_sq = grab_radius * grab_radius; // Comparer les carrés est plus rapide
    
    if (ball_id != -1) { // Mettre à jour une balle existante (pendant le glissement)
        if (ball_id >= 0 && ball_id < MAX_BALLS) {
            balls[ball_id].px = x;
            balls[ball_id].py = y;
            balls[ball_id].vx = 0.0f;
            balls[ball_id].vy = 0.0f;
            return ball_id;
        }
    } else { // Premier contact : chercher une balle à saisir ou en créer une
        // 1. Chercher une balle existante à proximité
        for (int i = 0; i < MAX_BALLS; i++) {
            if (balls[i].active) {
                float dx = balls[i].px - x;
                float dy = balls[i].py - y;
                if ((dx * dx + dy * dy) < grab_radius_sq) {
                    // Balle trouvée ! On met à jour sa position et on retourne son ID.
                    if (was_created) *was_created = false;
                    balls[i].px = x; balls[i].py = y;
                    balls[i].vx = 0.0f; balls[i].vy = 0.0f;
                    return i; // Balle trouvée ! On retourne son ID.
                }
            }
        }

        // 2. Si aucune balle n'est trouvée, chercher un emplacement pour en créer une nouvelle
        for (int i = 0; i < MAX_BALLS; i++) {
            if (!balls[i].active) { // Emplacement libre trouvé
                if (was_created) *was_created = true;
                balls[i].px = x; balls[i].py = y;
                balls[i].vx = 0.0f; balls[i].vy = 0.0f;
                balls[i].active = true;
                balls[i].squash_v = 0.0f;
                balls[i].squash_nx = 0.0f; balls[i].squash_ny = 1.0f;
                balls[i].creation_time = millis();
                balls[i].armed_time = 0; // S'assurer que la bombe n'est pas armée à la création

                if (current_game_mode == MODE_GAME && random(10) == 0) {
                    balls[i].is_explosive = true;
                    balls[i].color = TFT_WHITE; // Couleur de base pour la logique de groupe
                } else {
                    balls[i].is_explosive = false;
                    // Assigner la couleur "suivante"
                    balls[i].color = next_ball_color;
                }
                
                // Choisir une nouvelle couleur pour la prochaine balle
                int num_colors = sizeof(ball_colors) / sizeof(uint16_t);
                next_ball_color = ball_colors[random(num_colors)]; // La prochaine ne sera jamais une bombe
                return i; // Retourner l'ID de la nouvelle balle
            }
        }
    }
    return -1; // Pas de place disponible ou ID invalide
}

void arm_bomb(int ball_id) {
    if (ball_id >= 0 && ball_id < MAX_BALLS) {
        const unsigned long ARMING_DELAY = 500; // 0.5s de délai avant de pouvoir armer
        if (balls[ball_id].is_explosive && balls[ball_id].armed_time == 0 && millis() - balls[ball_id].creation_time > ARMING_DELAY) {
            balls[ball_id].armed_time = millis();
            Serial.println("Bombe armée !");
        }
    }
}

/**
 * @brief Retourne le score actuel.
 */
unsigned long get_score() {
    return score;
}

void set_ball_velocity(int ball_id, float new_vx, float new_vy) {
    if (ball_id >= 0 && ball_id < MAX_BALLS) {
        balls[ball_id].vx = new_vx;
        balls[ball_id].vy = new_vy;
    }
}

void deactivate_ball(int ball_id, bool create_explosion) {
    if (ball_id >= 0 && ball_id < MAX_BALLS) {
        if (balls[ball_id].active) {
            if (create_explosion) spawn_particles(balls[ball_id].px, balls[ball_id].py, balls[ball_id].color);
            balls[ball_id].active = false;
        }
    }
}