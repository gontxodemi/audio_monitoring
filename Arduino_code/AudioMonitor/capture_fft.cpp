#include "capture_fft.h"
#include <arduinoFFT.h>
#include <driver/i2s.h>

ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);

// Buffers compartidos
int32_t rawSamples[FFT_SAMPLES];
double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];

bool capture_audio(unsigned int duration_ms) {
  unsigned long start_time = millis();
  size_t bytes_read;
  int samples_captured = 0;

  while ((millis() - start_time) < duration_ms && samples_captured < FFT_SAMPLES) {
    esp_err_t result = i2s_read(
      I2S_NUM_0,
      &rawSamples[samples_captured],
      sizeof(int32_t) * 32,
      &bytes_read,
      portMAX_DELAY
    );

    if (result == ESP_OK) {
      samples_captured += bytes_read / sizeof(int32_t);
    } else {
      Serial.println("Error en lectura I2S");
      return false;
    }
  }

  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] = (rawSamples[i] >> 8) / 16777216.0;
    vImag[i] = 0;
  }

  return true;
}

void process_fft() {
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();
}

bool check_bird_frequency() {
  double freqPerBin = (SAMPLE_RATE / 2.0) / FFT_SAMPLES;
  bool detection = false;

  for (int i = 0; i < FFT_SAMPLES / 2; i++) {
    double freq = i * freqPerBin;
    if (freq >= BIRD_FREQ_MIN && freq <= BIRD_FREQ_MAX) {
      if (vReal[i] > 0.1) {
        detection = true;
        // Puedes descomentar para debug:
        // Serial.printf("Pico en %.2f Hz, Magnitud: %.2f\n", freq, vReal[i]);
      }
    }
  }

  return detection;
}
