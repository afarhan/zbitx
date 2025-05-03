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

## Raspberry Pico Integration

The zBITX system can use a Raspberry Pico microcontroller to handle the display and rotary encoder interface. This provides a modular approach to the user interface, separating it from the main signal processing handled by the Raspberry Pi.

### Display and Control Interface

- **TFT Display**: The Pico drives a TFT display using the TFT_eSPI library
- **Rotary Encoder**: Connected to the Pico for frequency tuning and menu navigation
- **I2C Communication**: The Pico communicates with the main Raspberry Pi via I2C
  - Pico acts as an I2C slave with address 0x0A
  - I2C pins used on Pico: SDA on GPIO 6, SCL on GPIO 7

### Communication Protocol

The Raspberry Pi and Pico exchange data using a text-based command protocol over I2C:

1. **Command Format**:
   - Commands use special delimiters: `COMMAND_START` (likely '{') and `COMMAND_END` (likely '}')
   - Format: `{FIELD_LABEL FIELD_VALUE}`
   - Example: `{FREQ 7074000}` to set frequency to 7.074 MHz
   - Example: `{MODE CW}` to set mode to CW

2. **Field Updates**:
   - **Frequency**: Updated whenever the operating frequency changes
   - **Mode**: Sent when mode changes (USB, LSB, CW, etc.)
   - **Filter Settings**: LOW/HIGH filter cutoff values
   - **S-Meter**: Signal strength updates (several times per second)
   - **Power/SWR**: Transmit power and SWR readings during transmission
   - **Waterfall Settings**: SPAN setting for spectrum display width
   - **Status Messages**: Various status updates and notifications
   - Updates typically happen on-demand when values change, not on a fixed schedule

3. **User Input**:
   - **Encoder Movement**: Sent immediately when the encoder is rotated
     - Coarse movements for menu navigation
     - Fine movements for frequency tuning (when FREQ field is selected)
   - **Button Presses**: Sent when encoder button or touch screen buttons are pressed
   - **Touch Events**: Screen coordinates are processed on the Pico and translated to field selections
   - User inputs are prioritized and sent as soon as they occur

4. **Polling Mechanism**:
   - The Raspberry Pi regularly polls the Pico for updates via I2C
   - The Pico responds with any pending user inputs or "NOTHING" if there are none
   - This happens many times per second to ensure responsive UI

### Data Structure

The Pico maintains a comprehensive field structure that represents all UI elements:

- **Field Types**:
  - `FIELD_BUTTON`: Interactive buttons (e.g., mode selection buttons)
  - `FIELD_TEXT`: Text display areas (e.g., status messages)
  - `FIELD_KEY`: Virtual keyboard keys
  - Other specialized types for different UI elements

- **Field Properties**:
  - `label`: Unique identifier for the field (e.g., "FREQ", "MODE")
  - `value`: Current value as text
  - `x`, `y`, `w`, `h`: Position and dimensions on screen
  - `update_to_radio`: Flag indicating the field needs to be sent to the main radio
  - `last_user_change`: Timestamp to manage update conflicts
  - `type`: Field type identifier

- **Update Mechanism**:
  - The `field_set()` function updates field values
  - Changes from the radio set `update_to_radio = false`
  - User interactions set `update_to_radio = true`
  - Fields with `update_to_radio = true` are prioritized in the next I2C response

- **Field Processing**:
  - The `command_tokenize()` function parses incoming commands from the radio
  - The `ui_slice()` function processes user inputs and updates the display
  - The `on_request()` function prepares responses to send back to the radio

This bidirectional communication system allows for a responsive user interface while keeping the complex signal processing on the main Raspberry Pi.

### Hardware Connections

- **Encoder**: Connected to Pico GPIO pins defined as ENC_A, ENC_B, and ENC_S (switch)
- **Display**: Connected via SPI to the Pico
- **Voltage Measurements**: The Pico can read analog values for power, SWR, and battery voltage

This modular approach allows for more flexibility in the UI design and offloads the display handling from the main Raspberry Pi, which can focus on the signal processing tasks.

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

## Display Update Mechanism

This section provides a detailed explanation of how the zBITX implements its real-time spectrum analyzer and waterfall display, including the signal processing pipeline, UI update mechanism, and rendering components.

### Overview of the Display Architecture

The zBITX display system is built on GTK (GIMP Toolkit), a cross-platform widget toolkit for creating graphical user interfaces. GTK provides a comprehensive set of visual elements and drawing primitives that zBITX leverages to create its radio interface. Here's how the display architecture works:

