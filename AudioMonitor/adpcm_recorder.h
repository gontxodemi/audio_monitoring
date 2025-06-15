#pragma once
#include <Arduino.h>

#define ADPCM_FILENAME "/recording.adpcm"
#define RECORD_TIME_SECONDS 20

// Lanza la tarea de grabación de audio con compresión ADPCM
void start_adpcm_recording_task();
void i2s_adc(void* arg);
void SPIFFSInit();
