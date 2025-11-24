#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

class NeuralNetwork {
public:
    NeuralNetwork();
    float predict(const float input[9]);

private:
    float relu(float x) const;
    float sigmoid(float x) const;
    void dense(const float *input, int in_size,
               const float *W, const float *b,
               int out_size, float *output) const;
};

extern NeuralNetwork neuralNet;

#endif
