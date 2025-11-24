#include "StepDetector.h"
#include "NeuralNetwork.h"


const float SCALER_MEANS[11] = { 0.077899f, -0.007444f, 0.077909f, 0.085343f, 0.085353f, 0.036678f, 0.005187f, 0.005288f, 0.495623f, 0.504377f, 30.513977f };
const float SCALER_STDS[11] = { 0.199575f, 0.169173f, 0.199571f, 0.159960f, 0.156068f, 0.079525f, 0.010317f, 0.010627f, 0.189128f, 0.189128f, 14.910222f };


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
            // float max2_val = prev_sma;
            // float diff_max1_min = candidate_first_max_val - candidate_min_val;
            // float diff_max2_min = max2_val - candidate_min_val;
            // float diff_max1_max2 = abs(candidate_first_max_val - max2_val);
            // unsigned long interval_width = (sample_count - 1) - candidate_first_max_sample;

            // float f0 = candidate_first_max_val;
            // float f1 = candidate_min_val;
            // float f2 = max2_val;
            // float f3 = diff_max1_min;
            // float f4 = diff_max2_min;
            // float f5 = diff_max1_max2;
            // float f6 = (float)(candidate_min_sample - candidate_first_max_sample)/interval_width;
            // float f7 = (float)((sample_count - 1) - candidate_min_sample)/interval_width;


            float val_max1 = candidate_first_max_val;
            float val_min  = candidate_min_val;
            float val_max2 = prev_sma; // The current peak we just found

            // --- 2. CALCULATE TIMES ---
            // Cast to float immediately so division works later
            float t1 = (float)(candidate_min_sample - candidate_first_max_sample);
            float t2 = (float)((sample_count - 1) - candidate_min_sample);
            float duration = t1 + t2;

            // --- 3. CALCULATE FEATURES (Order must match Python EXACTLY) ---
            
            // [0, 1, 2] Raw Values
            float f0 = val_max1;
            float f1 = val_min;
            float f2 = val_max2;

            // [3, 4, 5] Differences
            float f3 = val_max1 - val_min;
            float f4 = val_max2 - val_min;
            float f5 = abs(val_max1 - val_max2);

            // [6, 7] Slopes (Protect against divide by 0)
            float f6 = (t1 > 0) ? (f3 / t1) : 0.0f; 
            float f7 = (t2 > 0) ? (f4 / t2) : 0.0f;

            // [8, 9] Time Ratios
            float f8 = (duration > 0) ? (t1 / duration) : 0.0f;
            float f9 = (duration > 0) ? (t2 / duration) : 0.0f;

            // [10] Duration
            float f10 = duration;

            // --- 4. PACK INTO ARRAY ---
            float features[11] = { f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10 };

            for(int i=0; i<11; i++){
                features[i] = (features[i] - SCALER_MEANS[i]) / SCALER_STDS[i];
            }

            float prob = neuralNet.predict(features);
            stepDetected = (prob > 0.5f);

  

            currentState = LOOKING_FOR_MIN;
            candidate_first_max_val = val_max2;
            candidate_first_max_sample = sample_count - 1;
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