import axios from 'axios';

// Use relative URL - Vite proxy will forward to localhost:8080
const api = axios.create({
  baseURL: '/',
});

export const getApiInfo = async () => {
  const response = await api.get('/');
  return response.data;
};

export const listModels = async () => {
  const response = await api.get('/api/models');
  return response.data;
};

export const listFolderModels = async (folderName) => {
  const response = await api.get(`/api/models/folder/${encodeURIComponent(folderName)}`);
  return response.data;
};

export const uploadModels = async (file, onProgress) => {
  const formData = new FormData();
  formData.append('file', file);

  const response = await api.post('/api/models/upload', formData, {
    headers: {
      'Content-Type': 'multipart/form-data',
    },
    onUploadProgress: (progressEvent) => {
      if (onProgress) {
        const percentCompleted = Math.round(
          (progressEvent.loaded * 100) / progressEvent.total
        );
        onProgress(percentCompleted);
      }
    },
  });

  return response.data;
};

export const loadModel = async (modelPath, slot = 0) => {
  const response = await api.post('/api/models/load', {
    modelPath,
    slot,
  });
  return response.data;
};

export const getStatus = async () => {
  const response = await api.get('/api/status');
  return response.data;
};

export const setActiveSlot = async (slot) => {
  const response = await api.post('/api/models/activate', {
    slot,
  });
  return response.data;
};

export const setActiveSlotL = async (slot) => {
  const response = await api.post('/api/models/activate-l', {
    slot,
  });
  return response.data;
};

export const setActiveSlotR = async (slot) => {
  const response = await api.post('/api/models/activate-r', {
    slot,
  });
  return response.data;
};

export const setBypassModelL = async (bypass) => {
  const response = await api.post('/api/settings/bypass-model-l', {
    bypass,
  });
  return response.data;
};

export const setBypassModelR = async (bypass) => {
  const response = await api.post('/api/settings/bypass-model-r', {
    bypass,
  });
  return response.data;
};

export const setInputGain = async (gain) => {
  const response = await api.post('/api/settings/input-gain', {
    gain,
  });
  return response.data;
};

export const setOutputGain = async (gain) => {
  const response = await api.post('/api/settings/output-gain', {
    gain,
  });
  return response.data;
};

export const setBypassModel = async (bypass) => {
  const response = await api.post('/api/settings/bypass-model', {
    bypass,
  });
  return response.data;
};

// Stereo mode
export const setStereoMode = async (mode) => {
  const response = await api.post('/api/settings/stereo-mode', {
    mode,
  });
  return response.data;
};

export const setStereo2MonoMixL = async (level) => {
  const response = await api.post('/api/settings/stereo2mono-mix-l', {
    level,
  });
  return response.data;
};

export const setStereo2MonoMixR = async (level) => {
  const response = await api.post('/api/settings/stereo2mono-mix-r', {
    level,
  });
  return response.data;
};

// Per-channel gain
export const setInputGainL = async (gain) => {
  const response = await api.post('/api/settings/input-gain-l', {
    gain,
  });
  return response.data;
};

export const setInputGainR = async (gain) => {
  const response = await api.post('/api/settings/input-gain-r', {
    gain,
  });
  return response.data;
};

export const setOutputGainL = async (gain) => {
  const response = await api.post('/api/settings/output-gain-l', {
    gain,
  });
  return response.data;
};

export const setOutputGainR = async (gain) => {
  const response = await api.post('/api/settings/output-gain-r', {
    gain,
  });
  return response.data;
};

// Per-channel noise gate
export const setNoiseGateL = async (enabled, threshold) => {
  const response = await api.post('/api/settings/noise-gate-l', {
    enabled,
    threshold,
  });
  return response.data;
};

export const setNoiseGateR = async (enabled, threshold) => {
  const response = await api.post('/api/settings/noise-gate-r', {
    enabled,
    threshold,
  });
  return response.data;
};

// Per-channel EQ
export const setEQEnabledL = async (enabled) => {
  const response = await api.post('/api/settings/eq-enabled-l', {
    enabled,
  });
  return response.data;
};

export const setEQEnabledR = async (enabled) => {
  const response = await api.post('/api/settings/eq-enabled-r', {
    enabled,
  });
  return response.data;
};

export const setEQBassL = async (gain) => {
  const response = await api.post('/api/settings/eq-bass-l', {
    gain,
  });
  return response.data;
};

export const setEQMidL = async (gain) => {
  const response = await api.post('/api/settings/eq-mid-l', {
    gain,
  });
  return response.data;
};

export const setEQTrebleL = async (gain) => {
  const response = await api.post('/api/settings/eq-treble-l', {
    gain,
  });
  return response.data;
};

export const setEQBassR = async (gain) => {
  const response = await api.post('/api/settings/eq-bass-r', {
    gain,
  });
  return response.data;
};

export const setEQMidR = async (gain) => {
  const response = await api.post('/api/settings/eq-mid-r', {
    gain,
  });
  return response.data;
};

export const setEQTrebleR = async (gain) => {
  const response = await api.post('/api/settings/eq-treble-r', {
    gain,
  });
  return response.data;
};

export const setEQEnabled = async (enabled) => {
  const response = await api.post('/api/settings/eq-enabled', {
    enabled,
  });
  return response.data;
};

export const setEQBass = async (gain) => {
  const response = await api.post('/api/settings/eq-bass', {
    gain,
  });
  return response.data;
};

export const setEQMid = async (gain) => {
  const response = await api.post('/api/settings/eq-mid', {
    gain,
  });
  return response.data;
};

export const setEQTreble = async (gain) => {
  const response = await api.post('/api/settings/eq-treble', {
    gain,
  });
  return response.data;
};

export const setNoiseGateEnabled = async (enabled) => {
  const response = await api.post('/api/settings/noise-gate-enabled', {
    enabled,
  });
  return response.data;
};

export const setNoiseGateThreshold = async (threshold) => {
  const response = await api.post('/api/settings/noise-gate-threshold', {
    threshold,
  });
  return response.data;
};

export const startRecording = async (filename = null) => {
  const response = await api.post('/api/recording/start', {
    filename: filename || '',
  });
  return response.data;
};

export const stopRecording = async () => {
  const response = await api.post('/api/recording/stop');
  return response.data;
};

export const listRecordings = async () => {
  const response = await api.get('/api/recordings');
  return response.data;
};

export const deleteRecording = async (filename) => {
  const response = await api.delete(`/api/recordings/${filename}`);
  return response.data;
};

export const getRecordingURL = (filename) => {
  return `/api/recordings/${filename}`;
};

export const setReverbEnabled = async (enabled) => {
  const response = await api.post('/api/settings/reverb-enabled', {
    enabled,
  });
  return response.data;
};

export const setReverbRoomSize = async (size) => {
  const response = await api.post('/api/settings/reverb-room-size', {
    size,
  });
  return response.data;
};

export const setReverbDecayTime = async (seconds) => {
  const response = await api.post('/api/settings/reverb-decay-time', {
    seconds,
  });
  return response.data;
};

export const setReverbMix = async (dry, wet) => {
  const response = await api.post('/api/settings/reverb-mix', {
    dry,
    wet,
  });
  return response.data;
};

export const listAudioDevices = async () => {
  const response = await api.get('/api/audio/devices');
  return response.data;
};

export const getCurrentAudioDevice = async () => {
  const response = await api.get('/api/audio/current');
  return response.data;
};

export const setAudioDevice = async (device) => {
  const response = await api.post('/api/audio/device', {
    device,
  });
  return response.data;
};

export default api;