#### GTK Rendering Model

GTK uses a signal-based event-driven architecture where:

1. **Widget Hierarchy**: The UI is organized as a tree of widgets (containers, buttons, drawing areas, etc.)
2. **Event Signals**: User interactions and system events trigger signals that are connected to callback functions
3. **Drawing Model**: GTK uses Cairo, a 2D graphics library, for all rendering operations
4. **Invalidation-Based Rendering**: Rather than continuously redrawing the screen, GTK uses an invalidation model where only areas that need updating are redrawn

#### zBITX's GTK Implementation

In zBITX, the display system is implemented as follows:

1. **Main Window**: Created in `ui_init()` within `sbitx_gtk.c`, this is the top-level container for all UI elements
2. **Drawing Area**: A GTK widget that provides a canvas for custom drawing with Cairo
3. **Field System**: zBITX implements a custom "field" abstraction on top of GTK, where each UI element (spectrum, waterfall, buttons, etc.) is represented as a field with properties like position, size, and drawing functions
4. **Event Handling**: User interactions are captured through GTK event handlers like `on_mouse_press()`, `on_key_press()`, and `on_draw_event()`
5. **Periodic Updates**: A GTK timer (`g_timeout_add()`) calls the `ui_tick()` function periodically to handle time-based updates

The main GTK drawing cycle works as follows:

```
User/System Event → GTK Signal → Callback Function → Invalidate Region → GTK Redraws Region
```

For the spectrum and waterfall displays, this cycle is:

```
Audio Data → FFT Processing → Update Data Structures → Mark Field as Dirty → ui_tick() → invalidate_rect() → GTK Redraw → draw_spectrum()/draw_waterfall()
```

The key advantage of this architecture is efficiency - only the parts of the screen that need updating are redrawn, which is crucial for a real-time application like an SDR interface.

### 1. Signal Processing Pipeline

#### 1.1 Audio Sample Collection

The display update process begins with audio sample collection in the `sound_process()` function in `sbitx.c`. This function acts as a router that directs the audio processing flow:

```c
void sound_process(int32_t *input_rx, int32_t *input_mic, 
                  int32_t *output_speaker, int32_t *output_tx, 
                  int n_samples) {
    if (in_tx)
        tx_process(input_rx, input_mic, output_speaker, output_tx, n_samples);
    else
        rx_linear(input_rx, input_mic, output_speaker, output_tx, n_samples);
}
```

In receive mode, the `rx_linear()` function processes incoming audio samples and initiates the FFT processing that ultimately feeds the spectrum display.

#### 1.2 FFT Processing

The Fast Fourier Transform (FFT) is the core mathematical operation that converts time-domain audio samples into frequency-domain data for the spectrum display. This process involves:

1. **Sample Windowing**: The incoming audio samples are multiplied by a Hann window function to reduce spectral leakage.
2. **FFT Execution**: The windowed samples are processed by FFTW (Fastest Fourier Transform in the West) library.
3. **Spectrum Data Generation**: The raw FFT output is converted to a format suitable for display.

The key code in `rx_linear()` that initiates this process is:

```c
// Apply window function to input samples
for (int i = 0; i < MAX_BINS; i++) {
    __real__ fft_in[i] = input_rx[i] * spectrum_window[i];
    __imag__ fft_in[i] = 0;
}

// Execute FFT
my_fftw_execute(plan_spectrum);

// Update spectrum display data
spectrum_update();
```

#### 1.3 Spectrum Data Generation

The `spectrum_update()` function in `sbitx.c` processes the raw FFT data to make it suitable for display:

```c
void spectrum_update() {
    for (int i = 1269; i < 1803; i++) {
        // Apply exponential averaging for smoother display updates
        fft_bins[i] = ((1.0 - spectrum_speed) * fft_bins[i]) + 
            (spectrum_speed * cabs(fft_spectrum[i]));

        // Convert to dB scale for display
        int y = power2dB(cnrmf(fft_bins[i])); 
        spectrum_plot[i] = y;
    }
}
```

This function:
- Uses a subset of FFT bins (1269-1803) that correspond to the frequency range of interest
- Applies exponential averaging with a configurable `spectrum_speed` parameter to smooth display updates
- Converts magnitude values to decibels for better visualization
- Stores the results in the `spectrum_plot` array, which is used by the UI rendering code

### 2. UI Update Mechanism

#### 2.1 The UI Tick Function

