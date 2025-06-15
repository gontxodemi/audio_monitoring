#include "FileServer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>

static WebServer server(80);

static const char* _ssid;
static const char* _password;
static const char* _hostname;

String formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + "B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 2) + "KB";
  else return String(bytes / 1024.0 / 1024.0, 2) + "MB";
}

bool exists(String path) {
  File file = SPIFFS.open(path, "r");
  bool isFile = false;
  if (file) {
    isFile = !file.isDirectory();
    file.close();
  }
  return isFile;
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.htm";
  String contentType = "text/plain";
  if (path.endsWith(".htm") || path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".jpg")) contentType = "image/jpeg";
  else if (path.endsWith(".gif")) contentType = "image/gif";

  if (exists(path)) {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }
  String path = server.arg("dir");
  File root = SPIFFS.open(path);
  String output = "[";
  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      if (output != "[") output += ",";
      output += "{\"type\":\"";
      output += (file.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(file.name()).substring(1);
      output += "\"}";
      file = root.openNextFile();
    }
  }
  output += "]";
  server.send(200, "application/json", output);
}

void handleFileDelete() {
  if (server.args() == 0) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }
  String path = server.arg(0);
  if (path == "/") {
    server.send(500, "text/plain", "BAD PATH");
    return;
  }
  if (!exists(path)) {
    server.send(404, "text/plain", "FileNotFound");
    return;
  }
  SPIFFS.remove(path);
  server.send(200, "text/plain", "File Deleted");
}

void handleFileUpload() {
  if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    File fsUploadFile = SPIFFS.open(filename, "w");
    fsUploadFile.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File fsUploadFile = SPIFFS.open(server.uri(), "a");
    if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    fsUploadFile.close();
  }
}

void FileServer_setup(const char* ssid, const char* password, const char* hostname) {
  _ssid = ssid;
  _password = password;
  _hostname = hostname;

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);

  Serial.printf("Connecting to WiFi SSID: %s\n", _ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(_hostname)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.printf("Open http://%s.local to browse files\n", _hostname);

  server.on("/list", HTTP_GET, handleFileList);
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  server.on("/edit", HTTP_POST, []() { server.send(200); }, handleFileUpload);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.begin();
  Serial.println("HTTP File Server started");
}

void FileServer_loop() {
  server.handleClient();
}
