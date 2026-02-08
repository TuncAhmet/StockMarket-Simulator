#include "unity/unity.h"
#include "../include/math_model.h"
#include <math.h>

#define NUM_SAMPLES 10000
#define TOLERANCE 0.1  // 10% tolerance for statistical tests

void setUp(void) {
    rng_seed(12345);  // Fixed seed for reproducibility
}

void tearDown(void) {
}

// Test RNG seeding
void test_rng_seed(void) {
    rng_seed(42);
    double val1 = rng_uniform();
    
    rng_seed(42);
    double val2 = rng_uniform();
    
    TEST_ASSERT_EQUAL_DOUBLE(val1, val2);
}

// Test uniform distribution range
void test_rng_uniform_range(void) {
    rng_seed(12345);
    
    for (int i = 0; i < 1000; i++) {
        double val = rng_uniform();
        TEST_ASSERT_TRUE(val >= 0.0);
        TEST_ASSERT_TRUE(val <= 1.0);
    }
}

// Test uniform distribution mean (should be ~0.5)
void test_rng_uniform_mean(void) {
    rng_seed(12345);
    
    double sum = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += rng_uniform();
    }
    
    double mean = sum / NUM_SAMPLES;
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.5, mean);
}

// Test normal distribution mean (should be ~0)
void test_rng_normal_mean(void) {
    rng_seed(12345);
    
    double sum = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += rng_normal();
    }
    
    double mean = sum / NUM_SAMPLES;
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.0, mean);
}

// Test normal distribution variance (should be ~1)
void test_rng_normal_variance(void) {
    rng_seed(12345);
    
    double samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = rng_normal();
    }
    
    double variance = stats_variance(samples, NUM_SAMPLES);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 1.0, variance);
}

// Test normal with parameters
void test_rng_normal_params(void) {
    rng_seed(12345);
    
    double mean_target = 100.0;
    double stddev_target = 15.0;
    
    double sum = 0.0;
    double samples[NUM_SAMPLES];
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = rng_normal_params(mean_target, stddev_target);
        sum += samples[i];
    }
    
    double mean = sum / NUM_SAMPLES;
    double variance = stats_variance(samples, NUM_SAMPLES);
    double stddev = sqrt(variance);
    
    TEST_ASSERT_DOUBLE_WITHIN(mean_target * TOLERANCE, mean_target, mean);
    TEST_ASSERT_DOUBLE_WITHIN(stddev_target * TOLERANCE, stddev_target, stddev);
}

// Test GBM model creation
void test_gbm_create(void) {
    GBMModel* model = gbm_create(100.0, 0.05, 0.2, 1.0/252.0);
    
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, model->S0);
    TEST_ASSERT_EQUAL_DOUBLE(0.05, model->mu);
    TEST_ASSERT_EQUAL_DOUBLE(0.2, model->sigma);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, model->current_price);
    
    gbm_destroy(model);
}

// Test GBM reset
void test_gbm_reset(void) {
    GBMModel* model = gbm_create(100.0, 0.05, 0.2, 1.0/252.0);
    
    // Evolve a bit
    for (int i = 0; i < 10; i++) {
        gbm_next_price(model);
    }
    
    TEST_ASSERT_NOT_EQUAL(100.0, model->current_price);
    
    gbm_reset(model);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, model->current_price);
    
    gbm_destroy(model);
}

// Test GBM produces positive prices
void test_gbm_positive_prices(void) {
    rng_seed(12345);
    GBMModel* model = gbm_create(100.0, 0.0, 0.3, 1.0/252.0);
    
    for (int i = 0; i < 10000; i++) {
        double price = gbm_next_price(model);
        TEST_ASSERT_TRUE(price > 0);
    }
    
    gbm_destroy(model);
}

// Test GBM drift properties (with positive drift, price should tend to increase)
void test_gbm_drift(void) {
    rng_seed(12345);
    
    // High positive drift, low volatility
    GBMModel* model = gbm_create(100.0, 0.5, 0.01, 1.0/252.0);
    
    // Simulate one year (252 trading days)
    for (int i = 0; i < 252; i++) {
        gbm_next_price(model);
    }
    
    // With 50% annual drift and 1% volatility, price should increase significantly
    TEST_ASSERT_GREATER_THAN(100.0, model->current_price);
    
    gbm_destroy(model);
}

// Test GBM expected value (log-normal property)
// E[S(t)] = S(0) * exp(mu * t)
void test_gbm_expected_value(void) {
    rng_seed(12345);
    
    double S0 = 100.0;
    double mu = 0.1;  // 10% annual drift
    double sigma = 0.2;  // 20% annual volatility
    double dt = 1.0 / 252.0;  // Daily step
    int steps = 252;  // One year
    
    double sum = 0.0;
    int num_paths = 1000;
    
    for (int path = 0; path < num_paths; path++) {
        GBMModel* model = gbm_create(S0, mu, sigma, dt);
        
        for (int i = 0; i < steps; i++) {
            gbm_next_price(model);
        }
        
        sum += model->current_price;
        gbm_destroy(model);
    }
    
    double observed_mean = sum / num_paths;
    double expected_mean = S0 * exp(mu * 1.0);  // t = 1 year
    
    // Allow 20% tolerance due to sampling
    TEST_ASSERT_DOUBLE_WITHIN(expected_mean * 0.2, expected_mean, observed_mean);
}

// Test stats_mean
void test_stats_mean(void) {
    double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double mean = stats_mean(data, 5);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, mean);
}

// Test stats_variance
void test_stats_variance(void) {
    double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double variance = stats_variance(data, 8);
    // Sample variance = sum((x-mean)^2) / (n-1) = 32/7 ≈ 4.571
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 4.571, variance);
}

// Test stats_stddev
void test_stats_stddev(void) {
    double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double stddev = stats_stddev(data, 8);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 2.138, stddev);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_rng_seed);
    RUN_TEST(test_rng_uniform_range);
    RUN_TEST(test_rng_uniform_mean);
    RUN_TEST(test_rng_normal_mean);
    RUN_TEST(test_rng_normal_variance);
    RUN_TEST(test_rng_normal_params);
    RUN_TEST(test_gbm_create);
    RUN_TEST(test_gbm_reset);
    RUN_TEST(test_gbm_positive_prices);
    RUN_TEST(test_gbm_drift);
    RUN_TEST(test_gbm_expected_value);
    RUN_TEST(test_stats_mean);
    RUN_TEST(test_stats_variance);
    RUN_TEST(test_stats_stddev);
    
    return UNITY_END();
}
