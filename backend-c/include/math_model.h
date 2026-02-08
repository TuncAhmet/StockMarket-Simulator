#ifndef MATH_MODEL_H
#define MATH_MODEL_H

#include <stdbool.h>

// Geometric Brownian Motion parameters
typedef struct {
    double S0;      // Initial price
    double mu;      // Drift coefficient (annual return)
    double sigma;   // Volatility (annual)
    double dt;      // Time step in years (e.g., 1/252 for daily, 1/86400/252 for per-second)
    double current_price;
} GBMModel;

// GBM functions
GBMModel* gbm_create(double S0, double mu, double sigma, double dt);
void gbm_destroy(GBMModel* model);
double gbm_next_price(GBMModel* model);
void gbm_reset(GBMModel* model);

// Random number generation
void rng_seed(unsigned int seed);
double rng_uniform(void);           // U(0, 1)
double rng_normal(void);            // N(0, 1) using Box-Muller
double rng_normal_params(double mean, double stddev);

// Statistical utilities
double stats_mean(const double* data, int n);
double stats_variance(const double* data, int n);
double stats_stddev(const double* data, int n);

#endif // MATH_MODEL_H