The core of the display update mechanism is the `ui_tick()` function in `sbitx_gtk.c`. This function is called periodically (every millisecond) and handles various UI updates:

```c
gboolean ui_tick(gpointer gook) {
    int static ticks = 0;
    ticks++;
    
    // Process remote commands...
    
    // Update dirty UI fields
    for (struct field *f = active_layout; f->cmd[0] > 0; f++) {
        if (f->is_dirty) {
            if (f->y >= 0) {
                invalidate_rect(f->x, f->y, f->width, f->height);
            }
        }
    }
    
    // Periodic updates (every tick_count ticks)
    if (ticks >= tick_count) {
        // Update spectrum and waterfall displays
        struct field *f = get_field("spectrum");
        update_field(f);
        f = get_field("waterfall");
        update_field(f);
        
        ticks = 0;
    }
    
    return TRUE;
}
```

The update frequency is adjusted based on the current mode:
- CW/CWR modes: 50ms (20 updates per second)
- FT8 mode: 200ms (5 updates per second)
- Other modes: 100ms (10 updates per second)

#### 2.2 Field Invalidation Mechanism

The zBITX UI uses a field-based system where each UI element is represented by a "field" structure. When a field needs to be redrawn, its `is_dirty` flag is set, and the UI tick function will invalidate the corresponding rectangle on the screen:

```c
void invalidate_rect(int x, int y, int width, int height) {
    GdkRectangle r = {x, y, width, height};
    gdk_window_invalidate_rect(gtk_widget_get_window(window), &r, FALSE);
}
```

This triggers the GTK rendering system to redraw that portion of the screen.

#### 2.3 Thread Safety Considerations

Since GTK is not thread-safe, the display update mechanism is designed to handle thread synchronization properly. The `redraw()` function is called from other threads to request a UI update:

```c
void redraw() {
    struct field *f;
    f = get_field("#console");
    f->is_dirty = 1;
    f = get_field("#text_in");
    f->is_dirty = 1;
}
```

This function only sets the `is_dirty` flags, and the actual invalidation and redrawing happen in the main UI thread via the `ui_tick()` function.

### 3. Rendering Components

#### 3.1 Spectrum Display

The spectrum display is rendered by the `draw_spectrum()` function in `sbitx_gtk.c`. This function:

1. Clears the spectrum area
2. Draws the filter bandwidth indicator (showing the current receiver passband)
3. Draws the spectrum grid with frequency markers
4. Plots the spectrum data as a continuous line
5. Draws the tuning needle and pitch indicators
6. Draws the S-meter

The key part that renders the actual spectrum data is:

```c
// Start the plot
cairo_set_source_rgb(gfx, palette[SPECTRUM_PLOT][0], 
    palette[SPECTRUM_PLOT][1], palette[SPECTRUM_PLOT][2]);
cairo_move_to(gfx, f->x + f->width, f->y + grid_height);

float x = 0;
for (i = starting_bin; i <= ending_bin; i++) {
    int y;
    
    // Calculate y-coordinate based on spectrum amplitude
    y = ((spectrum_plot[i] + waterfall_offset) * f->height)/80; 
    
    // Limit y inside the spectrum display box
    if (y < 0)
        y = 0;
    if (y > f->height)
        y = f->height - 1;
        
    // Draw line to this point
    cairo_line_to(gfx, f->x + f->width - (int)x, f->y + grid_height - y);
    
    // Fill the waterfall data
    for (int k = 0; k <= 1 + (int)x_step; k++)
        wf[k + f->width - (int)x] = (y * 100)/grid_height;
        
    x += x_step;
}

cairo_stroke(gfx);
```

This code maps the spectrum data to screen coordinates and draws a line connecting all the data points. It also simultaneously fills the `wf` array with data for the waterfall display.

#### 3.2 Waterfall Display

The waterfall display is rendered by the `draw_waterfall()` function in `sbitx_gtk.c`. This function:

1. Shifts the existing waterfall data down by one row
2. Fills the top row with new data from the `wf` array
3. Maps the data values to colors using a temperature-like color scheme (blue→cyan→green→yellow→red)
4. Renders the waterfall image using a GDK pixbuf

