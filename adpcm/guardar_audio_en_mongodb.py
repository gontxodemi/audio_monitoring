from pymongo import MongoClient
import gridfs
from datetime import datetime

# === CONFIGURACIÓN: reemplaza con tu URI de MongoDB Atlas ===
mongo_uri = "mongodb+srv://gdemig1:imTAXX3uhMy4fcMw@cluster0.uxlb0r3.mongodb.net/?retryWrites=true&w=majority"

# === NOMBRE DE BASE DE DATOS Y ARCHIVO ===
db_name = "audio_database"
collection_name = "fs.files"  # colección por defecto de GridFS
filename = "output.wav"

# === CONEXIÓN Y CARGA DEL ARCHIVO ===
client = MongoClient(mongo_uri)
db = client[db_name]
fs = gridfs.GridFS(db)

# === GUARDAR ARCHIVO .WAV EN GRIDFS CON METADATOS ===
with open(filename, "rb") as f:
    file_id = fs.put(
        f,
        filename=filename,
        contentType="audio/wav",
        uploadDate=datetime.utcnow(),
        metadata={
            "fuente": "ESP32",
            "tipo": "audio comprimido",
            "duracion_estimada": "5s",
            "proyecto": "TFM_monitorizacion_animales"
        }
    )

print(f"✔️ Archivo '{filename}' guardado en MongoDB con ID: {file_id}")
