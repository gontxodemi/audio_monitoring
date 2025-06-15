#include "spiffs_utils.h"
#include <SPIFFS.h>

void init_and_list_spiffs() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS initialization failed!");
    while (true) yield();
  }

  Serial.println("✅ SPIFFS mounted successfully");
  list_spiffs_files();
}

void list_spiffs_files() {
  Serial.println("📁 Archivos en SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }
}

bool delete_spiffs_file(const char* path) {
  if (SPIFFS.exists(path)) {
    return SPIFFS.remove(path);
  }
  return false;
}
