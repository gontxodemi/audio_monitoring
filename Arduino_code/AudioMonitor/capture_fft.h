#pragma once
#include <Arduino.h>

#define SAMPLE_RATE 16000
#define FFT_SAMPLES 512
#define BIRD_FREQ_MIN 3000
#define BIRD_FREQ_MAX 10000

extern int32_t rawSamples[FFT_SAMPLES];
extern double vReal[FFT_SAMPLES];
extern double vImag[FFT_SAMPLES];

bool capture_audio(unsigned int duration_ms);
void process_fft();
bool check_bird_frequency();
