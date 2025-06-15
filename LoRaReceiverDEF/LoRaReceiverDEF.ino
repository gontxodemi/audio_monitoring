#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include "sha256.h"  // librería SHA256 para la comparación de la integridad
#include "LoRaWan_APP.h"
#include "Arduino.h"

#define RADIO_NSS LORA_DEFAULT_NSS_PIN
#define HELTEC_BOARD WIFI_LoRa_32_V2
#define SLOW_CLK_TPYE 0

#define RF_FREQUENCY 433000000
#define LORA_BANDWIDTH 1   // 250kHz
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

const char *ssid = "DIGIFIBRA-seM4";     
const char *password = "LpCvtxCV49bK";  

WebServer server(80);
static RadioEvents_t RadioEvents;

const char* filename = "/reconstruido.adpcm";
File receivedFile;

#define HEADER_SIZE 2
#define DATA_SIZE 220

uint16_t expectedPacket = 0;
bool lora_idle = true;

unsigned long lastPacketTime = 0;
bool transmissionComplete = false;

const char* serverUploadUrl = "http://192.168.1.146/upload";
bool fileUploaded = false;

String fileHash = "";  // Para mostrar el hash calculado

void serveFile() {
  if (SPIFFS.exists(filename)) {
    File f = SPIFFS.open(filename, "r");
    server.streamFile(f, "audio/adpcm");
    f.close();
  } else {
    server.send(404, "text/plain", "Archivo no encontrado");
  }
}

void handleRoot() {
  String html = "<html><body><h1>Archivo ADPCM recibido</h1>"
                "<a href='/audio' download='reconstruido.adpcm'>Descargar archivo reconstruido.adpcm</a><br><br>"
                "<p>IP dispositivo: ";
  html += WiFi.localIP().toString();
  html += "</p></body></html>";
  server.send(200, "text/html", html);
}

void OnRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size < HEADER_SIZE) {
    Serial.println("❌ Paquete demasiado pequeño");
    Radio.Sleep();
    lora_idle = true;
    return;
  }

  uint16_t packetIndex = (payload[0] << 8) | payload[1];

  // 🔚 Paquete final explícito
  if (packetIndex == 0xFFFF) {
    Serial.println("🔚 Paquete final recibido. Cerrando archivo y subiendo al servidor.");
    if (receivedFile) receivedFile.close();
    transmissionComplete = true;

    // Verificación SHA256
    receivedFile = SPIFFS.open(filename, "r");
    if (receivedFile) {
      sha256_context ctx;
      uint8_t hash[SHA256_SIZE_BYTES];
      sha256_init(&ctx);

      uint8_t buffer[512];
      size_t bytesRead = 0;
      while ((bytesRead = receivedFile.read(buffer, sizeof(buffer))) > 0) {
        sha256_hash(&ctx, buffer, bytesRead);
      }

      sha256_done(&ctx, hash);
      receivedFile.close();

      fileHash = "";
      for (int i = 0; i < SHA256_SIZE_BYTES; i++) {
        if (hash[i] < 16) fileHash += "0";
        fileHash += String(hash[i], HEX);
      }

      Serial.println("SHA256 Hash: " + fileHash);
    }

    Radio.Sleep();
    lora_idle = true;
    return;
  }

  uint8_t* data = payload + HEADER_SIZE;
  uint16_t dataLen = size - HEADER_SIZE;

  if (!receivedFile) {
    receivedFile = SPIFFS.open(filename, FILE_WRITE);
    if (!receivedFile) {
      Serial.println("❌ Error abriendo archivo");
      Radio.Sleep();
      lora_idle = true;
      return;
    }
  }

  // Si el paquete ya se había escrito, lo ignoramos pero reenviamos ACK
  static uint16_t lastWritten = 0xFFFF;
  if (packetIndex == lastWritten) {
    Serial.printf("🔁 Duplicado recibido: %u. Reenviando ACK\n", packetIndex);
  } else {
    Serial.printf("📥 Paquete %u recibido, %u bytes, RSSI %d\n", packetIndex, dataLen, rssi);
    receivedFile.write(data, dataLen);
    receivedFile.flush();
    lastWritten = packetIndex;
  }

  // ACK para todos los casos válidos
  uint8_t ackPayload[2];
  ackPayload[0] = (packetIndex >> 8) & 0xFF;
  ackPayload[1] = packetIndex & 0xFF;
  Radio.Send(ackPayload, 2);
  Serial.printf("✅ ACK enviado para paquete %u\n", packetIndex);

  Radio.Sleep();
  lora_idle = true;
}



void uploadFileToServer() {
  if (fileUploaded || !SPIFFS.exists(filename)) return;

  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("No se pudo abrir archivo para subir");
    return;
  }

  Serial.println("Subiendo archivo al servidor...");

  HTTPClient http;
  http.begin(serverUploadUrl);
  http.addHeader("Content-Type", "application/octet-stream");

  int fileSize = file.size();
  int httpResponseCode = http.sendRequest("POST", &file, fileSize);

  file.close();

  if (httpResponseCode > 0) {
    Serial.printf("Subida completada, código HTTP: %d\n", httpResponseCode);
    fileUploaded = true;
  } else {
    Serial.printf("Error en subida: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando receptor...");

  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    while (1) delay(1000);
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Conectado! IP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/audio", HTTP_GET, serveFile);
  server.begin();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    0, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  lora_idle = true;
}

void loop() {
  server.handleClient();

  if (!transmissionComplete) {
    if (lora_idle) {
      lora_idle = false;
      Serial.println("Entrando modo RX LoRa");
      Radio.Rx(0);
    }
  } else {
    uploadFileToServer();
  }

  Radio.IrqProcess();
  delay(1);
}
