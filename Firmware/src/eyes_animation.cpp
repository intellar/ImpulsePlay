#include "eyes_animation.h"
#include "SmoothedValue.h"
#include "ui_handler.h" // Pour pouvoir appeler draw_ui

// Déclarations externes pour les objets globaux
extern TFT_eSPI tft;
extern TFT_eSprite eyes_spr;

// --- Variables internes au module des yeux ---
static int16_t current_eye_center_x_tft;
static int16_t current_eye_center_y_tft;

const int PUPIL_MASK_SIZE = 32;
float pupil_mask[PUPIL_MASK_SIZE][PUPIL_MASK_SIZE];

bool laser_target_active = false;
static uint16_t laser_target_x = 0;
static uint16_t laser_target_y = 0;
static unsigned long laser_anim_start_time = 0;
const unsigned int LASER_CHARGE_DELAY = 250;
const unsigned int LASER_ANIM_DURATION = 600;

enum EyeAnimationState { IDLE, BLINKING, LOOK_AROUND };
static EyeAnimationState eye_anim_state = IDLE;
static unsigned long last_blink_time = 0;
static unsigned long next_blink_interval = 5000;
static unsigned long blink_start_time = 0;

static SmoothedValue smoothed_tilt_x(0.35f);
static SmoothedValue smoothed_yaw(0.25f);
static SmoothedValue smoothed_deform_factor(0.2f);
static SmoothedValue smoothed_tilt_y(0.25f);
static SmoothedValue smoothed_eyelid(0.25f);

static int16_t idle_wobble_x = 0;
static int16_t idle_wobble_y = 0;
static unsigned long last_wobble_time = 0;
const unsigned long WOBBLE_INTERVAL = 300;
const int WOBBLE_MAX_SHIFT = 2;

static bool is_dizzy = false;
static unsigned long dizzy_end_time = 0;
const float DIZZY_TRIGGER_YAW_SPEED = 4.0f;

enum IdleLookAroundState { S_IDLE, S_MOVING_OUT, S_PAUSING, S_MOVING_IN };
static IdleLookAroundState idle_look_state = S_IDLE;
static unsigned long last_activity_time = 0;
const unsigned long IDLE_ANIM_TRIGGER_TIME = 8000;
static int idle_anim_step = 0;
static int idle_anim_direction = 1;
const int IDLE_ANIM_STEPS = 5;
const int IDLE_ANIM_PAUSE_MS = 1500;
static unsigned long idle_anim_pause_end_time = 0;

// Déclaration anticipée (forward declaration) pour que init_eyes_animation puisse l'utiliser.
void precompute_pupil_mask(float sigma);


void init_eyes_animation() {
    // Pré-calculer le masque de la pupille au démarrage
    precompute_pupil_mask(PUPIL_MASK_SIZE / 2.5f);
    // Initialiser la position Y du centre des yeux
    current_eye_center_y_tft = (tft.height() - eyes_spr.height()) / 2 + eyes_spr.height() / 2;
}

void precompute_pupil_mask(float sigma) {
    const float center = (PUPIL_MASK_SIZE - 1) / 2.0f;
    const float effect_radius = (PUPIL_MASK_SIZE - 1) / 2.0f;

    for (int y = 0; y < PUPIL_MASK_SIZE; ++y) {
        for (int x = 0; x < PUPIL_MASK_SIZE; ++x) {
            const float dx = x - center;
            const float dy = y - center;
            const float distance = sqrt(dx * dx + dy * dy);

            if (distance >= effect_radius) {
                pupil_mask[y][x] = 1.0f;
            } else {
                float fade = 1.0f - (distance / effect_radius);
                fade = fade * fade;
                pupil_mask[y][x] = 1.0f + 0.5f * fade;
            }
        }
    }
}

