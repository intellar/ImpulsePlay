#ifndef SMOOTHED_VALUE_H
#define SMOOTHED_VALUE_H

class SmoothedValue {
public:
    // Le facteur de lissage (alpha) détermine l'impact des nouvelles valeurs.
    // Une valeur plus petite donnera un lissage plus prononcé mais plus de latence.
    // Une valeur plus grande donnera un résultat plus réactif mais moins lissé.
    // Valeurs typiques : 0.05 à 0.2
    SmoothedValue(float alpha = 0.1f) : alpha(alpha), lastValue(0.0f), initialized(false) {}

    // Met à jour la valeur avec une nouvelle lecture brute et retourne la valeur lissée
    float update(float rawValue) {
        if (!initialized) {
            lastValue = rawValue;
            initialized = true;
        } else {
            // Application d'un filtre passe-bas simple (moyenne mobile exponentielle)
            lastValue = (alpha * rawValue) + (1.0f - alpha) * lastValue;
        }
        return lastValue;
    }

private:
    float alpha;
    float lastValue;
    bool initialized;
};

#endif // SMOOTHED_VALUE_H