# RTP Client Cache Strategy

## Overview

The RTP client uses a circular frame cache (jitter buffer) to handle network timing variations and ensure smooth video playback. This document describes the caching strategy and its parameters.

## Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| `CACHE_SIZE` | 20 frames | Maximum frames in the circular buffer |
| `FRAME_BUFFER_SIZE` | 256 KB | Maximum size per frame (for HD MJPEG) |
| `MIN_BUFFER_FRAMES` | 10 frames | Frames to buffer before starting playback (~50% of cache) |

## Frame Rates

| Component | Rate | Interval | Purpose |
|-----------|------|----------|----------|
| Server send rate | 30 FPS | 33ms | Video streaming rate |
| Client consume rate | 30 FPS | 33ms | Matches server rate |

Server and client operate at the same frame rate. Stability is achieved through the initial buffer of 10 frames (half the cache size), which provides ~333ms of buffer against network jitter.

## Cache Behavior

### Initial Buffering
1. When playback starts, the cache enters **buffering mode**
2. Frames accumulate until `MIN_BUFFER_FRAMES` (3) are cached
3. Playback begins, status changes from "BUFFERING" to "PLAYING"

### During Playback
1. **Producer** (RTP listener thread): Receives UDP packets, reassembles fragments, adds complete frames to cache
2. **Consumer** (UI thread): Retrieves frames from cache at 30 FPS for display

### Buffer Full Handling
When the cache reaches `CACHE_SIZE` (20 frames):
- **Drop the OLDEST frame** to make room for the new one
- This keeps the stream current rather than falling behind
- Dropped frames are tracked in `stats.frames_dropped`

### Buffer Empty Handling
When the cache becomes empty during playback:
- Return 0 (no frame available)
- UI continues displaying the last frame
- **No re-buffering** - frames will arrive soon
- This prevents stuttering from constant buffering/playing transitions

## Fragment Reassembly

Large JPEG frames are fragmented for UDP transmission (MTU ~1400 bytes).

### Fragment Buffer
- Single frame reassembly buffer
- Tracks received fragments with a 32-bit bitmap
- Handles up to 32 fragments per frame

### Fragment Handling
1. **First fragment arrives**: Initialize reassembly, store total size and fragment count
2. **Subsequent fragments**: Copy to correct offset, mark as received in bitmap
3. **Duplicate detection**: Skip if fragment already received (bitmap check)
4. **Frame complete**: When all fragments received, add to cache
5. **New frame starts**: Abandon any incomplete previous frame

## Thread Safety

All cache operations are protected by `pthread_mutex`:
- `cache.mutex` - Protects frame cache read/write
- `stats_mutex` - Protects statistics counters

## Statistics Tracking

| Statistic | Description |
|-----------|-------------|
| `packets_received` | Total RTP packets received |
| `packets_lost` | Packets detected as lost (sequence gaps) |
| `frames_received` | Complete frames added to cache |
| `frames_dropped` | Frames dropped (buffer full or incomplete) |

## Memory Considerations

Total cache memory usage:
```
CACHE_SIZE × FRAME_BUFFER_SIZE = 20 × 256KB = 5.12 MB
```

The cache is allocated on the stack within `rtp_client_t`. Keep `CACHE_SIZE` reasonable to avoid stack overflow (recommended max: 20-30 frames).

## Tuning Guidelines

### For Higher Latency Networks
- Increase `CACHE_SIZE` to 30-40
- Increase `MIN_BUFFER_FRAMES` to 5-10

### For Lower Latency
- Decrease `CACHE_SIZE` to 10-15
- Keep `MIN_BUFFER_FRAMES` at 3

### For Higher Resolution Video
- Ensure `FRAME_BUFFER_SIZE` is large enough for your frames
- Consider heap allocation if cache size × frame size exceeds safe stack limits
