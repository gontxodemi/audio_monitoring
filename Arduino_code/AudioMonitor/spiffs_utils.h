#pragma once
#include <Arduino.h>

// Inicializa SPIFFS y muestra los archivos en memoria
void init_and_list_spiffs();

// Lista los archivos actuales del sistema SPIFFS
void list_spiffs_files();

// Elimina un archivo si existe
bool delete_spiffs_file(const char* path);
