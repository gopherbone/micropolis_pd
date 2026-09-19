#!/usr/bin/env python3
"""
convert_audio.py - Standardize Micropolis PCM audio assets into 16-bit 22050 Hz Mono WAVs.
Uses ffmpeg with automatic discovery of source audio repositories.
"""

import os
import sys
import subprocess
import shutil
import glob

CANDIDATE_SOURCE_DIRS = [
    "/home/iceman/Developer/games/city/micropolis-bsd/res/sounds",
    "/home/iceman/Developer/games/city/MicropolisCore/resources/sounds",
    "/home/iceman/Developer/games/city/dh-micro/micropolis-activity/res/sounds"
]

TARGET_SAMPLE_RATE = 22050
TARGET_CHANNELS = 1
TARGET_CODEC = "pcm_s16le"

def find_source_dir():
    for d in CANDIDATE_SOURCE_DIRS:
        if os.path.isdir(d):
            wav_count = len(glob.glob(os.path.join(d, "*.wav")))
            mp3_count = len(glob.glob(os.path.join(d, "*.mp3")))
            if wav_count > 0 or mp3_count > 0:
                return d
    return None

def main():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    dst_dir = os.path.join(project_root, "assets", "sounds")
    os.makedirs(dst_dir, exist_ok=True)

    src_dir = find_source_dir()
    if not src_dir:
        print("Error: Could not locate source audio files in candidate directories.")
        sys.exit(1)

    print(f"Using source audio directory: {src_dir}")
    print(f"Destination audio directory: {dst_dir}")

    # Check ffmpeg availability
    has_ffmpeg = shutil.which("ffmpeg") is not None
    if not has_ffmpeg:
        print("Warning: ffmpeg not found in PATH, will attempt simple copy if files are WAV.")

    # Find all sound files in source directory
    audio_files = []
    for f in os.listdir(src_dir):
        ext = os.path.splitext(f)[1].lower()
        if ext in (".wav", ".mp3", ".pcm", ".raw", ".aiff", ".snd"):
            audio_files.append(f)

    audio_files.sort()
    converted_count = 0

    for fname in audio_files:
        src_path = os.path.join(src_dir, fname)
        base_name = os.path.splitext(fname)[0].lower()
        dst_path = os.path.join(dst_dir, f"{base_name}.wav")

        if has_ffmpeg:
            cmd = [
                "ffmpeg",
                "-y",
                "-v", "error",
                "-i", src_path,
                "-ar", str(TARGET_SAMPLE_RATE),
                "-ac", str(TARGET_CHANNELS),
                "-c:a", TARGET_CODEC,
                dst_path
            ]
            res = subprocess.run(cmd)
            if res.returncode == 0:
                print(f"[OK] Converted: {fname} -> {base_name}.wav (16-bit {TARGET_SAMPLE_RATE}Hz mono)")
                converted_count += 1
            else:
                print(f"[FAIL] ffmpeg error converting {fname}")
        else:
            if fname.lower().endswith(".wav"):
                shutil.copyfile(src_path, dst_path)
                print(f"[COPY] Copied {fname} -> {dst_path}")
                converted_count += 1

    print(f"\nSuccessfully processed {converted_count} audio files in {dst_dir}/")

if __name__ == "__main__":
    main()
