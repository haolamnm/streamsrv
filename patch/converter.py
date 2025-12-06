import sys
import os

HEADER_SIZE = 5
MAX_FRAME_SIZE = 99999  # Maximum size representable with 5 ASCII digits

def extract_jpeg_frames(data):
    """
    Extract individual JPEG frames from raw MJPEG data.
    Each frame starts with 0xFF 0xD8 and ends with 0xFF 0xD9.
    Also removes COM (0xFF 0xFE) comment markers if present.
    """
    frames = []
    i = 0

    while i < len(data) - 1:
        # Look for JPEG SOI marker (FF D8)
        if data[i] == 0xFF and data[i+1] == 0xD8:
            frame_data = bytearray()
            frame_data.append(data[i])      # FF
            frame_data.append(data[i+1])    # D8
            i += 2

            # Check if next marker is COM (FF FE) - comment marker, skip it
            if i < len(data) - 3 and data[i] == 0xFF and data[i+1] == 0xFE:
                comment_len = (data[i+2] << 8) | data[i+3]
                i += 2 + comment_len  # Skip FF FE + length bytes + comment data

            # Now copy until we find EOI (FF D9)
            while i < len(data) - 1:
                frame_data.append(data[i])
                if data[i] == 0xFF and data[i+1] == 0xD9:
                    frame_data.append(data[i+1])  # Append D9
                    i += 2
                    break
                i += 1

            frames.append(bytes(frame_data))
        else:
            i += 1

    return frames

def convert_to_length_prefixed_mjpeg(input_file, output_file):
    """
    Convert raw MJPEG (or single JPEG) to length-prefixed MJPEG format.
    Format: [5-byte ASCII length][JPEG data][5-byte ASCII length][JPEG data]...

    This matches the format of movie.mjpeg where each frame is prefixed
    with its size as a 5-character ASCII string (e.g., "06014").
    """
    with open(input_file, 'rb') as f:
        data = f.read()

    # Check if file already has length prefix (starts with ASCII digits)
    if len(data) >= HEADER_SIZE and all(chr(b).isdigit() for b in data[:HEADER_SIZE]):
        print(f"File already appears to be in length-prefixed format!")
        print(f"First {HEADER_SIZE} bytes: {data[:HEADER_SIZE].decode('ascii')}")
        return

    # Extract all JPEG frames
    frames = extract_jpeg_frames(data)

    if not frames:
        print("ERROR: No JPEG frames found in input file!")
        return

    # Check for oversized frames
    oversized_frames = [(i, len(f)) for i, f in enumerate(frames) if len(f) > MAX_FRAME_SIZE]
    if oversized_frames:
        print(f"WARNING: {len(oversized_frames)} frames exceed {MAX_FRAME_SIZE} bytes (5-digit limit)!")
        print(f"  These frames cannot be represented with a 5-byte ASCII header.")
        for idx, size in oversized_frames[:5]:
            print(f"    Frame {idx+1}: {size} bytes")
        if len(oversized_frames) > 5:
            print(f"    ... and {len(oversized_frames) - 5} more")
        print(f"\nOptions:")
        print(f"  1. Use lower resolution video (960x540 or smaller)")
        print(f"  2. Re-encode with higher JPEG compression")
        print(f"  3. The server has been modified to handle variable-length headers")
        print(f"\nContinuing anyway (server supports variable-length headers)...")

    # Build output with length prefixes
    result = bytearray()

    for i, frame in enumerate(frames):
        frame_size = len(frame)
        # Create ASCII length prefix
        # Use exactly 5 digits if possible, otherwise use minimum digits needed
        if frame_size <= MAX_FRAME_SIZE:
            size_str = f"{frame_size:0{HEADER_SIZE}d}"  # Zero-padded 5 digits
        else:
            size_str = str(frame_size)  # Use actual number of digits needed
        result.extend(size_str.encode('ascii'))
        result.extend(frame)

    # Write output
    with open(output_file, 'wb') as out:
        out.write(result)

    print(f"=== CONVERSION COMPLETE ===")
    print(f"Input file: {input_file}")
    print(f"Output file: {output_file}")
    print(f"Original size: {len(data)} bytes")
    print(f"New size: {len(result)} bytes")
    print(f"Frames found: {len(frames)}")

    if frames:
        print(f"\nFrame sizes:")
        for i, frame in enumerate(frames[:5]):  # Show first 5 frames
            size = len(frame)
            if size <= MAX_FRAME_SIZE:
                print(f"  Frame {i+1}: {size} bytes (header: '{size:0{HEADER_SIZE}d}')")
            else:
                print(f"  Frame {i+1}: {size} bytes (header: '{size}') [OVERSIZED]")
        if len(frames) > 5:
            print(f"  ... and {len(frames) - 5} more frames")

    # Verify output format
    print(f"\nVerification:")
    # Parse the header from the generated result buffer directly
    header_len_detected = 0
    for b in result[:20]: # Check first few bytes
        if chr(b).isdigit():
            header_len_detected += 1
        else:
            break
            
    header_str = result[:header_len_detected].decode('ascii')
    print(f"  First header: '{header_str}' (Length: {header_len_detected})")
    print(f"  Bytes after header: {result[header_len_detected]:02X} {result[header_len_detected+1]:02X}")
    
    if header_len_detected == HEADER_SIZE:
        print(f"  Compliant with 5-byte header format")
    else:
        print(f"  WARNING: Header is {header_len_detected} bytes (expected {HEADER_SIZE})")

def verify_mjpeg_format(filename):
    """Verify and display the format of an MJPEG file."""
    with open(filename, 'rb') as f:
        data = f.read(100)

    print(f"=== FILE ANALYSIS: {filename} ===")
    print(f"First 20 bytes (hex): {' '.join(f'{b:02X}' for b in data[:20])}")
    print(f"First 10 bytes (ASCII): {repr(data[:10])}")

    # Check if length-prefixed format
    if len(data) >= HEADER_SIZE and all(chr(b).isdigit() for b in data[:HEADER_SIZE]):
        # Find where digits end
        digit_end = 0
        for i, b in enumerate(data[:20]):
            if chr(b).isdigit():
                digit_end = i + 1
            else:
                break
        size_str = data[:digit_end].decode('ascii')
        print(f"Format: Length-prefixed MJPEG")
        print(f"Header length: {digit_end} bytes")
        print(f"First frame size: {size_str} ({int(size_str)} bytes)")
        if data[digit_end] == 0xFF and data[digit_end+1] == 0xD8:
            print(f"JPEG SOI marker at offset {digit_end}: OK")
        if digit_end == 5:
            print(f"Compliant with 5-byte header format")
        else:
            print(f"Note: Header is {digit_end} bytes (specifies 5 bytes)")
    elif data[0] == 0xFF and data[1] == 0xD8:
        print(f"Format: Raw JPEG/MJPEG (no length prefix)")
        if data[2] == 0xFF and data[3] == 0xFE:
            print(f"Contains COM (comment) marker at offset 2")
        elif data[2] == 0xFF and data[3] == 0xDB:
            print(f"Clean JPEG (DQT at offset 2)")

if __name__ == "__main__":
    if len(sys.argv) >= 3:
        convert_to_length_prefixed_mjpeg(sys.argv[1], sys.argv[2])
    elif len(sys.argv) == 2:
        # Just verify the file format
        verify_mjpeg_format(sys.argv[1])
    else:
        print(f"Usage:")
        print(f"  python {sys.argv[0]} <input.mjpeg> <output.mjpeg>  # Convert to length-prefixed format")
        print(f"  python {sys.argv[0]} <file.mjpeg>                  # Verify file format")
