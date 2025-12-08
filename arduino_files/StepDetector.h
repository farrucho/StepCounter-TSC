#ifndef STEPDETECTOR_H
#define STEPDETECTOR_H

#include <Arduino.h>

class StepDetector {
public:
    StepDetector();

    void setAlpha(float alpha);
    void setWindowSize(int size);

    // main processing function
    bool process(float ax, float ay, float az);

    const char* getCurrentState();

private:
    enum State {
        LOOKING_FOR_FIRST_MAX,
        LOOKING_FOR_MIN,
        LOOKING_FOR_SECOND_MAX
    };
    State currentState;

    // EMA
    float alpha;
    float ax_lp, ay_lp, az_lp;
    bool isFirstSample;
    
    // SMA
    float* sma_buffer;
    int windowSize;
    int sma_index;
    float sma_sum;
    bool sma_buffer_full;
    float last_sma_value;

    // peak detection
    unsigned long sample_count;
    float prev_sma;
    float current_sma;

    // FSM candidate values
    float candidate_first_max_val;
    unsigned long candidate_first_max_sample;
    float candidate_min_val;
    unsigned long candidate_min_sample;

    // step validation thresholds
    int min_peak_interval_samples;
    float max1_min_diff_lower_bound, max1_min_diff_upper_bound;
    float max2_min_diff_lower_bound, max2_min_diff_upper_bound;
    float max1_max2_diff_lower_bound, max1_max2_diff_upper_bound;
};

#endif // STEPDETECTOR_H