```c
void draw_waterfall(struct field *f, cairo_t *gfx) {
    if (in_tx) {
        draw_tx_meters(f, gfx);
        return;
    }
    
    // Shift existing waterfall data down
    memmove(waterfall_map + f->width * 3, waterfall_map, 
        f->width * (f->height - 1) * 3);
    
    // Fill top row with new data and map to colors
    int index = 0;
    for (int i = 0; i < f->width; i++) {
        int v = wf[i] * 2;
        if (v > 100)
            v = 100;
            
        // Map value to color (blue→cyan→green→yellow→red)
        if (v < 20) {                  // r = 0, g= 0, increase blue
            waterfall_map[index++] = 0;
            waterfall_map[index++] = 0;
            waterfall_map[index++] = v * 12; 
        }
        else if (v < 40) {            // r = 0, increase g, blue is max
            waterfall_map[index++] = 0;
            waterfall_map[index++] = (v - 20) * 12;
            waterfall_map[index++] = 255; 
        }
        // ... other color ranges ...
    }
    
    // Render the waterfall image
    gdk_cairo_set_source_pixbuf(gfx, waterfall_pixbuf, f->x, f->y);        
    cairo_paint(gfx);
    cairo_fill(gfx);
}
```

The waterfall display provides a time-history view of the spectrum, with the most recent data at the top and older data scrolling downward.

#### 3.3 S-Meter Display

The S-meter is rendered by the `draw_smeter()` function in `sbitx_gtk.c`. It:

1. Gets the current S-meter value from the SDR core
2. Separates the value into S-units and additional dB
3. Draws colored boxes representing the S-meter scale (green for normal signal levels, red for strong signals)
4. Adds labels below the boxes showing S-unit values (S1, S3, S5, S7, S9, S9+20dB)

The S-meter value is calculated in the `calculate_s_meter()` function in `sbitx.c`, which converts the signal power to the standard S-unit scale used in amateur radio.

### 4. Data Flow Between Components

The complete data flow for the display update mechanism is:

1. **Audio Samples Collection**:
   - `sound_process()` receives audio samples
   - Routes to `rx_linear()` in receive mode

2. **FFT Processing**:
   - `rx_linear()` applies window function to samples
   - Executes FFT via `my_fftw_execute(plan_spectrum)`
   - Calls `spectrum_update()` to process FFT data

3. **Spectrum Data Generation**:
   - `spectrum_update()` applies averaging to FFT data
   - Converts to dB scale
   - Stores in `spectrum_plot` array

4. **UI Update Triggering**:
   - `ui_tick()` periodically calls `update_field()` for spectrum and waterfall
   - Sets dirty flags and invalidates screen regions

5. **Rendering**:
   - GTK drawing system calls `on_draw_event()`
   - `redraw_main_screen()` is called
   - Calls appropriate drawing functions for each field
   - `draw_spectrum()` renders spectrum using `spectrum_plot` data
   - `draw_waterfall()` renders waterfall using `wf` data

### 5. Performance Considerations

The display update mechanism includes several optimizations:

1. **Exponential Averaging**: The `spectrum_speed` parameter controls how quickly the spectrum display responds to changes, balancing responsiveness against visual stability.

2. **Selective FFT Bin Processing**: Only a subset of FFT bins (1269-1803) are processed, focusing on the frequency range of interest.

3. **Dirty Flag System**: Only UI elements that have changed are redrawn, reducing CPU usage.

4. **Variable Update Rate**: The update frequency is adjusted based on the current mode (CW: 50ms, FT8: 200ms, others: 100ms).

5. **Thread Safety**: The redraw mechanism is designed to work safely with GTK's single-threaded nature, using flags to request updates from other threads.

The zBITX display update mechanism demonstrates good software engineering practices, including separation of concerns (signal processing vs. UI rendering), efficient memory usage, and appropriate threading considerations for real-time performance.

## Hardware Integration

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

## Supported Hardware Versions

The zBITX software supports multiple hardware versions with different configurations. The software automatically detects the hardware version during initialization or reads it from the configuration file.

### Hardware Version Detection

The software identifies the hardware version in the following ways:
1. Reading from the `hw_settings.ini` file if available
2. If not specified in settings, attempting I2C communication with address 0x8:
   - If communication fails, it assumes SBITX_DE (version 0)
   - If communication succeeds, it assumes SBITX_V2 (version 1)

### Version Differences

The zBITX software supports three main hardware versions:

#### 1. SBITX_DE (Version 0)

This is the original "Direct Ethernet" version of the hardware.

**Key characteristics:**
- Uses a simpler T/R (transmit/receive) switching mechanism
- RX_LINE and TX_LINE GPIO pins control the T/R switching
- During T/R transitions:
  - First mutes audio
  - Switches RX_LINE low
  - Delays for debounce
  - Switches TX_LINE high
  - Sets appropriate low-pass filter