void draw_eyes(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]) {
    eyes_spr.fillSprite(TFT_BLACK);

    const int16_t REF_EYE_W = 100; // Encore plus grands
    const int16_t REF_EYE_H = 135; // Encore plus grands
    const int16_t REF_EYE_R = 15;
    int16_t spacing = 10;

    const int SQUINT_AMOUNT = 60;
    const int GROW_AMOUNT_W = 55;
    const int GROW_AMOUNT_H = 25;    
    
    const uint16_t IRIS_COLOR = tft.color565(0, 200, 100);
    const int16_t BASE_PUPIL_DRAW_RADIUS = 30;
    const int16_t MAX_PUPIL_GROWTH_RADIUS = 5;
    const int MAX_PUPIL_SHIFT = 15;

    int16_t cx = eyes_spr.width() / 2;
    int16_t cy = eyes_spr.height() / 2;
    
    float tilt_x = smoothed_tilt_x.update(gravity_vec[0]);

    const int max_shift = (eyes_spr.width() - (2 * REF_EYE_W + spacing)) / 2;

    const float max_tilt_observed = 0.707f; 
    cx += constrain((tilt_x / max_tilt_observed) * max_shift, -max_shift, max_shift);
    
    bool is_stable = abs(tilt_x) <= 0.15 && abs(gyro_rad_s[0]) < 0.2 && abs(gyro_rad_s[1]) < 0.2;
    
    if (!is_stable) {
        last_activity_time = millis();
        if (idle_look_state != S_IDLE) {
            idle_look_state = S_IDLE;
            idle_anim_step = 0;
        }
    } else {
        if (eye_anim_state == IDLE && idle_look_state == S_IDLE && millis() - last_activity_time > IDLE_ANIM_TRIGGER_TIME) {
            idle_look_state = S_MOVING_OUT;
            idle_anim_step = 0;
            idle_anim_direction = (random(0, 2) == 0) ? -1 : 1;
            last_activity_time = millis();
        }
    }
    
    if (is_stable && eye_anim_state == IDLE && idle_look_state == S_IDLE && millis() - last_blink_time > next_blink_interval) {
        eye_anim_state = BLINKING;
        blink_start_time = millis();
    }

    int16_t base_eye_h = REF_EYE_H;
    int16_t base_eye_w = REF_EYE_W;

    if (eye_anim_state == BLINKING) {
        const unsigned long BLINK_DURATION = 250; // Durée totale d'un clignotement en ms
        unsigned long elapsed = millis() - blink_start_time;
        if (elapsed < BLINK_DURATION) {
            float blink_progress = (float)elapsed / BLINK_DURATION;
            base_eye_h = (1.0f - sin(blink_progress * PI)) * REF_EYE_H;
        } else {
            eye_anim_state = IDLE;
            last_blink_time = millis();
            next_blink_interval = random(2000, 8000);
        }
    }

    if (abs(gyro_rad_s[2]) > DIZZY_TRIGGER_YAW_SPEED) {
        is_dizzy = true;
        dizzy_end_time = millis() + 2000;
    }
    if (is_dizzy && millis() > dizzy_end_time) {
        is_dizzy = false;
    }

    if (is_dizzy) {
        idle_wobble_x = random(-WOBBLE_MAX_SHIFT * 3, WOBBLE_MAX_SHIFT * 3 + 1); 
        idle_wobble_y = random(-WOBBLE_MAX_SHIFT * 3, WOBBLE_MAX_SHIFT * 3 + 1);
    } else if (eye_anim_state == IDLE && millis() - last_wobble_time > WOBBLE_INTERVAL) {
        idle_wobble_x = random(-WOBBLE_MAX_SHIFT, WOBBLE_MAX_SHIFT + 1); 
        idle_wobble_y = random(-WOBBLE_MAX_SHIFT, WOBBLE_MAX_SHIFT + 1);
        last_wobble_time = millis();
    } else if (eye_anim_state != IDLE) {
        idle_wobble_x = 0;
        idle_wobble_y = 0; 
    }
    
    const float max_gyro_speed = 1.5f; 
    float gyro_factor = gyro_rad_s[1] / max_gyro_speed;
    float tilt_factor = tilt_x / max_tilt_observed; 
    float target_deform_factor = constrain(gyro_factor + tilt_factor, -1.0, 1.0);
    float deform_factor = smoothed_deform_factor.update(target_deform_factor);

    // Déclarer les variables de taille des yeux ici
    int16_t left_eye_h = base_eye_h, left_eye_w = base_eye_w;
    int16_t right_eye_h = base_eye_h, right_eye_w = base_eye_w;

    // Squash & stretch from IMU deformation
    if (deform_factor > 0) { // Mouvement vers la droite
        right_eye_w -= deform_factor * SQUINT_AMOUNT;
        right_eye_h += deform_factor * (GROW_AMOUNT_H * 0.5);
        left_eye_w += deform_factor * GROW_AMOUNT_W;
        left_eye_h -= deform_factor * (SQUINT_AMOUNT * 0.5);
    } else { // Mouvement vers la gauche
        float abs_deform = abs(deform_factor);
        left_eye_w -= abs_deform * SQUINT_AMOUNT;
        left_eye_h += abs_deform * (GROW_AMOUNT_H * 0.5);
        right_eye_w += abs_deform * GROW_AMOUNT_W;
        right_eye_h -= abs_deform * (SQUINT_AMOUNT * 0.5);
    }

    if (idle_look_state != S_IDLE) {
        float anim_progress = 0.0f;

        if (idle_look_state == S_MOVING_OUT) {
            idle_anim_step++;
            anim_progress = (float)idle_anim_step / IDLE_ANIM_STEPS;
            if (idle_anim_step >= IDLE_ANIM_STEPS) {
                idle_look_state = S_PAUSING;
                idle_anim_pause_end_time = millis() + IDLE_ANIM_PAUSE_MS;
            }
        } else if (idle_look_state == S_PAUSING) {
            anim_progress = 1.0;
            if (millis() > idle_anim_pause_end_time) {
                idle_look_state = S_MOVING_IN;
            }
        } else if (idle_look_state == S_MOVING_IN) {
            idle_anim_step--;
            anim_progress = (float)idle_anim_step / IDLE_ANIM_STEPS;
            if (idle_anim_step <= 0) {
                idle_look_state = S_IDLE;
                anim_progress = 0.0;
            }
        }

        cx += idle_anim_direction * anim_progress * (max_shift * 0.4);

        int16_t* target_eye_w = (idle_anim_direction > 0) ? &right_eye_w : &left_eye_w;
        int16_t* other_eye_h = (idle_anim_direction > 0) ? &left_eye_h : &right_eye_h;

        *target_eye_w += anim_progress * (GROW_AMOUNT_W * 0.3);
        *other_eye_h -= anim_progress * (SQUINT_AMOUNT * 0.3);
    }

    const float SQUASH_W_FACTOR = 40.0f;
    const float STRETCH_H_FACTOR = 30.0f;
    const float SPACING_REDUCTION_FACTOR = 8.0f;

    const int16_t WALL_INSET = 10;
    float squash_amount = 0.0f;
    float overflow = 0.0f;
    
    const int max_shift_allowed = (eyes_spr.width() - (2 * REF_EYE_W + spacing)) / 2;
    float shift_by_tilt = constrain((tilt_x / max_tilt_observed) * max_shift_allowed, -max_shift_allowed, max_shift_allowed);

    int16_t original_cx = eyes_spr.width() / 2;

    if (deform_factor > 0) {
        int16_t right_eye_outer_edge = original_cx + shift_by_tilt + spacing/2 + REF_EYE_W;
        int16_t wall_pos = eyes_spr.width() - WALL_INSET;
        overflow = right_eye_outer_edge - wall_pos;
    } else if (deform_factor < 0) {
        int16_t left_eye_outer_edge = original_cx + shift_by_tilt - spacing/2 - REF_EYE_W;
        int16_t wall_pos = WALL_INSET;
        overflow = wall_pos - left_eye_outer_edge;
    }

    float cx_imu = cx;

    if (overflow > 0) {
        squash_amount = constrain(overflow * 0.05f, 0.0f, 1.0f);

        int16_t* outer_eye_w = (deform_factor > 0) ? &right_eye_w : &left_eye_w;
        int16_t* outer_eye_h = (deform_factor > 0) ? &right_eye_h : &left_eye_h;

        spacing -= squash_amount * SPACING_REDUCTION_FACTOR;
        *outer_eye_w -= squash_amount * SQUASH_W_FACTOR;
        *outer_eye_h += squash_amount * STRETCH_H_FACTOR;

        float cx_wall;

        if (deform_factor > 0) {
            const int16_t wall_pos = eyes_spr.width() - WALL_INSET;
            cx_wall = wall_pos - (spacing / 2 + right_eye_w / 2);
        } else {
            const int16_t wall_pos = WALL_INSET;
            cx_wall = wall_pos + (spacing / 2 + left_eye_w / 2);
        }
        
        float blend_factor = constrain(overflow / 30.0f, 0.0f, 1.0f);
        cx = (cx_imu * (1.0f - blend_factor)) + (cx_wall * blend_factor);
    }

    int16_t left_eye_r = constrain(map(left_eye_h, 4, REF_EYE_H, 2, REF_EYE_R), 1, left_eye_h / 2);
    int16_t right_eye_r = constrain(map(right_eye_h, 4, REF_EYE_H, 2, REF_EYE_R), 1, right_eye_h / 2);
    
    const int16_t OUTER_MARGIN = 2;

    eyes_spr.fillRoundRect(cx - left_eye_w - spacing/2 - OUTER_MARGIN, 
                      cy - left_eye_h/2 - OUTER_MARGIN, 
                      left_eye_w + 2 * OUTER_MARGIN, 
                      left_eye_h + 2 * OUTER_MARGIN, 
                      left_eye_r + OUTER_MARGIN, 
                      TFT_BLACK); 
    
    eyes_spr.fillRoundRect(cx + spacing/2 - OUTER_MARGIN, 
                      cy - right_eye_h/2 - OUTER_MARGIN, 
                      right_eye_w + 2 * OUTER_MARGIN, 
                      right_eye_h + 2 * OUTER_MARGIN, 
                      right_eye_r + OUTER_MARGIN, 
                      TFT_BLACK);

    eyes_spr.fillRoundRect(cx - left_eye_w - spacing/2, cy - left_eye_h/2, left_eye_w, left_eye_h, left_eye_r, IRIS_COLOR);
    eyes_spr.fillRoundRect(cx + spacing/2, cy - right_eye_h/2, right_eye_w, right_eye_h, right_eye_r, IRIS_COLOR);

    float tilt_y = smoothed_tilt_y.update(gravity_vec[2]);    
    const float MAX_LOOK_FACTOR = 0.5f; 
    float look_x_factor = constrain(tilt_x, -MAX_LOOK_FACTOR, MAX_LOOK_FACTOR); 
    float look_y_factor = constrain(tilt_y, -MAX_LOOK_FACTOR, MAX_LOOK_FACTOR); 

    int16_t pupil_shift_x = (int16_t)(look_x_factor * MAX_PUPIL_SHIFT * 1.5f) + idle_wobble_x;
    int16_t pupil_shift_y = (int16_t)(look_y_factor * MAX_PUPIL_SHIFT) + idle_wobble_y;
    
    int16_t center_left_eye_x = cx - (spacing/2) - (left_eye_w / 2); 
    int16_t center_right_eye_x = cx + (spacing/2) + (right_eye_w / 2); 
    int16_t center_y = cy;

    float pupil_dilation_factor = 1.0f - abs(deform_factor);
    int16_t dynamic_pupil_radius = BASE_PUPIL_DRAW_RADIUS + (int16_t)((pupil_dilation_factor) * MAX_PUPIL_GROWTH_RADIUS);

    auto draw_diffuse_pupil = [&](int16_t center_x, int16_t center_y, int16_t pupil_draw_radius, int16_t eye_box_x, int16_t eye_box_y, int16_t eye_box_w, int16_t eye_box_h) {
        int16_t draw_size = pupil_draw_radius * 2;
        int16_t start_x = center_x - pupil_draw_radius;
        int16_t start_y = center_y - pupil_draw_radius;
        
        for (int y = 0; y < draw_size; ++y) {
            for (int x = 0; x < draw_size; ++x) {
                int16_t px = start_x + x;
                int16_t py = start_y + y;

                if (px < eye_box_x || px >= eye_box_x + eye_box_w || py < eye_box_y || py >= eye_box_y + eye_box_h) {
                    continue;
                }

                if (eyes_spr.readPixel(px, py) != IRIS_COLOR) continue;

                int mask_x = map(x, 0, draw_size, 0, PUPIL_MASK_SIZE - 1);
                int mask_y = map(y, 0, draw_size, 0, PUPIL_MASK_SIZE - 1);

                float brightening_factor = pupil_mask[mask_y][mask_x];
                if (brightening_factor <= 1.01f) continue;
                
                float contrast_boost = is_dizzy ? 2.0f : 1.5f;
                brightening_factor = 1.0f + (brightening_factor - 1.0f) * contrast_boost;

                uint8_t r_base = (IRIS_COLOR & 0xF800) >> 8;
                uint8_t g_base = (IRIS_COLOR & 0x07E0) >> 3;
                uint8_t b_base = (IRIS_COLOR & 0x001F) << 3;
                
                uint8_t r = constrain((int)(r_base * brightening_factor), 0, 255);
                uint8_t g = constrain((int)(g_base * brightening_factor), 0, 255);
                uint8_t b = constrain((int)(b_base * brightening_factor), 0, 255);

                eyes_spr.drawPixel(px, py, tft.color565(r, g, b));
            }
        }
    };

    draw_diffuse_pupil(center_left_eye_x - pupil_shift_x, center_y - pupil_shift_y, dynamic_pupil_radius, cx - left_eye_w - spacing/2, cy - left_eye_h/2, left_eye_w, left_eye_h);
    draw_diffuse_pupil(center_right_eye_x - pupil_shift_x, center_y - pupil_shift_y, dynamic_pupil_radius, cx + spacing/2, cy - right_eye_h/2, right_eye_w, right_eye_h);

    current_eye_center_x_tft = cx;

    const float max_pitch_speed = 2.5f;
    float target_look_factor = constrain(gyro_rad_s[0] / max_pitch_speed, -1.0, 1.0);
    float vertical_look_factor = smoothed_eyelid.update(target_look_factor);
} 

