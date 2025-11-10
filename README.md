# ZBITX - Software-Defined Radio Transceiver

[![Version](https://img.shields.io/badge/version-3.052-blue.svg)](https://github.com/afarhan/zbitx)
[![License](https://img.shields.io/badge/license-GPL-green.svg)](LICENSE)

ZBITX is an open-source Software-Defined Radio (SDR) transceiver designed for amateur radio enthusiasts. Built to run on Raspberry Pi with custom hardware, it features a GTK-based GUI, web interface, and support for multiple digital communication modes including CW, FT8/FT4, SSB, and more.

## Features

- **Multi-mode Operation**: USB/LSB, CW (Morse), FT8/FT4, NBFM, AM, and digital modes
- **Advanced Signal Processing**: FFT-based filtering, waterfall display, AGC
- **Multiple User Interfaces**:
  - Native GTK+ GUI (800x480 touchscreen optimized)
  - Web-based remote interface
  - Telnet/CLI access
  - Hamlib protocol support
- **Digital Modes**: Full WSJT-X compatible FT8/FT4 implementation
- **QSO Logging**: SQLite-based logbook with Maidenhead grid mapping
- **Hardware Control**: Si5351/Si570 frequency synthesis, OLED display, SWR bridge
- **Remote Operation**: WebSocket server, multicast audio streaming, remote control
- **Contest Features**: Macro system, serial tracking, RBN/DX cluster integration

## Hardware Requirements

- **Raspberry Pi** 4B or newer (with GPIO support)
- **Audio**: ALSA-compatible sound card
- **Display**: 800x480 touchscreen (optional, for GTK GUI)
- **Radio Hardware**:
  - Si5351 or Si570 frequency synthesizer
  - Custom ZBITX RF front-end board
  - Optional: RP2040/Pico for front panel controls
  - Optional: ATTiny85 for SWR bridge
  - Optional: OLED display (I2C)

## Software Dependencies

```bash
# Core libraries
sudo apt-get install -y \
    libgtk-3-dev \
    libfftw3-dev \
    libasound2-dev \
    libsqlite3-dev \
    libncurses5-dev \
    wiringpi \
    pkg-config \
    gcc \
    make
```

## Quick Start

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/afarhan/zbitx.git
   cd zbitx
   ```

2. **Build the FT8 library**:
   ```bash
   cd ft8_lib
   make
   cd ..
   ```

3. **Build the main application**:
   ```bash
   ./build
   ```

4. **Initialize configuration** (first run):
   ```bash
   ./sbitx
   ```

### Configuration

Edit configuration files in the `data/` directory:

- **`sbitx_user_settings.ini`**: User preferences, frequencies, callsign, modes
- **`hw_settings_v*.ini`**: Hardware-specific settings (select appropriate version)
- **Macros**: `*.mc` files for contest/operation automation

### Running

```bash
# Run with GTK GUI
./sbitx

# Access web interface
# Open browser to: http://sbitx.local:8080 or http://[raspberry-pi-ip]:8080
```

## Directory Structure

```
zbitx/
├── sbitx                      # Main compiled executable
├── build                      # Build script
│
├── Core SDR Application (C source files)
├── sbitx.c                    # Main application entry point (1,612 lines)
├── sbitx_gtk.c                # GTK GUI implementation (5,332 lines)
├── sbitx_sound.c              # Audio I/O and processing (1,135 lines)
├── sbitx_utils.c              # Utility functions
├── sdr.h                      # Core SDR architecture definitions (259 lines)
├── sdr_ui.h                   # UI framework contracts
│
├── Radio/Modem Modules
├── vfo.c/h                    # Voltage-controlled oscillator
├── modems.c                   # Modem management and mode switching
├── modem_cw.c/h               # CW (Morse code) keying and iambic keyer
├── modem_ft8.c/h              # FT8 digital mode integration
├── fft_filter.c               # FFT-based signal filtering
│
├── Hardware Control
├── si5351v2.c                 # Si5351 frequency synthesizer driver
├── si570.c                    # Si570 oscillator control
├── i2cbb.c/h                  # I2C bit-banging (GPIO-based I2C)
├── oled.c/h                   # OLED display driver
├── swr_bridge.ino             # ATTiny85 SWR bridge firmware (42 lines)
│
├── Networking & Remote Control
├── mongoose.c/h               # Embedded web server framework (7,508 lines)
├── webserver.c/h              # Web interface backend
├── telnet.c                   # Telnet server for remote control
├── remote.c/h                 # Remote operation protocol
├── wsjtx.c/h                  # WSJT-X integration protocol
│
├── Logging & Database
├── logbook.c                  # QSO logging with SQLite (1,097 lines)
├── ini.c/h                    # INI configuration file parser
├── macros.c                   # Macro/button definitions
│
├── User Interface Components
├── settings_ui.c              # Settings management UI
├── hist_disp.c/h              # Histogram display rendering
├── bm_bmp.c/h                 # Bitmap image handling
│
├── Protocols & Standards
├── hamlib.c/h                 # Hamlib radio control protocol
├── ntputil.c/h                # NTP time synchronization (critical for FT8)
│
├── Data Structures & Utils
├── queue.c/h                  # Ring buffer queue management
├── sound.h                    # Audio device interface definitions
├── ubitx.c                    # Original µBITX radio control (832 lines)
│
├── ft8_lib/                   # FT8/FT4 Encoding/Decoding Library
│   ├── libft8.a               # Compiled library (703 KB)
│   ├── common/                # Core structures
│   ├── fft/                   # KISS FFT implementation
│   │   ├── kiss_fft.c/h       # Complex FFT
│   │   └── kiss_fftr.c/h      # Real FFT
│   ├── ft8/                   # FT8 protocol implementation
│   │   ├── encode.c/h         # Message encoding
│   │   ├── decode.c/h         # Message decoding
│   │   ├── ldpc.c/h           # LDPC error correction
│   │   ├── pack.c/h           # Message packing
│   │   ├── unpack.c/h         # Message unpacking
│   │   ├── crc.c/h            # CRC calculations
│   │   └── constants.c/h      # FT8 constants
│   ├── ft4_ft8_public/        # Reference Fortran implementations
│   └── decode_ft8             # Standalone decoder test tool
│
├── web/                       # Web-Based Interface (1.3 MB)
│   ├── index.html             # Main web UI (71 KB)
│   ├── logbook.html           # QSO logbook web display
│   ├── style.css              # Web styling
│   ├── gridmap.js             # Maidenhead grid mapping (13.6 KB)
│   ├── jogDial.js             # UI dial/knob controls
│   ├── jquery.knob.js         # Knob UI component
│   ├── pcm-player.js          # Audio playback in browser
│   ├── proj4.js               # Coordinate projection library
│   ├── *.mc                   # Web macro files (CW1, FT8, RUN, SP, CQWWRUN)
│   └── src/                   # Mongoose web server source (70+ files)
│
├── data/                      # Configuration & Database
│   ├── sbitx.db               # SQLite logbook database
│   ├── create_db.sql          # Database schema
│   ├── default_settings.ini   # Default user settings
│   ├── default_hw_settings.ini # Hardware settings template
│   ├── hw_settings_v2.ini     # Version 2 hardware config
│   ├── hw_settings_v3.ini     # Version 3 hardware config
│   ├── hw_settings_zbitxv1.ini # ZBITX v1 hardware config
│   └── ft8.mc                 # FT8 macro definitions
│
├── Microcontroller Firmware
├── zbitx_front_panel_sw.ino.uf2  # RP2040/Pico front panel (881 KB)
├── swr_bridge.ino             # ATTiny85 SWR bridge (I2C address #8)
│
└── Documentation
    ├── install.txt            # Complete Raspberry Pi setup guide (159 lines)
    ├── commands.txt           # CLI commands reference
    ├── coding_convention.txt  # C coding standards
    ├── how_gui_is_organized.txt # GUI architecture explanation
    ├── ui_notes.txt           # UI implementation notes
    ├── wsjtx_notes.txt        # WSJT-X integration notes
    ├── config.txt             # GPIO and system configuration
    └── *.pdf                  # Schematics and reference documents
```

## Key Files Explained

### Core Application Files

#### `sbitx.c` (1,612 lines)
Main application entry point. Initializes the SDR system, manages the main event loop, coordinates between GUI/web interfaces and radio hardware.

#### `sbitx_gtk.c` (5,332 lines)
Complete GTK+ graphical user interface implementation. Handles touchscreen input, renders waterfall display, spectrum analyzer, and all UI controls (buttons, knobs, settings panels).

#### `sbitx_sound.c` (1,135 lines)
Audio input/output handling using ALSA. Manages sound card buffers, sample rate conversion, audio routing between RX/TX paths, and multicast streaming for remote listening.

#### `sdr.h` (259 lines)
Core SDR architecture definitions. Contains:
- FFT structures and buffer management
- RX/TX signal chain definitions
- Mode enumerations (USB, LSB, CW, FT8, etc.)
- Filter and queue structures
- Key data structures used throughout the application

### Radio & Signal Processing

#### `vfo.c/h`
Virtual Frequency Oscillator implementation using phase accumulation. Generates I/Q signals for frequency tuning and heterodyning.

#### `fft_filter.c`
FFT-based digital signal processing. Implements frequency-domain filtering using overlap-save method with FFTW3 library. Provides sharp filters without ringing artifacts.

#### `modems.c`
Modem manager that switches between different operating modes (USB, LSB, CW, FT8, etc.). Handles demodulation, audio routing, and mode-specific processing.

#### `modem_cw.c/h`
CW (Morse code) implementation with:
- Iambic keyer (paddle support)
- Adjustable speed and sidetone
- Practice mode
- Automatic timing

#### `modem_ft8.c/h`
FT8/FT4 digital mode integration. Bridges between the main application and the ft8_lib decoder/encoder. Handles time synchronization, audio buffering, and WSJT-X protocol.

### Hardware Drivers

#### `si5351v2.c`
Driver for Si5351 clock generator chip. Controls VFO frequencies using I2C commands. Handles frequency synthesis for TX and RX with PPM calibration.

#### `si570.c`
Alternative frequency synthesizer driver for Si570-based hardware. Provides similar functionality to Si5351 driver.

#### `i2cbb.c/h`
Software I2C implementation using GPIO bit-banging. Used when hardware I2C is unavailable or conflicting. Communicates with OLED displays, frequency synthesizers, and other I2C peripherals.

#### `oled.c/h`
Driver for SSD1306-based OLED displays. Shows frequency, mode, S-meter, and status information on external display.

#### `swr_bridge.ino`
ATTiny85 microcontroller firmware for SWR (Standing Wave Ratio) measurement. Reads forward and reflected power via analog inputs, calculates SWR, and reports via I2C (address #8).

### Networking & Web Interface

#### `mongoose.c/h` (7,508 lines)
Full-featured embedded web server framework. Provides:
- HTTP/HTTPS server
- WebSocket support
- JSON parsing
- DNS, MQTT, NTP clients
- TLS/SSL encryption

#### `webserver.c/h`
Web interface backend. Handles WebSocket connections, serves web UI files, processes remote control commands, streams audio data to web clients.

#### `telnet.c`
Telnet server for command-line remote control. Accepts text commands for frequency changes, mode selection, and status queries. Also used for RBN/DX cluster connectivity.

#### `wsjtx.c/h`
WSJT-X UDP protocol implementation. Allows ZBITX to work with external WSJT-X applications for FT8/FT4 operation. Exchanges frequency, mode, and decode information.

#### `hamlib.c/h`
Hamlib-compatible radio control protocol. Allows third-party applications (fldigi, WSJT-X, logging software) to control ZBITX using standardized commands.

### Database & Configuration

#### `logbook.c` (1,097 lines)
QSO logging system using SQLite3. Features:
- Contact logging with date, time, frequency, mode, signal reports
- Call sign lookup
- Maidenhead grid distance calculation
- Export to ADIF format
- Contest serial number tracking

#### `ini.c/h`
INI file parser for configuration files. Reads/writes settings from `*.ini` files in `data/` directory.

#### `macros.c`
Macro system for contest operation. Allows programmable buttons to send sequences of commands, CW messages, or FT8 messages.

### FT8 Library (`ft8_lib/`)

Self-contained library for encoding and decoding FT8/FT4 messages.

#### `ft8/encode.c/h`
Encodes text messages into FT8 symbols. Handles call sign compression, grid encoding, and signal report packing.

#### `ft8/decode.c/h`
Decodes received FT8 signals. Uses multi-pass detection algorithm to find weak signals buried in noise.

#### `ft8/ldpc.c/h`
Low-Density Parity Check (LDPC) error correction. Provides forward error correction allowing FT8 to work at -24 dB SNR.

#### `ft8/pack.c` & `unpack.c`
Message packing/unpacking. Compresses amateur radio QSO information (call signs, grids, signal reports) into 77-bit payloads.

#### `fft/kiss_fft.c/h`
KISS FFT library - lightweight FFT implementation for embedded systems. Used for spectral analysis in FT8 decoder.

### Web Interface (`web/`)

#### `index.html` (71 KB)
Main web user interface. Full-featured remote control panel with:
- Waterfall display (Canvas-based)
- Frequency knobs and controls
- Mode selection
- Macro buttons
- Real-time status updates via WebSocket

#### `logbook.html`
Web-based logbook viewer. Displays QSO history, allows searching and filtering contacts.

#### `gridmap.js` (13.6 KB)
Maidenhead grid square mapping and visualization. Calculates distances and bearings, displays worked grids on world map.

#### `pcm-player.js`
Web Audio API player for streaming PCM audio from radio to browser. Enables remote listening via web interface.

### Microcontroller Firmware

#### `zbitx_front_panel_sw.ino.uf2` (881 KB)
Compiled firmware for Raspberry Pi Pico (RP2040). Handles front panel controls:
- Rotary encoder for tuning
- Push buttons for mode/function selection
- LED indicators
- USB HID interface to main application

**Installation**: Copy to Pico in bootloader mode (appears as USB drive).

### Documentation Files

#### `install.txt` (159 lines)
Complete step-by-step installation guide:
1. Raspberry Pi OS setup
2. Dependency installation
3. GPIO configuration
4. Audio device setup
5. Building and running ZBITX
6. Network configuration
7. Troubleshooting

#### `commands.txt`
Reference for telnet/CLI commands:
- Frequency control: `F 14074000` (set frequency)
- Mode selection: `M USB`, `M CW`, `M FT8`
- PTT control: `T 1` (transmit), `T 0` (receive)
- Status queries: `?` (get current settings)

#### `how_gui_is_organized.txt`
Explains GTK GUI architecture:
- Widget hierarchy
- Event handling
- Drawing callbacks
- Threading model
- Custom controls (waterfall, spectrum, dials)

## Building from Source

### Build the FT8 Library

```bash
cd ft8_lib
make
cd ..
```

This compiles `libft8.a` used by the main application.

### Build the Main Application

```bash
./build
```

The `build` script compiles all C source files and links them with required libraries:

```bash
gcc -g -o sbitx \
  vfo.c si570.c sbitx_sound.c fft_filter.c sbitx_gtk.c sbitx_utils.c \
  i2cbb.c si5351v2.c ini.c hamlib.c queue.c modems.c logbook.c \
  modem_cw.c settings_ui.c oled.c hist_disp.c ntputil.c \
  telnet.c macros.c modem_ft8.c remote.c mongoose.c webserver.c \
  ft8_lib/libft8.a \
  -lwiringPi -lasound -lm -lfftw3 -lfftw3f -pthread -lsqlite3 \
  -lncurses `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
```

### Build Microcontroller Firmware (Optional)

**For RP2040 front panel**:
1. Open `zbitx_front_panel_sw.ino` in Arduino IDE
2. Select board: "Raspberry Pi Pico"
3. Compile and export UF2
4. Copy to Pico in bootloader mode

**For ATTiny85 SWR bridge**:
1. Open `swr_bridge.ino` in Arduino IDE
2. Select board: "ATtiny85 (8 MHz)"
3. Upload via ISP programmer

## Usage Guide

### Basic Operation

1. **Start the application**:
   ```bash
   ./sbitx
   ```

2. **Select operating mode**: Click mode buttons (USB, LSB, CW, FT8)

3. **Tune to frequency**:
   - Click/drag waterfall display
   - Use tuning knobs
   - Type frequency directly

4. **Transmit**:
   - Press PTT button
   - Use microphone for SSB
   - Use paddle/keyboard for CW
   - Automatic for FT8

### FT8 Operation

1. Set mode to FT8
2. Ensure accurate time (NTP sync): Check system clock
3. Monitor waterfall for signals
4. Click decoded messages to respond
5. Use macros for common responses

### Web Interface

1. **Access**: http://sbitx.local:8080
2. **Remote control**: All functions available via web
3. **Audio streaming**: Enable to listen remotely
4. **Logbook**: View QSOs in browser

### Contest Operation

1. Load contest macros: `*.mc` files in `data/` or `web/`
2. Assign to function keys
3. Program CW messages or FT8 responses
4. Use serial number tracking

### Remote Control

**Via Telnet**:
```bash
telnet sbitx.local 7300
F 14074000      # Set frequency
M FT8           # Set mode
?               # Query status
```

**Via Hamlib**:
```bash
rigctl -m 2 -r localhost:4532
```

## Configuration Files

### `data/sbitx_user_settings.ini`

User preferences and operational settings:

```ini
[RADIO]
frequency=7074000
mode=FT8
callsign=YOURCALL
grid=FN42

[AUDIO]
tx_gain=50
rx_gain=80
mic_gain=50
```

### `data/hw_settings_v*.ini`

Hardware-specific calibration and GPIO mappings:

```ini
[SI5351]
xtal_freq=27000000
ppm_correction=0

[GPIO]
ptt=17
cw_key=27
band_0=5
band_1=6
```

Select the appropriate version for your hardware:
- `hw_settings_v2.ini` - ZBITX Hardware v2
- `hw_settings_v3.ini` - ZBITX Hardware v3
- `hw_settings_zbitxv1.ini` - ZBITX Hardware v1

## Architecture Overview

### SDR Signal Processing Chain

```
ADC → I/Q Sampling → FFT → Frequency-Domain Filter →
Bin Rotation (Tuning) → IFFT → Demodulation → Audio DAC
```

### Key Data Structures

**`struct rx`** (Receiver configuration):
- Frequency, mode, filter bandwidth
- AGC settings
- Signal levels

**`struct vfo`** (Virtual oscillator):
- Phase accumulator
- Frequency word
- I/Q output

**`struct filter`** (Digital filter):
- FFT bins
- Overlap-save buffers
- Filter response

**`struct Queue`** (Audio ring buffer):
- Sample buffer
- Read/write pointers
- Thread-safe operations

## Troubleshooting

### Audio Issues
```bash
# List audio devices
aplay -l
arecord -l

# Test audio
speaker-test -c2 -t wav
arecord -d 5 test.wav && aplay test.wav
```

### I2C Problems
```bash
# Check I2C devices
i2cdetect -y 1

# Verify GPIO access
gpio readall
```

### Web Interface Not Loading
```bash
# Check if sbitx is running
ps aux | grep sbitx

# Verify network
ping sbitx.local

# Check firewall
sudo ufw status
```

### FT8 Not Decoding
- Verify system time is accurate (critical!)
- Check audio levels (should see waterfall activity)
- Ensure USB sound device is configured
- Verify frequency is in FT8 sub-band

## Advanced Topics

### Custom Macros

Create `*.mc` files with commands:

```
# FT8 Contest macro
M FT8
F 14074000
TX CQ TEST YOURCALL FN42
```

### GPIO Mapping

Customize GPIO pins in hardware settings for:
- Band selection relays
- PTT control
- CW keying
- External sensors

### Audio Routing

Configure ALSA for:
- Multi-channel operation
- Loop-back devices
- Virtual audio cables
- Remote audio streaming

## Contributing

ZBITX is open-source software. Contributions are welcome:

1. Fork the repository
2. Create a feature branch
3. Follow coding conventions (see `coding_convention.txt`)
4. Test thoroughly on hardware
5. Submit pull request

## License

GPL - See LICENSE file for details

## Credits

- **Author**: Ashhar Farhan (VU2ESE)
- **FT8 Library**: Based on Karlis Goba's (YL3JG) ft8_lib
- **KISS FFT**: Mark Borgerding
- **Mongoose**: Cesanta Software

## Support & Community

- **Issues**: Report bugs on GitHub Issues
- **Documentation**: See `*.txt` files in repository
- **Hardware**: Schematics available in `*.pdf` files

## Version History

- **3.052** (Current) - CW keying optimization, updated Pico firmware
- **3.0** - Major rewrite with web interface
- **2.x** - GTK GUI implementation
- **1.x** - Initial release based on µBITX

---

**73 de ZBITX** 📻