- Uses 4 low-pass filters (LPF_A through LPF_D) for different frequency bands

#### 2. SBITX_V2 (Version 1)

This is the second generation of the hardware.

**Key characteristics:**
- Uses the low-pass filters to cut feedback during T/R transitions
- Different T/R switching sequence:
  - First turns off all LPFs
  - Mutes audio
  - Switches TX_LINE high
  - Delays for switching
  - Sets appropriate low-pass filter
- Same 4 low-pass filters as V0

#### 3. SBITX_V4 (Version 4)

This is the latest version with more sophisticated hardware.

**Key characteristics:**
- Uses separate lines for RX and TX powering
- Adds an additional low-pass filter (LPF_E)
- Different T/R switching sequence:
  - Switches RX_LINE low
  - Turns off all LPFs
  - Delays for switching
  - Sets appropriate low-pass filter
  - Switches TX_LINE high
- Modified low-pass filter frequency ranges:
  - LPF_D: < 5.5 MHz
  - LPF_C: 5.5-10.5 MHz
  - LPF_B: 10.5-21.5 MHz (different from V0/V2)
  - LPF_A: 21.5-30 MHz
  - LPF_E: Additional filter used in V4

### Low-Pass Filter Configuration

The low-pass filters are selected based on the operating frequency:

```
if (frequency < 5500000)
    lpf = LPF_D;
else if (frequency < 10500000)        
    lpf = LPF_C;
else if (frequency < 21500000 && sbitx_version >= 4)
    lpf = LPF_B;
else if (frequency < 18500000)        
    lpf = LPF_B;
else if (frequency < 30000000) 
    lpf = LPF_A;
```

Note that V4 has a different frequency range for LPF_B compared to earlier versions.

## Development Environment Requirements

The zBITX software is designed specifically for the Raspberry Pi hardware platform and has several hardware-specific dependencies that make development on a standard Linux machine challenging.

### Raspberry Pi Dependencies

1. **WiringPi Library**: The software extensively uses the WiringPi library for GPIO control, which is specific to Raspberry Pi. This is used for:
   - Controlling GPIO pins for T/R switching
   - Managing I2C communication with the Si5351 clock generator
   - Reading encoder inputs and switches
   - Interrupt handling for user inputs

2. **AudioInjector Sound Card**: The software is designed to work with the WM8731 audio codec via the AudioInjector sound card, which requires specific hardware configuration.

3. **I2C Hardware**: The software communicates with Si5351 clock generators and other I2C devices using Raspberry Pi's I2C interface.

4. **GPIO Pin Configuration**: The software expects specific GPIO pin configurations as defined in the installation instructions.

### Development Options

Based on the examination of the codebase, there is no built-in simulation or emulation mode that would allow development on a standard Linux machine without the required hardware. The software does not have:

1. Hardware abstraction layers that would allow mock implementations
2. Conditional compilation options for development without hardware
3. Simulation modes for testing without physical hardware

### Potential Development Approaches

If you need to develop or modify the zBITX software, the following approaches are possible:

1. **Raspberry Pi Development**: The most straightforward approach is to develop directly on a Raspberry Pi with the required hardware components.

2. **Remote Development**: Set up a Raspberry Pi with the hardware and develop remotely using SSH or remote development tools.

3. **Custom Hardware Abstraction**: For experienced developers, it might be possible to create a hardware abstraction layer that mocks the WiringPi and hardware interfaces, but this would require significant effort and is not supported by the existing codebase.

### Required Development Tools

To build the zBITX software, you need:

1. GCC compiler and standard build tools
2. GTK3 development libraries
3. ALSA audio libraries
4. FFTW3 library for FFT processing
5. SQLite3 for database functionality
6. WiringPi library for GPIO control
7. NCurses for terminal UI elements

The build command (from the `build` script) shows these dependencies:

```bash
gcc -g -o sbitx *.c ft8_lib/ft8_lib.a -lwiringPi -lasound -lm -lfftw3 -lfftw3f -pthread -lncurses -lsqlite3 -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0
```

In conclusion, while the zBITX software can be built on any Linux system with the required libraries, actual development and testing require a Raspberry Pi with the appropriate hardware components due to the direct hardware dependencies in the codebase.

## License

This software is provided as open-source. Please refer to the included license files for specific terms.

## Credits

zBITX is developed by a community of amateur radio enthusiasts. The project builds upon various open-source libraries and tools including FFTW3, WiringPi, GTK, and others.
