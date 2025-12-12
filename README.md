<img width="768" height="854" alt="image" src="https://github.com/user-attachments/assets/a595b0bf-9e7c-4c67-a2de-e3d89bc174bc" />
<img width="810" height="592" alt="image" src="https://github.com/user-attachments/assets/4e210483-1147-4544-842d-439b411fb89d" />
<img width="403" height="866" alt="image" src="https://github.com/user-attachments/assets/812eb410-f479-4474-8a21-aa24e6cbe61d" />

<img width="608" height="430" alt="image" src="https://github.com/user-attachments/assets/36fdb616-0291-4c13-b999-816e175f8e0a" />




# HoopiPi Debian Package

## Building the Package

### Build Prerequisites

Install required build tools and development libraries:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  git \
  libjack-jackd2-dev \
  libasound2-dev \
  libsndfile1-dev \
  libmpg123-dev \
  nodejs \
  npm \
  dpkg-dev
```

**Package descriptions:**
- `build-essential` - C/C++ compiler toolchain (gcc, g++, make)
- `cmake` - Build system generator
- `git` - Version control (if cloning from repository)
- `libjack-jackd2-dev` - JACK Audio Connection Kit development headers
- `libasound2-dev` - ALSA library development headers
- `libsndfile1-dev` - Audio file I/O library development headers
- `libmpg123-dev` - MP3 decoding library development headers
- `nodejs` - JavaScript runtime for web UI build
- `npm` - Node.js package manager for web UI dependencies
- `dpkg-dev` - Debian package development tools

### Build Steps

1. Build the project:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
cd ..
```

2. Build the web UI:
```bash
cd web-ui
npm install
npm run build
cd ..
```

3. Build the .deb package:
```bash
./build-deb.sh
```

### Build Script Options

The `build-deb.sh` script supports version management:

```bash
./build-deb.sh              # Auto-increment build number (default)
./build-deb.sh --version X.Y.Z-BUILD  # Set specific version
./build-deb.sh --major      # Bump major version (X.0.0-1)
./build-deb.sh --minor      # Bump minor version (x.Y.0-1)
./build-deb.sh --patch      # Bump patch version (x.y.Z-1)
./build-deb.sh --no-increment   # Build without changing version
./build-deb.sh --help       # Show all options
```

The script automatically:
- Detects architecture (arm64, armhf, amd64)
- Detects CPU model (cortex-a53, cortex-a72, cortex-a76)
- Injects build metadata into systemd service files
- Strips debug symbols from binaries
- Creates optimized package name with CPU information

## Package Structure

```
debian/
├── DEBIAN/
│   ├── control          # Package metadata
│   ├── postinst         # Post-installation script
│   └── prerm            # Pre-removal script
├── usr/local/bin/       # Binaries (HoopiPi, HoopiPiAPI)
├── usr/local/share/     # Web UI and models
│   └── hoopi-pi/
│       ├── web-ui/      # Web interface files
│       └── models/      # NAM model files
├── lib/systemd/system/  # Systemd service files
│   ├── hoopi-jack.service
│   ├── hoopi-engine.service
│   └── hoopi-api.service
└── etc/security/limits.d/
    └── 99-hoopi-pi-audio.conf  # RT audio settings
```

## Installation

```bash
sudo apt-get install ./hoopi-pi_1.0.0-XX_arm64_cortex-aXX.deb
```

Note: The actual package name will include the version, architecture, and CPU model (e.g., `hoopi-pi_1.0.0-19_arm64_cortex-a76.deb`).

The installer creates a detailed log at `~/.config/hoopi-pi/install.log` that captures:
- All installation steps with timestamps
- Package version and user information
- Directory creation and file operations
- JACK configuration status
- Systemd service setup
- Any errors or warnings

You can review this log later to troubleshoot issues or verify the installation.

## What Gets Installed

- **Binaries**:
  - `/usr/local/bin/HoopiPi` - Main audio processing engine
  - `/usr/local/bin/HoopiPiAPI` - Web API server
  - `/usr/local/bin/hoopi-check-jack` - JACK diagnostic tool
- **Web UI**: `/usr/local/share/hoopi-pi/web-ui/` - Web interface files
- **Models**: `/usr/local/share/hoopi-pi/models/` - Pre-installed NAM models (if included)
- **Services**: User systemd services in `~/.config/systemd/user/`
  - `hoopi-jack.service` - JACK audio server
  - `hoopi-engine.service` - HoopiPi audio processing engine
  - `hoopi-api.service` - Web API server
- **User Data**: `~/.local/share/hoopi-pi/`
  - `models/` - User NAM models
  - `recordings/` - Audio recordings
  - `backing-tracks/` - MP3 backing tracks
  - `web-ui/` - Copy of web interface
