# zBITX: Software Defined Radio (SDR) for Raspberry Pi

zBITX is a Software Defined Radio (SDR) implementation designed to run on Raspberry Pi hardware. It provides a complete amateur radio transceiver solution with a GTK-based graphical user interface, digital mode support, and integration with various hardware components.

## Overview

zBITX is built as a software-defined radio that interfaces with specific hardware components to create a complete amateur radio transceiver. The software handles signal processing, user interface, and various radio modes including SSB, CW, AM, and digital modes like FT8.

## Features

- **Multiple Operating Modes**: Supports USB, LSB, CW, CWR, NBFM, AM, FT8, and other digital modes
- **Spectrum Display**: Real-time FFT-based spectrum analyzer and waterfall display
- **Digital Mode Support**: Built-in FT8 support with integration capabilities
- **Logbook**: Integrated QSO logging functionality with SQLite database
- **Remote Operation**: Web interface for remote control
- **Hamlib Support**: Compatible with standard ham radio control software
- **CW Keyer**: Built-in CW keyer functionality
- **Customizable UI**: Adjustable interface with various display options
- **Band Memory**: Remembers settings for different frequency bands
- **Audio Processing**: DSP-based audio filtering and processing

## Hardware Requirements

The zBITX software is designed to work with specific hardware components:

- **Raspberry Pi** (tested on Raspberry Pi 3 and 4)
- **Si5351** clock generator for frequency synthesis
- **WM8731** audio codec (via AudioInjector sound card)
- **I2C interface** for controlling oscillators and other peripherals
- **GPIO pins** for PTT, CW keying, and other control functions
- **OLED display** support (optional)

The software expects specific GPIO pin configurations as defined in the installation instructions.

## Source Files

The project consists of numerous source files, each handling specific functionality:

### Core SDR Components
- `sbitx.c` - Main SDR implementation with signal processing
- `sbitx_sound.c` - Audio handling and processing
- `fft_filter.c` - FFT-based filtering implementation
- `vfo.c` - Virtual frequency oscillator implementation

### User Interface
- `sbitx_gtk.c` - GTK-based user interface
- `sbitx_utils.c` - Utility functions for the UI
- `hist_disp.c` - Histogram display functionality
- `settings_ui.c` - Settings interface

### Hardware Interfaces
- `i2cbb.c` / `i2cbb.h` - I2C bit-banging implementation
- `si5351v2.c` - Si5351 clock generator control
- `si570.c` - Si570 oscillator control
- `oled.c` - OLED display interface

### Digital Modes
- `modem_cw.c` - CW modem implementation
- `modem_ft8.c` - FT8 digital mode support
- `ft8_lib/` - FT8 protocol implementation library

### Connectivity
- `hamlib.c` - Hamlib integration for rig control
- `webserver.c` / `mongoose.c` - Web server for remote control
- `remote.c` - Remote control functionality
- `telnet.c` - Telnet interface

### Utilities
- `ini.c` - INI file parsing
- `logbook.c` - QSO logging functionality
- `queue.c` - Queue implementation for data handling
- `ntputil.c` - NTP time synchronization utilities

## Building and Installation

### Prerequisites

Before building zBITX, you need to install several dependencies:

```bash
# Update system
sudo apt update
sudo apt upgrade

# Install required libraries
sudo apt-get install ncurses-dev
sudo apt-get install libasound2-dev
sudo apt-get install libgtk-3-dev
sudo apt-get install libgtk+-3-dev
sudo apt-get install libsqlite3-dev
sudo apt-get install ntp ntpstat

# Install wiringPi
cd /tmp
wget https://project-downloads.drogon.net/wiringpi-latest.deb
sudo dpkg -i wiringpi-latest.deb

# Install FFTW3
# Download from www.fftw.org and follow these steps:
./configure
make
sudo make install
# For single precision library:
./configure --enable-float
make
sudo make install
```

### Audio Configuration

