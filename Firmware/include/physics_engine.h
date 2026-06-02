#pragma once

#include <stdint.h>
#include <math.h>

// Constantes de physique (cohérentes avec le rendu squash & stretch).
static constexpr float BALL_RADIUS = 35.0f;
static constexpr float RESTITUTION = 0.7f;
static constexpr float AIR_FRICTION = 0.99f;
static constexpr float TIMESTEP = 1.0f / 100.0f;
static constexpr int SUB_STEPS = 2;
static constexpr float GRAVITY_FORCE = 1800.0f;
static constexpr float INERTIA_SENSITIVITY = 5.0f;

struct Ball {
    float px, py;
    float vx, vy;
    float squash_nx, squash_ny, squash_v;
    unsigned long creation_time = 0;
    unsigned long armed_time = 0;
    bool is_explosive = false;
    bool active = false;
    uint16_t color;

    // Champs laissés pour compatibilité (optimisations de rendu potentielles).
    int16_t last_draw_x = 0, last_draw_y = 0, last_draw_w = 0, last_draw_h = 0;
};

struct Particle {
    float px, py;
    float vx, vy;
    int lifespan;
    bool active = false;
    uint16_t color;
};

// Résolution collision balle-balle en impulsion 2D (masse unitaire).
inline void resolve_collision(Ball &b1, Ball &b2) {
    float dx = b2.px - b1.px;
    float dy = b2.py - b1.py;
    float dist_sq = dx * dx + dy * dy;
    float radius_sum = BALL_RADIUS * 2.0f;

    // Si collision (et éviter divisions instables)
    if (dist_sq < radius_sum * radius_sum && dist_sq > 0.0001f) {
        float dist = sqrtf(dist_sq);
        float nx = dx / dist;
        float ny = dy / dist;

        // ÉTAPE 1 : correction de position (anti-enfoncement)
        float overlap = radius_sum - dist;
        float percent = 0.8f;
        float correction = (overlap * percent) / 2.0f;

        b1.px -= nx * correction;
        b1.py -= ny * correction;
        b2.px += nx * correction;
        b2.py += ny * correction;

        // ÉTAPE 2 : résolution d'impulsion
        float rvx = b2.vx - b1.vx;
        float rvy = b2.vy - b1.vy;

        float vel_along_normal = rvx * nx + rvy * ny;

        // Ne pas rebondir si les objets s'éloignent déjà
        if (vel_along_normal > 0.0f) return;

        // Impact -> squash
        float impact_strength = fabsf(vel_along_normal);
        b1.squash_v = fmaxf(b1.squash_v, impact_strength);
        b2.squash_v = fmaxf(b2.squash_v, impact_strength);
        b1.squash_nx = nx;
        b1.squash_ny = ny;
        b2.squash_nx = -nx;
        b2.squash_ny = -ny;

        // Calcul de l'impulsion (j)
        float j = -(1.0f + RESTITUTION) * vel_along_normal;
        j /= 2.0f; // 1/mass + 1/mass (masse unitaire)

        float impulse_x = j * nx;
        float impulse_y = j * ny;

        b1.vx -= impulse_x;
        b1.vy -= impulse_y;
        b2.vx += impulse_x;
        b2.vy += impulse_y;
    }
}

// Fixed-timestep update (physique + collisions + intégration particules).
void physics_update_fixed(const float gravity_vec[3],
                           Ball *balls,
                           int max_balls,
                           Particle *particles,
                           int max_particles,
                           int screen_w,
                           int screen_h,
                           bool pause_menu);

