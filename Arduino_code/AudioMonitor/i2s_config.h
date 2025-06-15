#pragma once
#include <driver/i2s.h>

// Pines definidos como en tus códigos originales
#define I2S_WS 15     // LRC
#define I2S_SD 13     // DOUT
#define I2S_SCK 2     // BCLK
#define I2S_PORT I2S_NUM_0

#define I2S_SAMPLE_RATE 16000
#define I2S_SAMPLE_BITS 16

// Inicializa el I2S con la configuración original
void i2sInit();
