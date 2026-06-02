#include "physics_engine.h"

#include <Arduino.h>

// Vecteur de gravité filtré (commun à toutes les balles)
static float g_filt[3] = {0.0f, 1.0f, 0.0f};

void physics_update_fixed(const float gravity_vec[3],
                           Ball *balls,
                           int max_balls,
                           Particle *particles,
                           int max_particles,
                           int screen_w,
                           int screen_h,
                           bool pause_menu) {
    // --- Boucle Physique à Pas de Temps Fixe ---
    static float time_accumulator = 0.0f;
    static unsigned long last_micros = 0;
    if (last_micros == 0) last_micros = micros();

    // Pause uniquement sur l'accumulation de temps (mimique le comportement original).
    if (!pause_menu) {
        unsigned long now = micros();
        time_accumulator += (now - last_micros) / 1000000.0f;
        last_micros = now;
    }

    if (time_accumulator > 0.1f) time_accumulator = 0.1f;

    // Filtrage gravité (EMA)
    const float GRAVITY_FILTER_ALPHA = 0.3f;
    g_filt[0] = g_filt[0] * (1.0f - GRAVITY_FILTER_ALPHA) + gravity_vec[0] * GRAVITY_FILTER_ALPHA;
    g_filt[1] = g_filt[1] * (1.0f - GRAVITY_FILTER_ALPHA) + gravity_vec[1] * GRAVITY_FILTER_ALPHA;
    float a_lin_x = gravity_vec[0] - g_filt[0];
    float a_lin_y = gravity_vec[1] - g_filt[1];

    // Mise à jour particules (gravité + friction + durée de vie)
    const float PARTICLE_GRAVITY = 30.0f;
    const float PARTICLE_FRICTION = 0.98f;
    for (int p = 0; p < max_particles; p++) {
        if (particles[p].active) {
            particles[p].vy += PARTICLE_GRAVITY;
            particles[p].vx *= PARTICLE_FRICTION;
            particles[p].vy *= PARTICLE_FRICTION;
            particles[p].lifespan--;
            if (particles[p].lifespan <= 0) particles[p].active = false;
        }
    }

    while (time_accumulator >= TIMESTEP) {
        float dt = TIMESTEP / SUB_STEPS;
        for (int step = 0; step < SUB_STEPS; step++) {
            // Avancer les particules une seule fois par sous-step
            for (int p = 0; p < max_particles; ++p) {
                if (particles[p].active) {
                    particles[p].px += particles[p].vx * dt;
                    particles[p].py += particles[p].vy * dt;
                }
            }

            for (int i = 0; i < max_balls; i++) {
                if (!balls[i].active) continue;

                // 1. Appliquer les forces (Gravité + Inertie)
                balls[i].vx += (g_filt[0] * GRAVITY_FORCE + a_lin_x * INERTIA_SENSITIVITY * GRAVITY_FORCE) * dt;
                balls[i].vy += (g_filt[1] * GRAVITY_FORCE + a_lin_y * INERTIA_SENSITIVITY * GRAVITY_FORCE) * dt;

                // 2. Friction de l'air
                balls[i].vx *= AIR_FRICTION;
                balls[i].vy *= AIR_FRICTION;

                // 3. Avancer la position
                balls[i].px += balls[i].vx * dt;
                balls[i].py += balls[i].vy * dt;

                // 4. Collisions avec les murs + squash
                if (balls[i].px < BALL_RADIUS) {
                    balls[i].px = BALL_RADIUS;
                    if (balls[i].vx < 0) {
                        balls[i].squash_nx = 1.0f;
                        balls[i].squash_ny = 0.0f;
                        float vel_along_normal = balls[i].vx * balls[i].squash_nx + balls[i].vy * balls[i].squash_ny;
                        balls[i].squash_v = fmaxf(balls[i].squash_v, fabsf(vel_along_normal));
                        balls[i].vx *= -RESTITUTION;
                    }
                } else if (balls[i].px > screen_w - BALL_RADIUS) {
                    balls[i].px = screen_w - BALL_RADIUS;
                    if (balls[i].vx > 0) {
                        balls[i].squash_nx = -1.0f;
                        balls[i].squash_ny = 0.0f;
                        float vel_along_normal = balls[i].vx * balls[i].squash_nx + balls[i].vy * balls[i].squash_ny;
                        balls[i].squash_v = fmaxf(balls[i].squash_v, fabsf(vel_along_normal));
                        balls[i].vx *= -RESTITUTION;
                    }
                }

                if (balls[i].py < BALL_RADIUS) {
                    balls[i].py = BALL_RADIUS;
                    if (balls[i].vy < 0) {
                        balls[i].squash_nx = 0.0f;
                        balls[i].squash_ny = 1.0f;
                        float vel_along_normal = balls[i].vx * balls[i].squash_nx + balls[i].vy * balls[i].squash_ny;
                        balls[i].squash_v = fmaxf(balls[i].squash_v, fabsf(vel_along_normal));
                        balls[i].vy *= -RESTITUTION;
                    }
                } else if (balls[i].py > screen_h - BALL_RADIUS) {
                    balls[i].py = screen_h - BALL_RADIUS;
                    if (balls[i].vy > 0) {
                        balls[i].squash_nx = 0.0f;
                        balls[i].squash_ny = -1.0f;
                        float vel_along_normal = balls[i].vx * balls[i].squash_nx + balls[i].vy * balls[i].squash_ny;
                        balls[i].squash_v = fmaxf(balls[i].squash_v, fabsf(vel_along_normal));
                        balls[i].vy *= -RESTITUTION;
                    }
                }

                // Amortir l'effet de squash
                balls[i].squash_v *= 0.9f;

                // 5. Collisions balle-balle
                for (int j = i + 1; j < max_balls; j++) {
                    if (balls[j].active) {
                        resolve_collision(balls[i], balls[j]);
                    }
                }
            }
        }

        time_accumulator -= TIMESTEP;
    }
}

