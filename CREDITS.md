# Credits

HoopiPi is built on top of excellent open-source projects. We are grateful to all contributors.

## Core Audio Processing

### NeuralAudio
- **Repository**: https://github.com/mikeoliphant/NeuralAudio
- **Contributors**: [GitHub contributors](https://github.com/mikeoliphant/NeuralAudio/graphs/contributors)
- **Description**: C++ library for real-time neural network audio processing

### Neural Amp Modeler Core
- **Repository**: https://github.com/sdatkinson/NeuralAmpModelerCore
- **Description**: Reference implementation for Neural Amp Modeler models

### RTNeural
- **Repository**: https://github.com/jatinchowdhury18/RTNeural
- **Description**: Real-time neural network inference library

### RTNeural-NAM
- **Repository**: https://github.com/jatinchowdhury18/RTNeural-NAM
- **Description**: NAM-optimized variant of RTNeural

### math_approx
- **Repository**: https://github.com/jatinchowdhury18/math_approx
- **Description**: Fast approximations for mathematical functions

## Mathematics & Linear Algebra

### Eigen
- **Repository**: https://gitlab.com/libeigen/eigen
- **Description**: C++ template library for linear algebra

### xsimd
- **Repository**: https://github.com/xtensor-stack/xsimd
- **Description**: C++ SIMD wrapper library (used by RTNeural)

## Audio Libraries

### JACK Audio Connection Kit
- **Website**: https://jackaudio.org/
- **Description**: Professional audio server for low-latency audio routing

### libsndfile
- **Repository**: https://github.com/libsndfile/libsndfile
- **Description**: C library for reading and writing audio files (WAV, AIFF, etc.)

### libmpg123
- **Repository**: https://sourceforge.net/projects/mpg123/
- **Description**: Fast and accurate MPEG audio decoder (MP3 support)

### ALSA (Advanced Linux Sound Architecture)
- **Website**: https://www.alsa-project.org/
- **Description**: Linux kernel sound card drivers and libraries

## DSP & Effects

### Signalsmith Audio DSP Library
- **Repository**: https://signalsmith-audio.co.uk/code/dsp/
- **Author**: Geraint Luff / Signalsmith Audio Ltd.
- **License**: MIT
- **Description**: C++11 header-only library for audio signal-processing tasks
- **Components Used**: Delay lines, filters, and other DSP building blocks

### Feedback Delay Network (FDN) Reverb Algorithm
- **Article**: https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/
- **Author**: Geraint Luff / Signalsmith Audio
- **Description**: Algorithmic reverb implementation using multi-channel feedback delays with mixing matrices
- **Note**: HoopiPi's reverb is based on this reverb-example-code using the Signalsmith DSP library

## Web Server & API

### cpp-httplib
- **Repository**: https://github.com/yhirose/cpp-httplib
- **Description**: Single-header C++ HTTP/HTTPS server and client library

### nlohmann/json
- **Repository**: https://github.com/nlohmann/json
- **Description**: JSON for Modern C++ - JSON parser and serializer

## Web UI

### React
- **Repository**: https://github.com/facebook/react
- **Description**: JavaScript library for building user interfaces

### Material-UI (MUI)
- **Repository**: https://github.com/mui/material-ui
- **Description**: React UI components implementing Material Design

### Apache ECharts
- **Repository**: https://github.com/apache/echarts
- **Description**: Powerful charting and visualization library

### echarts-for-react
- **Repository**: https://github.com/hustcc/echarts-for-react
- **Description**: React wrapper for Apache ECharts

### Axios
- **Repository**: https://github.com/axios/axios
- **Description**: Promise-based HTTP client for JavaScript

### Vite
- **Repository**: https://github.com/vitejs/vite
- **Description**: Next-generation frontend build tool

## Build Tools

### CMake
- **Website**: https://cmake.org/
- **Description**: Cross-platform build system generator

---

Please see individual repositories for license information. HoopiPi respects and complies with all upstream licenses.
