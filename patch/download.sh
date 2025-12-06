#!/bin/bash
# Download sample MJPEG video files and convert them for streaming

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create samples directory if needed
mkdir -p ../samples

echo "=== Downloading sample video files ==="

BASE_URL="https://filesamples.com/samples/video/mjpeg"

FILES=(
    "sample_640x360.mjpeg"
    "sample_960x540.mjpeg"
    "sample_1280x720.mjpeg"
)

for file in "${FILES[@]}"; do
    if [ -f "../samples/$file" ]; then
        echo "$file already exists, skipping"
    else
        echo "Downloading $file..."
        wget -q --show-progress -O "../samples/$file" "$BASE_URL/$file" || {
            echo "Failed to download $file"
            continue
        }
    fi
done

echo ""
echo "=== Converting files for streaming ==="

for file in "${FILES[@]}"; do
    base="${file%.mjpeg}"
    patched="${base}_patched.mjpeg"

    if [ -f "../samples/$patched" ]; then
        echo "$patched already exists, skipping"
    elif [ -f "../samples/$file" ]; then
        echo "Converting $file -> $patched..."
        python3 converter.py "../samples/$file" "../samples/$patched"
    else
        echo "$file not found, skipping conversion"
    fi
done

echo ""
echo "=== Done! ==="
echo "Sample files are in samples/"
echo ""
echo "To use:"
echo "  ./bin/server 8554"
echo "  ./bin/client 127.0.0.1 8554 9000 sample_640x360_patched.mjpeg"