zBITX requires specific audio configuration:

1. Edit `/etc/pulse/client.conf` and add:
   ```
   autospawn = no
   daemon-binary = /bin/true
   ```

2. Set up audio loopback:
   ```bash
   sudo modprobe snd-aloop enable=1,1,1 index=1,2,3
   ```

3. To make loopback permanent, add to `/etc/rc.local`:
   ```
   sudo modprobe snd-aloop enable=1,1,1 index=1,2,3
   ```

### GPIO Configuration

Edit `/boot/config.txt` and add:

```
gpio=4,17,27,22,10,9,11,5,6,13,26,16,12,7,8,25,24=ip,pu
gpio=24,23=op,pu
dtoverlay=audioinjector-wm8731-audio
avoid_warnings=1
```

Also, comment out the built-in audio:
```
#dtparam=audio=on
```

### Building zBITX

To build the zBITX software:

1. Clone the repository (if you haven't already)
2. Navigate to the zBITX directory
3. Build the FT8 library:
   ```bash
   cd ft8_lib
   make
   make install
   cd ..
   ```
4. Build the main program:
   ```bash
   ./build sbitx
   ```

### Database Setup

The logbook functionality requires an SQLite database:

```bash
cd data
sqlite3 sbitx.db < create_db.sql
cd ..
```

### Web Interface Setup

To enable the web interface on port 80:

```bash
sudo iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-port 8080
sudo iptables -t nat -I OUTPUT -p tcp -d 127.0.0.1 --dport 80 -j REDIRECT --to-ports 8080
sudo apt-get install iptables-persistent --fix-missing
```

## Running zBITX

After building, you can run zBITX with:

```bash
./sbitx
```

## Low-Level Transmission Process

This section provides a detailed explanation of how the zBITX implements RF transmission at a low level, including the signal path, hardware control, and measurement systems.

### Transmission Signal Path

The transmission process in zBITX follows these steps:

1. **Audio Input Acquisition**: 
   - Audio samples are captured from the microphone input at 96 kHz sampling rate
   - For digital modes (CW, FT8), the audio is generated in software

2. **FFT Processing**:
   - The audio samples are collected in a buffer of 2048 samples (MAX_BINS)
   - The previous 1024 samples are retained for overlap processing
   - The samples are converted to the frequency domain using FFT (Fast Fourier Transform)
   - The `fftw_execute(plan_fwd)` function performs this transformation

3. **Sideband Filtering**:
   - The frequency domain signal is filtered according to the selected mode
   - For USB: Frequencies below the carrier are zeroed out
   - For LSB: Frequencies above the carrier are zeroed out
   - For AM: Both sidebands are retained
   - The filter is applied by multiplying with the `tx_filter->fir_coeff` array

4. **Frequency Shifting**:
   - The filtered signal is shifted to the appropriate transmit frequency
   - This is done by rotating the FFT bins by `tx_shift` positions
   - The result is stored in the `fft_freq` array

5. **Inverse FFT**:
   - The frequency domain signal is converted back to the time domain
   - The `fftw_execute(r->plan_rev)` function performs this transformation

6. **Output Scaling**:
   - The time domain samples are scaled by several factors:
     - `volume`: User-controlled volume setting
     - `tx_amp`: Band-specific power scaling factor
     - `alc_level`: Automatic Level Control to prevent overdriving

7. **Modulation Display Update**:
   - The `sdr_modulation_update()` function captures the modulation envelope
   - This is used to display the modulation level in the UI

### Transmit/Receive Switching

The T/R (Transmit/Receive) switching process is critical for proper operation. The zBITX supports three hardware versions with different T/R mechanisms:

#### SBITX_DE (Original Design)

When switching to transmit mode (`tr_switch_de(1)`):

1. Set RX_LINE (GPIO 16) LOW to disable receive path
2. Mute audio output and input by setting mixer levels to 0
3. Set `mute_count` to 20 to prevent audio glitches
4. Reset FFT processing with `tx_process_restart = 1`
5. Set transmit power levels with `set_tx_power_levels()`
6. If using band relays, select appropriate low-pass filter with `set_lpf_40mhz()`
7. Set TX_LINE (GPIO 24) HIGH to enable transmit path
8. Reset spectrum display with `spectrum_reset()`

When switching back to receive mode (`tr_switch_de(0)`):

1. Set `in_tx` flag to 0
2. Mute audio to prevent switching transients
3. Reset FFT bins with `fft_reset_m_bins()`
4. Set `mute_count` to prevent audio glitches
5. Reset low-pass filter relays if used
6. Set TX_LINE (GPIO 24) LOW to disable transmit path
7. Restore audio mixer levels for reception
8. Reset spectrum display
9. Set RX_LINE (GPIO 16) HIGH to enable receive path

### CW Tone Generation

CW (Morse code) tones are generated entirely in software:

1. **Tone Oscillator**:
   - A virtual oscillator (`cw_tone`) is configured to the user-selected pitch (default 700 Hz)
   - The `vfo_read(&cw_tone)` function generates a sine wave at this frequency

2. **Envelope Shaping**:
   - To prevent key clicks, the CW tone is shaped with rise and fall times
   - A separate envelope oscillator (`cw_env`) creates a smooth envelope
   - The envelope is applied by multiplying the tone by the envelope value

3. **Timing Control**:
   - Dot duration is set by `cw_period` (calculated from WPM setting)
   - Dash duration is 3x dot duration
   - Inter-element spacing is 1x dot duration
   - Inter-character spacing is 3x dot duration
   - Inter-word spacing is 7x dot duration

4. **Keying Methods**:
   - **Straight Key**: Directly follows the state of the key input
   - **Iambic Mode A**: Alternates between dots and dashes
   - **Iambic Mode B**: Similar to Mode A but with different timing behavior

5. **Key Input Sources**:
   - Hardware keys connected to GPIO pins (PTT on GPIO 7, DASH on GPIO 21)
   - Software keying through the UI
   - Text input that is converted to Morse code

### FT8 Tone Generation

FT8 uses 8-FSK (Frequency Shift Keying) modulation:

1. **Message Encoding**:
   - The text message is encoded into a 77-bit payload using `pack77()`
   - The payload is then encoded with error correction into 79 FSK tones

2. **Tone Synthesis**:
   - Tones are generated using GFSK (Gaussian Frequency Shift Keying)
   - The `synth_gfsk()` function creates the waveform
   - Each symbol is 0.16 seconds long (for FT8) or 0.048 seconds (for FT4)
   - The base frequency is configurable, with 8 possible tone frequencies

3. **Timing**:
   - Transmissions are synchronized to start at 0 or 30 seconds of each minute
   - NTP time synchronization ensures accurate timing
   - Total transmission time is 12.6 seconds for FT8

### Power Measurement and ALC

The zBITX includes a sophisticated power measurement and control system:

1. **Power Bridge Reading**:
   - Forward and reflected power are read from an I2C-connected power bridge
   - The I2C address is 0x8, and 4 bytes are read (2 for forward, 2 for reflected)

2. **SWR Calculation**:
   - SWR (Standing Wave Ratio) is calculated from forward and reflected power
   - The formula used is: `vswr = (10*(vfwd + vref))/(vfwd-vref)`
   - If reflected power exceeds forward power, SWR is set to 100

3. **Power Calculation**:
   - Forward power is calculated as: `fwdpower = (fwdvoltage * fwdvoltage)/400`
   - This gives power in 1/10th of a watt (400 = 40 watts)

4. **Automatic Level Control (ALC)**:
   - If RF voltage exceeds 135 (approximately 40W), ALC reduces the output level
   - The `alc_level` factor is adjusted to maintain safe power levels
   - This prevents overdriving the power amplifier

5. **Band Power Calibration**:
   - Each amateur band has a specific power scaling factor
   - The `calibrate_band_power()` function automatically determines these factors
   - Calibration data is stored in `hw_settings.ini`

### Low-Pass Filter Selection

To ensure clean output and comply with regulations, the appropriate low-pass filter is selected based on frequency:

1. The `set_lpf_40mhz()` function sets GPIO pins to select the correct filter
2. Filter selection is based on the operating frequency
3. GPIO pins 5, 6, 10, 11, and 26 control the filter selection
4. The filter selection occurs during the T/R switching process

### Complete Transmission Sequence

When the user initiates transmission (by pressing PTT, sending CW, or starting an FT8 transmission):

1. The appropriate mode-specific function is called (e.g., `ft8_tx()` for FT8)
2. The T/R switch is activated with `tr_switch(1)`
3. The Si5351 is configured to the transmit frequency
4. Audio samples are generated (from mic or digitally for CW/FT8)
5. The `tx_process()` function processes these samples
6. The processed samples are sent to the audio output
7. Power and SWR are continuously monitored
8. When transmission ends, `tr_switch(0)` returns to receive mode

This process ensures clean, efficient transmission with appropriate filtering, power control, and timing for all supported modes.

# zBITX Hardware Integration

This document provides detailed information about the hardware integration in the zBITX software-defined radio (SDR) system, including the chips used, GPIO connections, and how digital modes like CW and FT8 are implemented.

## Hardware Architecture Overview

The zBITX is a software-defined radio that runs on a Raspberry Pi, interfacing with specific hardware components to create a complete amateur radio transceiver. The system uses a combination of dedicated chips, GPIO pins, and audio interfaces to implement both transmit and receive functionality across multiple amateur radio bands.

## Key Hardware Components

### 1. Si5351 Clock Generator

The Si5351 is a programmable clock generator that serves as the primary frequency synthesis device in the zBITX.

**Implementation Details:**
- **I2C Address**: 0x60
- **Control Method**: I2C bit-banging through GPIO pins
- **Clock Outputs**:
  - CLK0: Used for receiver local oscillator
  - CLK1: Used for BFO (Beat Frequency Oscillator) at 40.035 MHz
  - CLK2: Used for transmitter local oscillator
- **Frequency Range**: 3.5 MHz to 30 MHz (amateur radio bands)
- **Reference Oscillator**: 25 MHz TCXO (Temperature Compensated Crystal Oscillator)

The Si5351 is configured to generate the necessary frequencies for both receiving and transmitting. For reception, it provides the local oscillator signal that mixes with the incoming RF to produce the intermediate frequency (IF). For transmission, it generates the carrier frequency.

### 2. WM8731 Audio Codec

The WM8731 is a high-quality audio codec used for audio processing in the zBITX.

**Implementation Details:**
- **Interface**: I2S (Integrated Interchip Sound)
- **Sampling Rate**: 96 kHz
- **Resolution**: 24-bit
- **Connection**: Via AudioInjector sound card for Raspberry Pi
- **Configuration**: Enabled in `/boot/config.txt` with `dtoverlay=audioinjector-wm8731-audio`

The WM8731 handles all audio input and output for the zBITX, including microphone input for transmitting and speaker output for receiving.

### 3. GPIO Interface

The Raspberry Pi's GPIO pins are used for various control functions in the zBITX.

## GPIO Pin Assignments

| GPIO Pin | Function | Direction | Description |
|----------|----------|-----------|-------------|
| 4 | General Input | Input (Pull-up) | Used for control functions |
| 5 | LPF Band Select | Input (Pull-up) | Low-pass filter band selection |
| 6 | LPF_B | Input (Pull-up) | Low-pass filter band selection |
| 7 | PTT | Input (Pull-up) | Push-to-Talk input |
| 8 | General Input | Input (Pull-up) | Used for control functions |
| 9 | LPF_A | Input (Pull-up) | Low-pass filter band selection |
| 10 | LPF_C | Input (Pull-up) | Low-pass filter band selection |
| 11 | LPF_D | Input (Pull-up) | Low-pass filter band selection |
| 12 | ENC1_B | Input (Pull-up) | Encoder 1 B input |
| 13 | ENC1_A | Input (Pull-up) | Encoder 1 A input |
| 14 | ENC1_SW | Input (Pull-up) | Encoder 1 switch input |
| 16 | RX_LINE | Input (Pull-up) | Receive line control |
| 17 | General Input | Input (Pull-up) | Used for control functions |
| 21 | DASH | Input (Pull-up) | CW dash input |
| 22 | SCL / SW5 | Input (Pull-up) | I2C clock line / Switch 5 |
| 23 | SDA | Output (Pull-up) | I2C data line |
| 24 | TX_LINE | Output (Pull-up) | Transmit line control |
| 25 | General Input | Input (Pull-up) | Used for control functions |
| 26 | LPF_E | Input (Pull-up) | Low-pass filter band selection |
| 27 | General Input | Input (Pull-up) | Used for control functions |

### Front Panel Controls

The zBITX supports front panel controls through GPIO pins:

**Encoders:**
- **Encoder 1**: Used for frequency tuning
  - A: GPIO 13
  - B: GPIO 12
  - Switch: GPIO 14
- **Encoder 2**: Used for menu navigation and parameter adjustment
  - A: GPIO 0
  - B: GPIO 2
  - Switch: GPIO 3

**CW Keying:**
- PTT: GPIO 7
- DASH: GPIO 21

## Hardware Versions

The zBITX software supports multiple hardware versions with different T/R (transmit/receive) switching mechanisms:

1. **SBITX_DE (0)**: Original design with basic T/R switching
2. **SBITX_V2 (1)**: Uses LPFs to cut feedback during T/R transitions
3. **SBITX_V4 (4)**: Uses separate lines for RX and TX powering

The hardware version is specified in the `hw_settings.ini` file.

## Low-Pass Filter Control

The zBITX uses GPIO pins to control band-specific low-pass filters:

| Band | LPF_A | LPF_B | LPF_C | LPF_D | LPF_E |
|------|-------|-------|-------|-------|-------|
| < 5 MHz | LOW | LOW | LOW | LOW | LOW |
| 5-8 MHz | HIGH | LOW | LOW | LOW | LOW |
| 8-12 MHz | LOW | HIGH | LOW | LOW | LOW |
| 12-15 MHz | HIGH | HIGH | LOW | LOW | LOW |
| 15-20 MHz | LOW | LOW | HIGH | LOW | LOW |
| 20-22 MHz | HIGH | LOW | HIGH | LOW | LOW |
| 22-24 MHz | LOW | HIGH | HIGH | LOW | LOW |
| > 24 MHz | HIGH | HIGH | HIGH | LOW | LOW |

## I2C Communication

The zBITX implements I2C communication using bit-banging on GPIO pins:

- **SCL**: GPIO 22
- **SDA**: GPIO 23

This I2C interface is used to communicate with the Si5351 clock generator and potentially other I2C devices.

## Audio Configuration

The zBITX uses a complex audio routing setup to enable both internal processing and external program integration:

1. **Hardware Audio**: Uses the WM8731 codec via the AudioInjector sound card
2. **Audio Loopback**: Uses the Linux `snd-aloop` kernel module to create virtual sound cards
3. **Audio Routing**:
   - Card 0: Physical audio hardware (mic/speaker)
   - Card 1-3: Virtual loopback devices for digital mode integration

## Digital Mode Implementation

### CW (Morse Code) Implementation

CW operation in zBITX is implemented through a combination of software and hardware:

#### CW Transmission:

1. **Tone Generation**: 
   - CW tones are generated in software using a sine wave generator
   - The tone frequency is determined by the user-configurable pitch setting (default 700 Hz)
   - The tone is shaped with rise and fall times to prevent key clicks

2. **Keying Methods**:
   - **Direct Keying**: Using GPIO pins (PTT on GPIO 7, DASH on GPIO 21)
   - **Software Keying**: Through the user interface or external programs
   - **Iambic Keyer**: Software implementation of an iambic keyer with adjustable speed

3. **Signal Path**:
   - The CW tone is generated in the `cw_tx_get_sample()` function
   - The tone is processed through the transmit chain
   - The Si5351 is set to the operating frequency
   - T/R switching is handled by the GPIO pins

#### CW Reception:

1. **Signal Processing**:
   - Incoming audio is filtered around the CW pitch frequency
   - Signal magnitude is calculated and compared to a threshold
   - A denoising algorithm removes short glitches

2. **Decoding**:
   - The decoder tracks mark/space timing to determine dots and dashes
   - Symbols are matched against a lookup table to decode characters
   - Adaptive speed tracking adjusts to the sender's speed

### FT8 Implementation

FT8 is a digital mode that uses 8-FSK modulation with 15-second transmission cycles.

#### FT8 Transmission:

1. **Message Encoding**:
   - Messages are encoded using the `pack77()` function from the FT8 library
   - The binary message is then encoded into a sequence of 79 FSK tones

2. **Tone Generation**:
   - Tones are generated using GFSK (Gaussian Frequency Shift Keying)
   - The `synth_gfsk()` function creates the waveform with proper shaping
   - Symbol period is 0.16 seconds (for FT8) or 0.048 seconds (for FT4)
   - Base frequency is configurable, with tones spaced at specific intervals

3. **Timing**:
   - Transmissions are synchronized to even or odd 15-second intervals
   - NTP time synchronization is used for accurate timing

#### FT8 Reception:

1. **Signal Processing**:
   - Audio is captured at 12 kHz sampling rate
   - FFT processing creates a waterfall display
   - The `monitor_process()` function analyzes the spectrum

2. **Decoding**:
   - The FT8 library's decoding functions identify potential messages
   - LDPC error correction is applied to recover the message
   - Decoded messages are displayed and can trigger automated responses

## Power Calibration

The zBITX includes a power calibration system to maintain consistent output power across all bands:

1. Each band has a power scaling factor stored in the `band_power` array
2. The `calibrate_band_power()` function automatically determines the optimal scaling factor
3. Power readings are taken from the forward power sensor
4. Calibration data is stored in the `hw_settings.ini` file

## Transmit/Receive Switching

The T/R switching mechanism varies by hardware version:

1. **SBITX_DE**:
   - Uses GPIO 16 (RX_LINE) and GPIO 24 (TX_LINE)
   - Includes audio muting during transitions
   - Implements a delay sequence to prevent hot switching

2. **SBITX_V2**:
   - Uses LPFs to reduce feedback during transitions
   - Implements a more sophisticated switching sequence

3. **SBITX_V4**:
   - Uses separate control lines for RX and TX
   - Designed for improved isolation and performance

## Hardware Integration Summary

The zBITX represents a sophisticated integration of software and hardware components to create a full-featured SDR transceiver. The key elements are:

1. **Frequency Generation**: Si5351 clock generator
2. **Audio Processing**: WM8731 codec via AudioInjector
3. **Control Interface**: GPIO pins for T/R switching, band selection, and user input
4. **Software Processing**: FFT-based signal processing on the Raspberry Pi
5. **Digital Modes**: Software implementation of CW and FT8/FT4

This architecture provides a flexible platform that can be adapted to different hardware configurations while maintaining consistent functionality through the software layer.

## License

This software is provided as open-source. Please refer to the included license files for specific terms.

## Credits

zBITX is developed by a community of amateur radio enthusiasts. The project builds upon various open-source libraries and tools including FFTW3, WiringPi, GTK, and others.
