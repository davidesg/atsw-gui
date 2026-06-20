// weighting_utils.h
#ifndef WEIGHTING_UTILS_H
#define WEIGHTING_UTILS_H

// Declarar las funciones como static inline para evitar múltiples definiciones
static inline double geometric_weight(int lag, double decay_factor) {
    if (lag <= 0) return 1.0;
    return pow(decay_factor, lag - 1);
}

static inline double seasonal_weight(int lag, int s, double seasonal_strength) {
    if (s <= 1) return 1.0;

    // Retardos estacionales exactos
    if (lag % s == 0) {
        return seasonal_strength;
    }

    // Satélites estacionales (s±1, 2s±1, etc.)
    for (int m = 1; m * s <= lag + 2; m++) {
        int seasonal_lag = m * s;
        if (abs(lag - seasonal_lag) <= 2) {
            double distance = abs(lag - seasonal_lag);
            return seasonal_strength * pow(0.7, distance);
        }
    }

    return 1.0;
}

static inline double combined_weight(int lag, int s, double decay_factor, double seasonal_strength) {
    double geo = geometric_weight(lag, decay_factor);
    double seas = seasonal_weight(lag, s, seasonal_strength);
    return geo * seas;
}

#endif
