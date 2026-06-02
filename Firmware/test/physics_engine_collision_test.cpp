// Tests natifs (host) du moteur physique.
// Garde ARDUINO: sur la cible ESP32, le framework Arduino fournit déjà main(),
// donc on neutralise ce fichier pour éviter un conflit de linkage avec `pio test`.
#ifndef ARDUINO

#include <cassert>
#include <cmath>
#include <initializer_list>

#include "../include/physics_engine.h"

static void test_head_on_collision() {
    Ball b1{};
    Ball b2{};

    // Même hauteur, collision frontale sur l'axe X.
    // b1 à gauche, b2 à droite, qui se rapprochent.
    b1.px = 0.0f;
    b1.py = 0.0f;
    b1.vx = 10.0f;
    b1.vy = 0.0f;
    b1.squash_v = 0.0f;

    b2.px = BALL_RADIUS * 2.0f - 1.0f; // légèrement en recouvrement
    b2.py = 0.0f;
    b2.vx = -10.0f;
    b2.vy = 0.0f;
    b2.squash_v = 0.0f;

    resolve_collision(b1, b2);

    // Impulsion: les vitesses doivent s'inverser en ~{e}-scaled.
    assert(b1.vx < 0.0f);
    assert(b2.vx > 0.0f);

    // squash_v doit capturer l'intensité d'impact (|vel_along_normal|).
    // Ici: vel_along_normal ~= -20 donc impact_strength ~= 20.
    assert(std::fabs(b1.squash_v - 20.0f) < 0.5f);
    assert(std::fabs(b2.squash_v - 20.0f) < 0.5f);

    // Normale attendue: b1 compresse vers +X, b2 vers -X.
    assert(std::fabs(b1.squash_nx - 1.0f) < 0.01f);
    assert(std::fabs(b1.squash_ny) < 0.01f);
    assert(std::fabs(b2.squash_nx + 1.0f) < 0.01f);
    assert(std::fabs(b2.squash_ny) < 0.01f);
}

static void test_separating_objects_no_bounce() {
    Ball b1{};
    Ball b2{};

    b1.px = 0.0f;
    b1.py = 0.0f;
    b1.vx = -10.0f; // b1 part vers la gauche
    b1.vy = 0.0f;
    b1.squash_v = 0.0f;

    b2.px = BALL_RADIUS * 2.0f - 1.0f;
    b2.py = 0.0f;
    b2.vx = 10.0f; // b2 part vers la droite
    b2.vy = 0.0f;
    b2.squash_v = 0.0f;

    float b1_vx_before = b1.vx;
    float b2_vx_before = b2.vx;

    resolve_collision(b1, b2);

    // Si vel_along_normal > 0, resolve_collision retourne avant d'appliquer impulsion/squash.
    assert(std::fabs(b1.vx - b1_vx_before) < 1e-6f);
    assert(std::fabs(b2.vx - b2_vx_before) < 1e-6f);
    assert(b1.squash_v == 0.0f);
    assert(b2.squash_v == 0.0f);
}

static void test_squash_stretch_conservation_radius_product() {
    auto clamp01_03 = [](float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 0.3f) return 0.3f;
        return v;
    };

    // On échantillonne plusieurs squash_v pour valider rx*ry ~= BALL_RADIUS^2.
    for (float squash_v : {0.0f, 300.0f, 1500.0f, 3000.0f, 6000.0f}) {
        float compress = clamp01_03(squash_v / 3000.0f);
        float squash_factor = 1.0f - compress;
        float stretch_factor = 1.0f / squash_factor;

        float rx = BALL_RADIUS * squash_factor;
        float ry = BALL_RADIUS * stretch_factor;

        float expected = BALL_RADIUS * BALL_RADIUS;
        assert(std::fabs(rx * ry - expected) < 1e-3f);
    }
}

int main() {
    test_head_on_collision();
    test_separating_objects_no_bounce();
    test_squash_stretch_conservation_radius_product();
    return 0;
}

#endif // ARDUINO

