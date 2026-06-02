#ifndef BALL_ANIMATION_H
#define BALL_ANIMATION_H

#include <TFT_eSPI.h>

// --- Structure pour représenter une balle ---
// Note: La définition complète est dans le .cpp car elle n'est pas nécessaire à l'extérieur.

// Game state (shared with display_handler / ui_handler)
enum GameState { PLAYING, GAME_OVER, MENU };
extern GameState current_game_state;

enum GameMode { MODE_GAME, MODE_SIMULATION };
extern GameMode current_game_mode;

// --- Fonctions publiques du module d'animation des balles ---

/**
 * @brief Fonction principale pour dessiner et mettre à jour l'animation des balles.
 * @param gravity_vec Vecteur de gravité provenant de l'IMU.
 */
void draw_ball_animation(float gravity_vec[3]);

/**
 * @brief Crée une nouvelle balle à la position donnée ou met à jour une balle existante.
 * @return L'ID de la balle affectée, ou -1 si aucune action n'a été effectuée.
 */
int set_ball_position(float x, float y, int ball_id = -1, bool* was_created = nullptr);

/**
 * @brief Définit la vélocité d'une balle spécifique.
 */
void set_ball_velocity(int ball_id, float new_vx, float new_vy);

/**
 * @brief Arme une bombe si la balle spécifiée en est une.
 */
void arm_bomb(int ball_id);

/**
 * @brief Désactive une balle, la faisant disparaître de l'écran.
 * @param create_explosion Si true, déclenche une explosion de particules.
 */
void deactivate_ball(int ball_id, bool create_explosion = false);

/**
 * @brief Retourne le score actuel.
 */
unsigned long get_score();

/**
 * @brief Réinitialise l'état du jeu pour une nouvelle partie.
 */
void reset_game();

/**
 * @brief Réinitialise le meilleur score.
 */
void reset_high_score();

#endif // BALL_ANIMATION_H