## What is HoopiPi?

HoopiPi is an open-source, real-time audio processing engine designed specifically for guitar amplifier modeling using Neural Amp Modeler (NAM) technology. 

### Key Highlights

- ✅ **Neural Amp Modeler Support**: Full WaveNet and LSTM model compatibility
- ✅ **Flexible Stereo Processing**: Multiple stereo modes for guitar and microphone
- ✅ **Web-Based Control**: Modern, responsive UI accessible from any device
- ✅ **Effects Chain**: Noise gate, 3-band EQ, and algorithmic reverb
- ✅ **Recording & Playback**: Built-in WAV recording and MP3 backing track support
- ✅ **Model Management**: Upload model ZIPs directly from [ToneHunt](https://tonehunt.org)

---

## 🎛️ Audio Processing Pipeline

```
┌─────────────┐
│ Audio Input │ ──► JACK Audio Server (Low Latency)
└─────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────┐
│              Per-Channel Processing                   │
├──────────────────────────────────────────────────────┤
│  1. Input Gain (-20 to +20 dB)                       │
│  2. Noise Gate (per-channel, -60 to 0 dB threshold)  │
│  3. Neural Amp Model (NAM WaveNet/LSTM)              │
│  4. 3-Band EQ (per-channel parametric)               │
│     • Bass: 120 Hz (-20 to +20 dB)                   │
│     • Mid: 750 Hz (-20 to +20 dB)                    │
│     • Treble: 3 kHz (-20 to +20 dB)                  │
│  5. Output Gain (-20 to +20 dB)                      │
└──────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────┐
│           Stereo Effects (Post-Processing)            │
├──────────────────────────────────────────────────────┤
│  • FDN Reverb (Feedback Delay Network)               │
│    - Room Size: 0.0 to 1.0                           │
│    - Decay Time: 0.0 to 1.0                          │
│    - Wet/Dry Mix: 0.0 to 1.0                         │
└──────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────┐
│              Recording & Playback                     │
├──────────────────────────────────────────────────────┤
│  • WAV Recording (lossless, 48kHz)                   │
│  • MP3 Backing Tracks (with position controls)       │
└──────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────┐
│ Audio Output │ ──► Headphones/Speakers/DAW
└──────────────┘
```

<img width="768" height="854" alt="image" src="https://github.com/user-attachments/assets/a595b0bf-9e7c-4c67-a2de-e3d89bc174bc" />

### Performance
I have a pi-sound card. CPU loading is 30%-45% on the Pi 4 for most NAM models (reverb adds about 5%):

<img width="810" height="592" alt="image" src="https://github.com/user-attachments/assets/4e210483-1147-4544-842d-439b411fb89d" />

On the 3b+ Pi, it is not great but bearable :-):

<img width="2416" height="1776" alt="image" src="https://github.com/user-attachments/assets/49c2e27d-f812-41ea-ad61-7e8cb245f6b4" />

## Settings

Audio device settings and service management can be done via the UI:

<img width="403" height="866" alt="image" src="https://github.com/user-attachments/assets/812eb410-f479-4474-8a21-aa24e6cbe61d" />

Zip files containing NAM models (downloaded from tone3000.com) can be directly imported:

<img width="608" height="430" alt="image" src="https://github.com/user-attachments/assets/36fdb616-0291-4c13-b999-816e175f8e0a" />

## Installation

HoopiPi is distributed as a Debian package for one-command installation:

```bash
sudo apt install --no-install-recommends ./hoopi-pi_1.0.0-XX_arm64_cortex-aXX.deb
```

**What Gets Installed:**
- ✅ HoopiPi audio engine and API server
- ✅ JACK audio server with automatic configuration
- ✅ Web interface (accessible on port 11995)
- ✅ Systemd services (auto-start on boot)
- ✅ Audio device detection and setup
- ✅ Real-time audio priority configuration

**Post-Install:**
- Services start automatically
- Access web UI at `http://YOUR_PI_IP:11995`
- Upload your first NAM model and start playing!

---

