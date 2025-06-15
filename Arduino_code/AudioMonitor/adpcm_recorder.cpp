#include "adpcm_recorder.h"
#include "i2s_config.h"
#include <SPIFFS.h>
#include "spiffs_utils.h"


extern "C" {
  #include "adpcm-lib.h"
}

#define I2S_READ_LEN      (1024 * 4)
#define I2S_CHANNEL_NUM   1
#define PCM_TOTAL_SIZE    (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME_SECONDS)

static File file;

void SPIFFSInit() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield();
  }

  SPIFFS.remove(ADPCM_FILENAME);
  file = SPIFFS.open(ADPCM_FILENAME, FILE_WRITE);
  if (!file) {
    Serial.println("File is not available!");
  }

  File root = SPIFFS.open("/");
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("Archivo: %s (%d bytes)\n", entry.name(), entry.size());
    entry = root.openNextFile();
  }
}

void start_adpcm_recording_task() {
  xTaskCreate(i2s_adc, "i2s_adc", 8192, NULL, 1, NULL);
}

void i2s_adc(void *arg)
{
  size_t bytes_read;
  int flash_wr_size = 0;
  const int samples_per_block = 505;
  int16_t* pcm_block = (int16_t*) malloc(samples_per_block * sizeof(int16_t));
  uint8_t adpcm_block[256];

  int32_t deltas[2] = {0, 0};
  void* adpcm_ctx = adpcm_create_context(1, 2, NOISE_SHAPING_DYNAMIC, deltas);

  if (!pcm_block || !adpcm_ctx) {
    Serial.println("Memory allocation failed!");
    vTaskDelete(NULL);
  }

  Serial.println("*** Recording Start (ADPCM-XQ) ***");
  while (flash_wr_size < PCM_TOTAL_SIZE) {
    if (i2s_read(I2S_PORT, pcm_block, samples_per_block * 2, &bytes_read, portMAX_DELAY) != ESP_OK) {
      Serial.println("Error reading I2S");
      break;
    }

    size_t adpcm_size = 0;
    adpcm_encode_block(adpcm_ctx, adpcm_block, &adpcm_size, pcm_block, samples_per_block);

    if (file.write(adpcm_block, adpcm_size) != adpcm_size) {
      Serial.println("Error writing to file");
      break;
    }

    flash_wr_size += samples_per_block * 2;
    ets_printf("Recorded: %u / %u bytes\n", flash_wr_size, PCM_TOTAL_SIZE);
  }

  file.close();
  adpcm_free_context(adpcm_ctx);
  free(pcm_block);
  list_spiffs_files();
  vTaskDelete(NULL);
}


