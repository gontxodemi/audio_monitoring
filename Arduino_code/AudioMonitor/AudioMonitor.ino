#include <Arduino.h>

// Módulos del proyecto
#include "i2s_config.h"
#include "capture_fft.h"
#include "adpcm_recorder.h"
#include "spiffs_utils.h"
#include "FileServer.h"

const char* ssid = "DIGIFIBRA-seM4";
const char* password = "LpCvtxCV49bK";
const char* hostname = "esp32fs";

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("🔊 Iniciando sistema de monitoreo acústico...");
  Serial.print("📦 Heap libre: ");
  Serial.println(ESP.getFreeHeap());

  // Inicializar SPIFFS y mostrar archivos existentes
  init_and_list_spiffs();

  //LoRaWAN_setup();  // Inicializar LoRaWAN

  // Configurar I2S
  i2sInit();

  FileServer_setup(ssid, password, hostname);

  delay(500); // Estabilización de hardware
}

void loop() {
  // Capturar 200ms de audio
  if (capture_audio(200)) {
    // Procesar ventana FFT
    process_fft();

    // Verificar si hay frecuencias dentro del rango objetivo
    if (check_bird_frequency()) {
      Serial.println("[DETECCIÓN] Sonido de ave detectado - Preparando grabación larga (20s)");
      SPIFFSInit();  // Inicializa SPIFFS y archivo .adpcm
      start_adpcm_recording_task();  // Lanza tarea de grabación ADPCM
      delay(RECORD_TIME_SECONDS * 1000 + 1000);  // Esperar a que termine

      //transmit_adpcm_file();  // Envía el archivo grabado por LoRa
      Serial.println("✅ Grabación finalizada. Deteniendo ejecución para pruebas.");
      while (true) {
        FileServer_loop();  // Mantiene activo el servidor web para acceso a archivos
        delay(1000);  // Detiene el loop indefinidamente
      }
    }
  }

  // Si no hay pico, no continuar tras la primera prueba
  delay(3000);
  Serial.println("Comenzando nueva captura");

  //LoRaWAN_loop();
}
