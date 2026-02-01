# Sandpiper ALSA Audio Driver

## Overview

This ALSA driver provides audio playback support for the Sandpiper APU (Audio Processing Unit). The APU is a hardware audio device that uses a ping-pong buffer architecture with DMA to play audio samples.

## Hardware Specifications

- **Base Address**: 0x40000000
- **Register Size**: 4KB (0x1000)
- **Sample Rate**: 44.1 KHz (fixed)
- **Format**: 16-bit stereo (S16_LE)
- **Buffer Size**: 512 stereo samples (2048 bytes) per period
- **Architecture**: Ping-pong buffer (dual-buffer DMA)

## APU Architecture

The APU uses an internal dual-port sample memory with two buffers:
- Each buffer holds 512 stereo samples (2KB)
- Total internal memory: 8KB (2 × 4KB)
- The APU alternates between buffers automatically
- A frame counter toggles each time the APU switches buffers

### Synchronization

The driver monitors the APU's buffer swap by polling the frame counter (bit 0 of status register at offset 0). When the counter changes, it indicates:
1. The APU has finished playing the previous buffer
2. The APU has switched to the other buffer
3. The driver can now submit the next buffer via DMA

This polling is done in a high-resolution timer callback (2ms interval) to ensure timely buffer updates without interrupts.

## ALSA Device

After loading the driver, you should see:
```bash
cat /proc/asound/cards
```

Output:
```
0 [sandpiper     ]: Sandpiper - Sandpiper APU
                     Sandpiper Audio Processing Unit at 0x40000000
```

## Usage Examples

### Play a WAV file
```bash
aplay -D hw:0,0 /path/to/audio.wav
```

### Test with speaker-test
```bash
speaker-test -D hw:0,0 -c 2 -r 44100 -F S16_LE
```

### Play audio with ALSA utilities
```bash
# Set default device
cat > /etc/asound.conf <<EOF
pcm.!default {
    type hw
    card 0
}

ctl.!default {
    type hw
    card 0
}
EOF

# Now you can use aplay without -D option
aplay /path/to/audio.wav
```

## Limitations

1. **Playback Only**: No recording/capture support
2. **Fixed Sample Rate**: 44.1 KHz only
3. **Fixed Format**: 16-bit stereo only (SNDRV_PCM_FMTBIT_S16_LE)
4. **No Interrupts**: Uses polling timer instead of interrupt-driven operation
5. **Buffer Size**: Fixed period size of 512 samples (2048 bytes)

## Troubleshooting

### Driver not loading
Check kernel log:
```bash
dmesg | grep sandpiper
```

### No sound output
1. Check if device is present:
   ```bash
   cat /proc/asound/pcm
   ```

2. Check mixer settings (if applicable):
   ```bash
   alsamixer
   ```

3. Verify DMA buffer alignment (should see in dmesg on load)

4. Check APU register access:
   ```bash
   devmem 0x40000000
   ```

### Playback issues
- Ensure audio file is 44.1 KHz, 16-bit stereo
- Try resampling if needed:
  ```bash
  ffmpeg -i input.wav -ar 44100 -ac 2 -sample_fmt s16 output.wav
  aplay output.wav
  ```

## Developer Notes

### APU Command Interface

The APU accepts commands via 32-bit writes to the base address:

1. **Set Buffer Size**: 
   - Write `0x00000000` (APUCMD_BUFFERSIZE)
   - Write `0x00000004` (size code for 512 samples)

2. **Start DMA**:
   - Write `0x00000001` (APUCMD_START)
   - Write DMA physical address (must be 16-byte aligned)

3. **Set Sample Rate**:
   - Write `0x00000004` (APUCMD_SETRATE)
   - Write `0x00000000` (44.1 KHz) or `0x00000003` (Halt)

4. **Read Status**:
   - Read from base address
   - Bit 0: Frame counter (toggles on buffer swap)
   - Bits 10-1: Word count (current buffer position)

### Timer-Based Operation

The driver uses a high-resolution timer (hrtimer) instead of interrupts:
- Timer period: 2ms
- Checks APU frame counter on each timer tick
- When frame counter changes, submits next DMA buffer
- Calls `snd_pcm_period_elapsed()` to notify ALSA

This approach is reliable and doesn't require interrupt routing in the hardware.

## Building

The driver is built as a kernel module via Yocto/PetaLinux:

```bash
petalinux-build -c sandpiper-alsa
```

## License

GPL-2.0-only
