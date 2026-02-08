#include "math_model.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Thread-local RNG state (simple implementation)
static unsigned int rng_state = 0;
static int has_spare = 0;
static double spare_normal = 0.0;

void rng_seed(unsigned int seed) {
    rng_state = seed;
    has_spare = 0;
}

// Linear congruential generator for reproducibility
static unsigned int lcg_next(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7FFF;
}

double rng_uniform(void) {
    return (double)lcg_next() / 32767.0;
}

// Box-Muller transform for normal distribution
double rng_normal(void) {
    if (has_spare) {
        has_spare = 0;
        return spare_normal;
    }
    
    double u1, u2, s;
    do {
        u1 = 2.0 * rng_uniform() - 1.0;
        u2 = 2.0 * rng_uniform() - 1.0;
        s = u1 * u1 + u2 * u2;
    } while (s >= 1.0 || s == 0.0);
    
    double mul = sqrt(-2.0 * log(s) / s);
    spare_normal = u2 * mul;
    has_spare = 1;
    
    return u1 * mul;
}

double rng_normal_params(double mean, double stddev) {
    return mean + stddev * rng_normal();
}

// GBM model
GBMModel* gbm_create(double S0, double mu, double sigma, double dt) {
    GBMModel* model = (GBMModel*)malloc(sizeof(GBMModel));
    if (!model) return NULL;
    
    model->S0 = S0;
    model->mu = mu;
    model->sigma = sigma;
    model->dt = dt;
    model->current_price = S0;
    
    // Seed RNG if not already done
    if (rng_state == 0) {
        rng_seed((unsigned int)time(NULL));
    }
    
    return model;
}

void gbm_destroy(GBMModel* model) {
    free(model);
}

void gbm_reset(GBMModel* model) {
    if (model) {
        model->current_price = model->S0;
    }
}

// GBM formula: S(t+dt) = S(t) * exp((mu - sigma^2/2)*dt + sigma*sqrt(dt)*Z)
// where Z ~ N(0,1)
double gbm_next_price(GBMModel* model) {
    if (!model) return 0.0;
    
    double Z = rng_normal();
    double drift = (model->mu - 0.5 * model->sigma * model->sigma) * model->dt;
    double diffusion = model->sigma * sqrt(model->dt) * Z;
    
    model->current_price = model->current_price * exp(drift + diffusion);
    
    // Ensure price doesn't go negative
    if (model->current_price < 0.01) {
        model->current_price = 0.01;
    }
    
    return model->current_price;
}

// Statistical utilities
double stats_mean(const double* data, int n) {
    if (!data || n <= 0) return 0.0;
    
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    return sum / n;
}

double stats_variance(const double* data, int n) {
    if (!data || n <= 1) return 0.0;
    
    double mean = stats_mean(data, n);
    double sum_sq = 0.0;
    
    for (int i = 0; i < n; i++) {
        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    
    return sum_sq / (n - 1);  // Sample variance (Bessel's correction)
}

double stats_stddev(const double* data, int n) {
    return sqrt(stats_variance(data, n));
}
