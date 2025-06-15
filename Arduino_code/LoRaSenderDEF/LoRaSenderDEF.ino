#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "sha256.h"

#define RADIO_NSS LORA_DEFAULT_NSS_PIN
#define HELTEC_BOARD WIFI_LoRa_32_V2
#define SLOW_CLK_TPYE 0

#define RF_FREQUENCY 433000000
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 1
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define BUFFER_DATA_SIZE 220
#define HEADER_SIZE 2
#define PAYLOAD_SIZE (BUFFER_DATA_SIZE + HEADER_SIZE)
#define WINDOW_SIZE 10

bool radioBusy = false;  // Controla si hay un envío LoRa activo
bool ackReceived[1024] = {false};
uint16_t baseIndex = 0;
uint16_t nextToSend = 0;

const char *ssid = "DIGIFIBRA-seM4";
const char *password = "LpCvtxCV49bK";

WebServer server(80);

File audioFile;

bool lora_idle = true;
bool transmissionStarted = false;

static RadioEvents_t RadioEvents;
uint8_t txBuffer[PAYLOAD_SIZE];

String fileHash = "";

void handleRoot() {
  String html = "<html><body><h1>Subir audio.adpcm</h1>"
                "<form method='POST' action='/upload' enctype='multipart/form-data'>"
                "<input type='file' name='audiofile'><br><br>"
                "<input type='submit' value='Subir'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Subiendo archivo: %s\n", upload.filename.c_str());
    if (SPIFFS.exists("/audio.adpcm")) SPIFFS.remove("/audio.adpcm");
    audioFile = SPIFFS.open("/audio.adpcm", FILE_WRITE);
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (audioFile) audioFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (audioFile) {
      audioFile.close();
      Serial.println("Archivo subido correctamente.");

      audioFile = SPIFFS.open("/audio.adpcm", "r");
      if (audioFile) {
        sha256_context ctx;
        uint8_t hash[SHA256_SIZE_BYTES];
        sha256_init(&ctx);
        uint8_t buffer[512];
        size_t bytesRead = 0;
        while ((bytesRead = audioFile.read(buffer, sizeof(buffer))) > 0) {
          sha256_hash(&ctx, buffer, bytesRead);
        }
        sha256_done(&ctx, hash);
        audioFile.close();

        fileHash = "";
        for (int i = 0; i < SHA256_SIZE_BYTES; i++) {
          if (hash[i] < 16) fileHash += "0";
          fileHash += String(hash[i], HEX);
        }

        Serial.println("SHA256 Hash: " + fileHash);

        transmissionStarted = true;
        baseIndex = 0;
        nextToSend = 0;
        lora_idle = true;
        memset(ackReceived, 0, sizeof(ackReceived));
      }
    }
    server.send(200, "text/html", "<html><body><h2>Archivo subido correctamente!</h2></body></html>");
  }
}

void OnTxDone(void) {
  Serial.println("TX done");
  radioBusy = false;
  Radio.Rx(200);  // Escuchar ACK tras el envío
}


void OnTxTimeout(void) {
  Serial.println("TX Timeout");
  lora_idle = true;
}

void OnRxDoneACK(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size == 2) {
    uint16_t ackIndex = ((uint16_t)payload[0] << 8) | payload[1];

    if (ackIndex >= baseIndex && ackIndex < nextToSend) {
      Serial.printf("✅ ACK recibido para paquete %u\n", ackIndex);
      ackReceived[ackIndex] = true;

      while (ackReceived[baseIndex]) {
        baseIndex++;
      }
    } else {
      Serial.printf("⚠️ ACK fuera de ventana: %u (esperando %u a %u)\n",
                    ackIndex, baseIndex, nextToSend - 1);
    }
  }

  Radio.Sleep();
  lora_idle = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando...");

  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    while (1);
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Conectado! IP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() { server.send(200); }, handleUpload);
  server.begin();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDoneACK;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Serial.println("Listo para subir archivo.");
}

void loop() {
  server.handleClient();

  // Enviar nuevos paquetes si aún hay espacio en la ventana y no hay transmisión en curso
  if (transmissionStarted && (nextToSend < baseIndex + WINDOW_SIZE) && !radioBusy) {
    if (!audioFile) {
      audioFile = SPIFFS.open("/audio.adpcm", "r");
      if (!audioFile) {
        Serial.println("No se pudo abrir audio.adpcm para transmisión");
        transmissionStarted = false;
        return;
      }
      Serial.println("Archivo abierto para transmisión");
    }

    if (audioFile.available()) {
      audioFile.seek(nextToSend * BUFFER_DATA_SIZE, SeekSet);
      size_t bytesRead = audioFile.read(&txBuffer[HEADER_SIZE], BUFFER_DATA_SIZE);
      txBuffer[0] = (nextToSend >> 8) & 0xFF;
      txBuffer[1] = nextToSend & 0xFF;

      Serial.printf("📤 Enviando paquete %u con %d bytes\n", nextToSend, bytesRead + HEADER_SIZE);
      radioBusy = true;
      Radio.Send(txBuffer, bytesRead + HEADER_SIZE);
      nextToSend++;
    } else if (baseIndex == nextToSend) {
      Serial.println("Archivo transmitido completo");
      audioFile.close();
      transmissionStarted = false;

      txBuffer[0] = 0xFF;
      txBuffer[1] = 0xFF;
      txBuffer[2] = 0;
      radioBusy = true;
      Radio.Send(txBuffer, HEADER_SIZE);
      Serial.println("Paquete final enviado al receptor");
    }
  }

  // Retransmitir si hay paquetes pendientes de ACK
  static unsigned long lastRetryCheck = 0;
  if (millis() - lastRetryCheck > 1000) {
    lastRetryCheck = millis();
    for (uint16_t i = baseIndex; i < nextToSend; i++) {
      if (!ackReceived[i] && !radioBusy) {
        Serial.printf("⏳ Reenviando paquete %u\n", i);
        audioFile.seek(i * BUFFER_DATA_SIZE, SeekSet);
        size_t bytesRead = audioFile.read(&txBuffer[HEADER_SIZE], BUFFER_DATA_SIZE);
        txBuffer[0] = (i >> 8) & 0xFF;
        txBuffer[1] = i & 0xFF;
        radioBusy = true;
        Radio.Send(txBuffer, bytesRead + HEADER_SIZE);
        break;  // Solo uno por ciclo
      }
    }
  }

  Radio.IrqProcess();
  delay(1);
}