- **Config**: `~/.config/hoopi-pi/`
  - `install.log` - Installation log
  - `uninstall.log` - Uninstallation log (if removed)
  - Configuration files and settings
- **JACK Config**: `~/.jackdrc` - JACK audio server configuration
- **RT Audio Limits**: `/etc/security/limits.d/99-hoopi-pi-audio.conf` - Real-time priority settings

## Post-Installation

The postinst script automatically:
1. Adds the user to the `audio` group (for real-time priority)
2. Configures RT audio limits (rtprio=95, memlock=unlimited)
3. Detects available audio devices and creates JACK configuration at `~/.jackdrc`
   - Filters out unwanted devices (bcm2835 headphones, etc.)
   - Auto-selects if only one suitable device found
   - Prompts for selection if multiple devices available
   - Preserves existing `.jackdrc` if present
4. Creates HoopiPi configuration directories
5. Copies web UI files and pre-installed models to user directories
6. Installs and enables systemd user services
7. Enables user lingering (services run without login)
8. Starts all HoopiPi services automatically
9. Displays all LAN IP addresses for web UI access

### If JACK is Already Installed

The installer is smart about existing JACK installations:

- **Existing `.jackdrc`**: Your configuration is preserved and used by HoopiPi
- **JACK already running**: The installer detects this and provides instructions
- **Multiple JACK instances**: You can't run multiple JACK servers simultaneously

If you have JACK running (e.g., via QjackCtl or manually), you have two options:

**Option 1: Use your existing JACK setup**
```bash
# Don't start hoopi-jack service, just start the engine and API
systemctl --user start hoopi-engine
systemctl --user start hoopi-api
```

**Option 2: Switch to HoopiPi's JACK service**
```bash
# Stop existing JACK
killall jackd
# Or if using QjackCtl, stop it there

# Start HoopiPi's JACK service
systemctl --user start hoopi-jack
systemctl --user start hoopi-engine
systemctl --user start hoopi-api
```

## Checking JACK Status

A helpful diagnostic tool is included:

```bash
hoopi-check-jack
```

This will show:
- JACK installation status
- Whether JACK is currently running (and by which process)
- JACK configuration (.jackdrc contents)
- HoopiPi service status
- Audio group membership
- RT audio limits configuration

## Starting Services

After installation and logging out/in:

```bash
# Start all services
systemctl --user start hoopi-jack
systemctl --user start hoopi-engine
systemctl --user start hoopi-api

# Check status
systemctl --user status hoopi-jack
systemctl --user status hoopi-engine
systemctl --user status hoopi-api

# View logs
journalctl --user -u hoopi-jack -f
journalctl --user -u hoopi-engine -f
journalctl --user -u hoopi-api -f
```

## Accessing the Web UI

After starting the services, access the web interface at:
```
http://localhost:11995
```

## Configuring Audio Device

Edit `~/.jackdrc` to configure your audio device:

```bash
# Example for USB audio interface
/usr/bin/jackd -dalsa -dhw:USB -r48000 -p128 -n2

# Example for Raspberry Pi built-in audio
/usr/bin/jackd -dalsa -dhw:0 -r48000 -p128 -n2
```

After changing, restart the JACK service:
```bash
systemctl --user restart hoopi-jack
systemctl --user restart hoopi-engine
```

## Uninstallation

```bash
sudo apt-get remove hoopi-pi
```

The uninstaller:
- Stops all HoopiPi services (hoopi-jack, hoopi-engine, hoopi-api)
- Disables all services
- Creates an uninstall log at `~/.config/hoopi-pi/uninstall.log`
- **Preserves** your configuration and data directories

### Preserved After Uninstall

The following directories are NOT removed to protect your data:
- `~/.config/hoopi-pi/` - Configuration files, settings, install/uninstall logs
- `~/.local/share/hoopi-pi/` - NAM models, recordings, backing tracks
- `~/.jackdrc` - JACK configuration (may be used by other applications)

To completely remove HoopiPi including all data:
```bash
sudo apt-get remove hoopi-pi
rm -rf ~/.config/hoopi-pi
rm -rf ~/.local/share/hoopi-pi
# Optional: Remove JACK config if not using JACK for other purposes
# rm ~/.jackdrc
```

## Dependencies

The package depends on:
- `jackd2` - JACK Audio Connection Kit server
- `libasound2` - ALSA library
- `libjack-jackd2-0` - JACK client library
- `libsndfile1` - Audio file I/O library
- `libmpg123-0` - MP3 decoding library (for backing tracks)
- `alsa-utils` - ALSA utilities (provides `aplay` for audio device detection)

Optional:
- `qjackctl` - JACK control GUI (not required, but useful for manual JACK management)

All required dependencies will be automatically installed by apt-get.
