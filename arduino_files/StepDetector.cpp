#include "StepDetector.h"

// --- Constructor ---
StepDetector::StepDetector() {
    // Initialize default parameters
    alpha = 0.03;
    windowSize = 10; 
    isFirstSample = true;

    // SMA variables
    sma_buffer = nullptr;
    sma_index = 0;
    sma_sum = 0.0;
    sma_buffer_full = false;
    last_sma_value = 0.0;
    
    // Peak detection state
    currentState = LOOKING_FOR_FIRST_MAX;
    sample_count = 0;
    prev_sma = 0;
    current_sma = 0;

    // UPDATED: Set default thresholds from your Python example, including upper bounds
    setStepThresholds(30, 0.0001, 1.0, 0.0001, 1.0, 0.0001, 1.0);
    
    // Allocate buffer with default size
    setWindowSize(windowSize);
}

// --- Configuration Methods ---
void StepDetector::setAlpha(float new_alpha) {
    alpha = new_alpha;
}

void StepDetector::setWindowSize(int size) {
    if (size < 1) size = 1;
    if (sma_buffer != nullptr) {
        delete[] sma_buffer;
    }
    windowSize = size;
    sma_buffer = new float[windowSize];
    for (int i = 0; i < windowSize; ++i) sma_buffer[i] = 0.0;
    sma_index = 0;
    sma_sum = 0.0;
    sma_buffer_full = false;
}

// UPDATED: Now stores all 6 bound values
void StepDetector::setStepThresholds(int min_peak_interval, 
                                   float max1_min_diff_lower, float max1_min_diff_upper,
                                   float max2_min_diff_lower, float max2_min_diff_upper,
                                   float max1_max2_diff_lower, float max1_max2_diff_upper) {
    min_peak_interval_samples = min_peak_interval;
    max1_min_diff_lower_bound = max1_min_diff_lower;
    max1_min_diff_upper_bound = max1_min_diff_upper;
    max2_min_diff_lower_bound = max2_min_diff_lower;
    max2_min_diff_upper_bound = max2_min_diff_upper;
    max1_max2_diff_lower_bound = max1_max2_diff_lower;
    max1_max2_diff_upper_bound = max1_max2_diff_upper;
}


const char* StepDetector::getCurrentState() {
    switch (currentState) {
        case LOOKING_FOR_FIRST_MAX: return "LOOKING_FOR_FIRST_MAX";
        case LOOKING_FOR_MIN:       return "LOOKING_FOR_MIN";
        case LOOKING_FOR_SECOND_MAX:return "LOOKING_FOR_SECOND_MAX";
        default:                    return "UNKNOWN";
    }
}


// --- Main Processing Function ---
bool StepDetector::process(float ax, float ay, float az) {
    bool stepDetected = false;
    sample_count++;
    
    // 1. Exponential Moving Average
    if (isFirstSample) {
        ax_lp = ax; ay_lp = ay; az_lp = az;
        isFirstSample = false;
    } else {
        ax_lp = (alpha * ax) + (1.0 - alpha) * ax_lp;
        ay_lp = (alpha * ay) + (1.0 - alpha) * ay_lp;
        az_lp = (alpha * az) + (1.0 - alpha) * az_lp;
    }

    // 2. Magnitude
    float magnitude_lp = sqrt(ax_lp * ax_lp + ay_lp * ay_lp + az_lp * az_lp) - 1.0;

    // 3. Simple Moving Average
    sma_sum -= sma_buffer[sma_index];
    sma_buffer[sma_index] = magnitude_lp;
    sma_sum += magnitude_lp;
    sma_index++;
    if (sma_index >= windowSize) {
        sma_index = 0;
        sma_buffer_full = true;
    }
    if (!sma_buffer_full) return false;
    
    prev_sma = current_sma;
    current_sma = sma_sum / windowSize;

    // 4. Peak Detection State Machine
    // Check for local maximum (positive peak)
    if (prev_sma > current_sma && prev_sma > last_sma_value) {
        if (currentState == LOOKING_FOR_FIRST_MAX || currentState == LOOKING_FOR_MIN) {
            currentState = LOOKING_FOR_MIN;
            candidate_first_max_val = prev_sma;
            candidate_first_max_sample = sample_count - 1;
        }
        else if (currentState == LOOKING_FOR_SECOND_MAX) {
            float max2_val = prev_sma;
            float diff_max1_min = candidate_first_max_val - candidate_min_val;
            float diff_max2_min = max2_val - candidate_min_val;
            float diff_max1_max2 = abs(candidate_first_max_val - max2_val);
            unsigned long interval_width = (sample_count - 1) - candidate_first_max_sample;

            // --- UPDATED: Full validation check with lower AND upper bounds ---
            bool is_wide_enough = interval_width >= min_peak_interval_samples;
            bool is_max1_min_valid = (max1_min_diff_lower_bound <= diff_max1_min && diff_max1_min <= max1_min_diff_upper_bound);
            bool is_max2_min_valid = (max2_min_diff_lower_bound <= diff_max2_min && diff_max2_min <= max2_min_diff_upper_bound);
            bool is_max1_max2_valid = (max1_max2_diff_lower_bound <= diff_max1_max2 && diff_max1_max2 <= max1_max2_diff_upper_bound);
            
            if (is_wide_enough && is_max1_min_valid && is_max2_min_valid && is_max1_max2_valid) {
                stepDetected = true;
                currentState = LOOKING_FOR_MIN;
                candidate_first_max_val = max2_val;
                candidate_first_max_sample = sample_count - 1;
            } else {
                currentState = LOOKING_FOR_MIN;
                candidate_first_max_val = max2_val;
                candidate_first_max_sample = sample_count - 1;
            }
        }
    }

    // Check for local minimum (negative peak)
    if (prev_sma < current_sma && prev_sma < last_sma_value) {
        if (currentState == LOOKING_FOR_MIN) {
            currentState = LOOKING_FOR_SECOND_MAX;
            candidate_min_val = prev_sma;
            candidate_min_sample = sample_count - 1;
        }
    }

    last_sma_value = prev_sma;
    return stepDetected;
}