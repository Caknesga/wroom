import wave
import struct

SAMPLE_RATE = 16000
samples = []

# -------- LOAD SAMPLES --------
with open("audio2.txt", "r", errors="ignore") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        if line.lstrip("-").isdigit():
            samples.append(int(line))

print(f"Loaded {len(samples)} samples")

if len(samples) == 0:
    raise RuntimeError("No samples loaded")

# -------- DC REMOVAL --------
mean = sum(samples) / len(samples)
samples = [int(s - mean) for s in samples]

# -------- NORMALIZATION --------
peak = max(abs(s) for s in samples)
if peak > 0:
    gain = 0.9 * 32767 / peak
    samples = [int(s * gain) for s in samples]

# -------- WRITE WAV --------
wav = wave.open("audio2.wav", "w")
wav.setnchannels(1)
wav.setsampwidth(2)      # 16-bit PCM
wav.setframerate(SAMPLE_RATE)

for s in samples:
    if s > 32767:
        s = 32767
    elif s < -32768:
        s = -32768
    wav.writeframes(struct.pack("<h", s))

wav.close()

print("WAV written with DC removal + normalization")