void draw_laser_animation(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]) {
    unsigned long elapsed = millis() - laser_anim_start_time;

    if (elapsed > LASER_ANIM_DURATION) {
        laser_target_active = false;
        return;
    }

    draw_eyes(final_x, final_y, final_z, gravity_vec, gyro_rad_s);

    bool laser_has_reached_target = false;
    if (elapsed >= LASER_CHARGE_DELAY) {
        unsigned long laser_elapsed = elapsed - LASER_CHARGE_DELAY;
        unsigned long laser_fire_duration = LASER_ANIM_DURATION - LASER_CHARGE_DELAY;
        float travel_progress = (float)laser_elapsed / laser_fire_duration;
        travel_progress = constrain(travel_progress, 0.0f, 1.0f);
        if (travel_progress >= 1.0f) {
            laser_has_reached_target = true;
        }
    }

    if (!laser_has_reached_target) {
        int16_t target_in_sprite_x = laser_target_x - (tft.width() - eyes_spr.width()) / 2;
        int16_t target_in_sprite_y = laser_target_y - (tft.height() - eyes_spr.height()) / 2;
        eyes_spr.fillCircle(target_in_sprite_x, target_in_sprite_y, 5, TFT_RED);
    }

    if (elapsed >= LASER_CHARGE_DELAY) {
        unsigned long laser_elapsed = elapsed - LASER_CHARGE_DELAY;
        unsigned long laser_fire_duration = LASER_ANIM_DURATION - LASER_CHARGE_DELAY;
        float travel_progress = (float)laser_elapsed / laser_fire_duration;
        travel_progress = constrain(travel_progress, 0.0f, 1.0f);

        const float LASER_DASH_LENGTH_FACTOR = 0.15f;
        float head_progress = travel_progress;
        float tail_progress = max(0.0f, travel_progress - LASER_DASH_LENGTH_FACTOR);

        uint16_t laser_color = TFT_RED;
        const float LASER_WIDTH = 3.0f;

        int16_t spacing = 10;
        int16_t left_eye_w = 60;
        int16_t right_eye_w = 60;
        int16_t center_left_eye_x = eyes_spr.width()/2 - (spacing/2) - (left_eye_w / 2); 
        int16_t center_right_eye_x = eyes_spr.width()/2 + (spacing/2) + (right_eye_w / 2); 
        
        auto draw_thick_projectile = [&](int16_t p1x, int16_t p1y, int16_t p2x, int16_t p2y, float width, uint16_t color) {
            float dx = p2x - p1x;
            float dy = p2y - p1y;
            float len = sqrt(dx * dx + dy * dy);
            if (len < 1.0f) return;

            float nx = -dy / len;
            float ny = dx / len;
            float half_w = width / 2.0f;

            int16_t x1 = p1x + nx * half_w;
            int16_t y1 = p1y + ny * half_w;
            int16_t x2 = p1x - nx * half_w;
            int16_t y2 = p1y - ny * half_w;
            int16_t x3 = p2x - nx * half_w;
            int16_t y3 = p2y - ny * half_w;
            int16_t x4 = p2x + nx * half_w;
            int16_t y4 = p2y + ny * half_w;

            eyes_spr.fillTriangle(x1, y1, x2, y2, x3, y3, color);
            eyes_spr.fillTriangle(x1, y1, x3, y3, x4, y4, color);
        };

        int16_t target_in_sprite_x = laser_target_x - (tft.width() - eyes_spr.width()) / 2;
        int16_t target_in_sprite_y = laser_target_y - (tft.height() - eyes_spr.height()) / 2;
        int16_t center_y = eyes_spr.height() / 2;

        draw_thick_projectile(center_left_eye_x + (target_in_sprite_x - center_left_eye_x) * tail_progress, center_y + (target_in_sprite_y - center_y) * tail_progress, center_left_eye_x + (target_in_sprite_x - center_left_eye_x) * head_progress, center_y + (target_in_sprite_y - center_y) * head_progress, LASER_WIDTH, laser_color);
        draw_thick_projectile(center_right_eye_x + (target_in_sprite_x - center_right_eye_x) * tail_progress, center_y + (target_in_sprite_y - center_y) * tail_progress, center_right_eye_x + (target_in_sprite_x - center_right_eye_x) * head_progress, center_y + (target_in_sprite_y - center_y) * head_progress, LASER_WIDTH, laser_color);
    }

    // Utiliser un canvas plein écran pour combiner l'animation et l'UI
    static TFT_eSprite canvas = TFT_eSprite(&tft);
    static bool canvas_created = false;
    if (!canvas_created) {
        canvas.createSprite(tft.width(), tft.height());
        canvas_created = true;
    }
    canvas.fillSprite(TFT_BLACK);
    eyes_spr.pushToSprite(&canvas, (tft.width() - eyes_spr.width()) / 2, (tft.height() - eyes_spr.height()) / 2);
    canvas.pushSprite(0, 0);
}

void draw_imu_axes(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], const float gyro_rad_s[3]) {
    draw_eyes(final_x, final_y, final_z, gravity_vec, gyro_rad_s);

    // Le rendu est maintenant géré par draw_laser_animation pour la cohérence
    // On peut considérer que draw_imu_axes est un cas particulier de l'animation laser sans laser.
    // Pour éviter la duplication de code, on appelle draw_laser_animation qui gère le canvas.
    draw_laser_animation(final_x, final_y, final_z, gravity_vec, gyro_rad_s);
}