import wave
import struct
from pymongo import MongoClient
import gridfs
from datetime import datetime

# === Parámetros de ADPCM ===
ADPCM_BLOCK_SIZE = 256
SAMPLES_PER_BLOCK = 505
CHANNELS = 1
SAMPLE_RATE = 16000
INPUT_ADPCM_FILE = "recording.adpcm"
OUTPUT_WAV_FILE = "output.wav"

# === Tablas ADPCM ===
step_table = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

index_table = [-1, -1, -1, -1, 2, 4, 6, 8]

# === Función de decodificación ADPCM a PCM ===
def decode_block(block):
    pcm_samples = []
    pred_sample = struct.unpack('<h', block[0:2])[0]
    index = block[2]
    if index > 88: index = 88
    step = step_table[index]
    pcm_samples.append(pred_sample)

    for byte in block[4:]:
        for nibble_shift in (0, 4):
            nibble = (byte >> nibble_shift) & 0x0F
            delta = step >> 3
            if nibble & 1: delta += step >> 2
            if nibble & 2: delta += step >> 1
            if nibble & 4: delta += step
            if nibble & 8: pred_sample -= delta
            else: pred_sample += delta
            pred_sample = max(-32768, min(32767, pred_sample))
            pcm_samples.append(pred_sample)
            index += index_table[nibble & 0x07]
            index = max(0, min(88, index))
            step = step_table[index]
    return pcm_samples

# === Decodificar archivo .adpcm ===
with open(INPUT_ADPCM_FILE, "rb") as f:
    adpcm_data = f.read()

pcm_output = []
for i in range(0, len(adpcm_data), ADPCM_BLOCK_SIZE):
    block = adpcm_data[i:i + ADPCM_BLOCK_SIZE]
    if len(block) == ADPCM_BLOCK_SIZE:
        pcm_output.extend(decode_block(block))

# === Guardar como archivo .wav ===
with wave.open(OUTPUT_WAV_FILE, "wb") as wf:
    wf.setnchannels(CHANNELS)
    wf.setsampwidth(2)  # 16 bits = 2 bytes
    wf.setframerate(SAMPLE_RATE)
    for sample in pcm_output:
        wf.writeframes(struct.pack('<h', sample))

print(f"✔️ Archivo WAV generado: {OUTPUT_WAV_FILE}")

# === Subir a MongoDB ===
mongo_uri = "mongodb+srv://gdemig1:imTAXX3uhMy4fcMw@cluster0.uxlb0r3.mongodb.net/?retryWrites=true&w=majority"
db_name = "audio_database"

client = MongoClient(mongo_uri)
db = client[db_name]
fs = gridfs.GridFS(db)

with open(OUTPUT_WAV_FILE, "rb") as f:
    file_id = fs.put(
        f,
        filename=OUTPUT_WAV_FILE,
        contentType="audio/wav",
        uploadDate=datetime.utcnow(),
        metadata={
            "fuente": "ESP32",
            "tipo": "audio comprimido",
            "duracion_estimada": "5s",
            "proyecto": "TFM_monitorizacion_animales"
        }
    )

print(f"✔️ Archivo '{OUTPUT_WAV_FILE}' guardado en MongoDB con ID: {file_id}")
