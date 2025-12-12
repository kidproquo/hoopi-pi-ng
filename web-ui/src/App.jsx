import { useState, useEffect, useRef } from 'react';
import {
  Container,
  AppBar,
  Toolbar,
  Typography,
  Box,
  Paper,
  Grid,
  Alert,
  Snackbar,
  CssBaseline,
  ThemeProvider,
  createTheme,
  Button,
  Stack,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  IconButton,
} from '@mui/material';
import { Upload, Close } from '@mui/icons-material';
import ModelList from './components/ModelList';
import SlotStatus from './components/SlotStatus';
import InputGainControl from './components/InputGainControl';
import OutputGainControl from './components/OutputGainControl';
import NAMModelControl from './components/NAMModelControl';
import EQControls from './components/EQControls';
import NoiseGateControls from './components/NoiseGateControls';
import ReverbControls from './components/ReverbControls';
import Recordings from './components/Recordings';
import BackingTrackControl from './components/BackingTrackControl';
import MetricsHistory from './components/MetricsHistory';
import StereoModeSelector from './components/StereoModeSelector';
import Stereo2MonoMixControls from './components/Stereo2MonoMixControls';
import AudioDeviceSettings from './components/AudioDeviceSettings';
import { listModels, uploadModels, loadModel, getStatus, setActiveSlot, setActiveSlotL, setActiveSlotR, setInputGain, setOutputGain, setBypassModel, setBypassModelL, setBypassModelR, setStereoMode, setStereo2MonoMixL, setStereo2MonoMixR, setInputGainL, setInputGainR, setOutputGainL, setOutputGainR, setEQEnabledL, setEQEnabledR, setEQBassL, setEQMidL, setEQTrebleL, setEQBassR, setEQMidR, setEQTrebleR, setEQEnabled, setEQBass, setEQMid, setEQTreble, setNoiseGateL, setNoiseGateR, setNoiseGateEnabled, setNoiseGateThreshold, setReverbEnabled, setReverbRoomSize, setReverbDecayTime, setReverbMix, startRecording, stopRecording, listRecordings, deleteRecording } from './api';

const darkTheme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#90caf9',
    },
    secondary: {
      main: '#f48fb1',
    },
    background: {
      default: '#121212',
      paper: '#1e1e1e',
    },
  },
});

function App() {
  const [models, setModels] = useState([]);
  const [status, setStatus] = useState(null);
  const [statusError, setStatusError] = useState(null);
  const [loading, setLoading] = useState(false);
  const [recordings, setRecordings] = useState([]);
  const [snackbar, setSnackbar] = useState({ open: false, message: '', severity: 'info' });
  const [modelModalOpen, setModelModalOpen] = useState(false);
  const [settingsModalOpen, setSettingsModalOpen] = useState(false);
  const [selectedSlot, setSelectedSlot] = useState(0);
  const [metricsHistoryOpen, setMetricsHistoryOpen] = useState(false);
  const [metricsHistory, setMetricsHistory] = useState([]);
  const [modelChanges, setModelChanges] = useState([]);
  const [bypassChanges, setBypassChanges] = useState([]);
  const lastActiveModelRef = useRef(null);
  const lastModelNamesRef = useRef(null);
  const lastBypassStateRef = useRef(null);

  const fetchModels = async () => {
    try {
      setLoading(true);
      const data = await listModels();
      // Store entire hierarchical structure
      setModels(data);
    } catch (error) {
      showSnackbar('Failed to fetch models: ' + error.message, 'error');
    } finally {
      setLoading(false);
    }
  };

  const fetchStatus = async () => {
    try {
      const data = await getStatus();
      if (data && data.success !== false) {
        setStatus(data);
        setStatusError(null);

        // Record metrics history (limit to 3 hours)
        const now = Date.now();
        const threeHoursAgo = now - (3 * 60 * 60 * 1000);

        setMetricsHistory(prev => {
          const newEntry = {
            timestamp: now,
            cpuLoad: data.cpuLoad || 0,
            cpuTemp: data.cpuTemp !== undefined ? data.cpuTemp : -1,
            memoryUsage: data.memoryUsage !== undefined ? data.memoryUsage : -1,
            xruns: data.xruns || 0,
          };

          // Add new entry and filter out old entries (> 3 hours)
          const updated = [...prev, newEntry].filter(entry => entry.timestamp >= threeHoursAgo);
          return updated;
        });

        // Detect active model changes
        if (data.modelNames && data.activeModel !== undefined) {
          const currentActiveModel = data.activeModel;
          const currentModelNames = data.modelNames;

          // Initialize refs on first run
          if (lastActiveModelRef.current === null) {
            lastActiveModelRef.current = currentActiveModel;
            lastModelNamesRef.current = currentModelNames;
          } else if (lastActiveModelRef.current !== currentActiveModel) {
            // Active model changed - record it with before/after
            const previousSlot = lastActiveModelRef.current;
            const newSlot = currentActiveModel;
            const previousModel = lastModelNamesRef.current[previousSlot] || 'None';
            const newModel = currentModelNames[newSlot] || 'None';

            setModelChanges(prev => {
              const newChange = {
                timestamp: now,
                previousSlot: previousSlot,
                newSlot: newSlot,
                previousModel: previousModel,
                newModel: newModel,
              };

              // Keep model changes within 3 hours
              const updated = [...prev, newChange].filter(change => change.timestamp >= threeHoursAgo);
              return updated;
            });

            // Update refs
            lastActiveModelRef.current = currentActiveModel;
            lastModelNamesRef.current = currentModelNames;
          } else {
            // Active model didn't change, but update model names in case they were loaded
            lastModelNamesRef.current = currentModelNames;
          }
        }

        // Detect NAM bypass state changes
        if (data.bypassModel !== undefined) {
          const currentBypassState = data.bypassModel;
          const currentActiveModel = data.activeModel;
          const currentModelNames = data.modelNames;

          // Initialize ref on first run
          if (lastBypassStateRef.current === null) {
            lastBypassStateRef.current = currentBypassState;
          } else if (lastBypassStateRef.current !== currentBypassState) {
            // Bypass state changed - record it
            const modelName = (currentModelNames && currentModelNames[currentActiveModel]) || 'None';

            setBypassChanges(prev => {
              const newChange = {
                timestamp: now,
                bypassed: currentBypassState,
                modelName: modelName,
                slot: currentActiveModel,
              };

              // Keep bypass changes within 3 hours
              const updated = [...prev, newChange].filter(change => change.timestamp >= threeHoursAgo);
              return updated;
            });

            // Update ref
            lastBypassStateRef.current = currentBypassState;
          }
        }
      } else {
        setStatusError(data?.error || 'Unknown error');
      }
    } catch (error) {
      console.error('Failed to fetch status:', error);
      setStatusError(error.message);
      // Keep previous status visible instead of clearing it
    }
  };

  const fetchRecordings = async () => {
    try {
      const data = await listRecordings();
      if (data && data.success) {
        setRecordings(data.recordings || []);
      }
    } catch (error) {
      console.error('Failed to fetch recordings:', error);
    }
  };

  useEffect(() => {
    fetchModels();
    fetchStatus();
    fetchRecordings();

    // Poll status every 2 seconds
    const interval = setInterval(fetchStatus, 2000);
    return () => clearInterval(interval);
  }, []);

  const showSnackbar = (message, severity = 'info') => {
    setSnackbar({ open: true, message, severity });
  };

  const handleCloseSnackbar = () => {
    setSnackbar({ ...snackbar, open: false });
  };

  const handleUpload = async (event) => {
    const file = event.target.files?.[0];
    if (!file) return;

    try {
      setLoading(true);
      const result = await uploadModels(file, (progress) => {
        console.log('Upload progress:', progress);
      });

      if (result.success) {
        showSnackbar(`Successfully uploaded ${file.name}`, 'success');
        await fetchModels();
      } else {
        showSnackbar('Upload failed: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Upload error: ' + error.message, 'error');
    } finally {
      setLoading(false);
      // Reset file input
      event.target.value = '';
    }
  };

  const handleLoadModel = async (modelPath, slot) => {
    try {
      setLoading(true);
      const result = await loadModel(modelPath, slot);

      if (result.success) {
        showSnackbar(`Model loaded into slot ${slot}`, 'success');
        // Refresh status immediately
        await fetchStatus();
      } else {
        showSnackbar('Failed to load model: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Load error: ' + error.message, 'error');
    } finally {
      setLoading(false);
    }
  };

  const handleActivateSlot = async (slot) => {
    try {
      setLoading(true);
      const result = await setActiveSlot(slot);

      if (result.success) {
        showSnackbar(`Activated slot ${slot}`, 'success');
        // Refresh status immediately
        await fetchStatus();
      } else {
        showSnackbar('Failed to activate slot: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Activation error: ' + error.message, 'error');
    } finally {
      setLoading(false);
    }
  };

  const handleInputGainLChange = async (gain) => {
    try {
      const result = await setInputGainL(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set input gain L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Input gain L error: ' + error.message, 'error');
    }
  };

  const handleInputGainRChange = async (gain) => {
    try {
      const result = await setInputGainR(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set input gain R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Input gain R error: ' + error.message, 'error');
    }
  };

  const handleOutputGainLChange = async (gain) => {
    try {
      const result = await setOutputGainL(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set output gain L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Output gain L error: ' + error.message, 'error');
    }
  };

  const handleOutputGainRChange = async (gain) => {
    try {
      const result = await setOutputGainR(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set output gain R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Output gain R error: ' + error.message, 'error');
    }
  };

  const handleBypassModelToggle = async () => {
    try {
      const newBypassState = !status?.bypassModelL;
      const result = await setBypassModelL(newBypassState);
      if (result.success) {
        await fetchStatus();
        showSnackbar(`NAM ${newBypassState ? 'OFF' : 'ON'}`, 'success');
      } else {
        showSnackbar('Failed to toggle NAM: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('NAM toggle error: ' + error.message, 'error');
    }
  };

  const handleStereoModeChange = async (mode) => {
    try {
      const result = await setStereoMode(mode);
      if (result.success) {
        await fetchStatus();
        showSnackbar(`Stereo mode set to ${mode}`, 'success');
      } else {
        showSnackbar('Failed to set stereo mode: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Stereo mode error: ' + error.message, 'error');
    }
  };

  const handleStereo2MonoMixLChange = async (level) => {
    try {
      const result = await setStereo2MonoMixL(level);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set mix L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Mix L error: ' + error.message, 'error');
    }
  };

  const handleStereo2MonoMixRChange = async (level) => {
    try {
      const result = await setStereo2MonoMixR(level);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set mix R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Mix R error: ' + error.message, 'error');
    }
  };

  const handleEQEnabledLChange = async (enabled) => {
    try {
      const result = await setEQEnabledL(enabled);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ L enabled: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ L enabled error: ' + error.message, 'error');
    }
  };

  const handleEQEnabledRChange = async (enabled) => {
    try {
      const result = await setEQEnabledR(enabled);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ R enabled: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ R enabled error: ' + error.message, 'error');
    }
  };

  const handleEQBassLChange = async (gain) => {
    try {
      const result = await setEQBassL(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ bass L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ bass L error: ' + error.message, 'error');
    }
  };

  const handleEQMidLChange = async (gain) => {
    try {
      const result = await setEQMidL(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ mid L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ mid L error: ' + error.message, 'error');
    }
  };

  const handleEQTrebleLChange = async (gain) => {
    try {
      const result = await setEQTrebleL(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ treble L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ treble L error: ' + error.message, 'error');
    }
  };

  const handleEQBassRChange = async (gain) => {
    try {
      const result = await setEQBassR(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ bass R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ bass R error: ' + error.message, 'error');
    }
  };

  const handleEQMidRChange = async (gain) => {
    try {
      const result = await setEQMidR(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ mid R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ mid R error: ' + error.message, 'error');
    }
  };

  const handleEQTrebleRChange = async (gain) => {
    try {
      const result = await setEQTrebleR(gain);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set EQ treble R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('EQ treble R error: ' + error.message, 'error');
    }
  };

  const handleNoiseGateLChange = async (enabled, threshold) => {
    try {
      const result = await setNoiseGateL(enabled, threshold);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set noise gate L: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Noise gate L error: ' + error.message, 'error');
    }
  };

  const handleNoiseGateRChange = async (enabled, threshold) => {
    try {
      const result = await setNoiseGateR(enabled, threshold);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set noise gate R: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Noise gate R error: ' + error.message, 'error');
    }
  };

  const handleReverbEnabledChange = async (enabled) => {
    try {
      const result = await setReverbEnabled(enabled);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set reverb enabled: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Reverb enabled error: ' + error.message, 'error');
    }
  };

  const handleReverbRoomSizeChange = async (size) => {
    try {
      const result = await setReverbRoomSize(size);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set reverb room size: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Reverb room size error: ' + error.message, 'error');
    }
  };

  const handleReverbDecayTimeChange = async (seconds) => {
    try {
      const result = await setReverbDecayTime(seconds);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set reverb decay time: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Reverb decay time error: ' + error.message, 'error');
    }
  };

  const handleReverbDryChange = async (dry) => {
    try {
      const wet = status?.reverbWet ?? 0.3;
      const result = await setReverbMix(dry, wet);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set reverb dry: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Reverb dry error: ' + error.message, 'error');
    }
  };

  const handleReverbWetChange = async (wet) => {
    try {
      const dry = status?.reverbDry ?? 1.0;
      const result = await setReverbMix(dry, wet);
      if (result.success) {
        await fetchStatus();
      } else {
        showSnackbar('Failed to set reverb wet: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Reverb wet error: ' + error.message, 'error');
    }
  };

  const handleStartRecording = async (filename) => {
    try {
      const result = await startRecording(filename);
      if (result.success) {
        showSnackbar('Recording started', 'success');
        await fetchStatus();
      } else {
        showSnackbar('Failed to start recording: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Recording error: ' + error.message, 'error');
    }
  };

  const handleStopRecording = async () => {
    try {
      const result = await stopRecording();
      if (result.success) {
        showSnackbar('Recording stopped', 'success');
        await fetchStatus();
        await fetchRecordings(); // Refresh recordings list
      } else {
        showSnackbar('Failed to stop recording: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Recording error: ' + error.message, 'error');
    }
  };

  const handleDeleteRecording = async (filename) => {
    try {
      const result = await deleteRecording(filename);
      if (result.success) {
        showSnackbar('Recording deleted', 'success');
        await fetchRecordings();
      } else {
        showSnackbar('Failed to delete recording: ' + result.error, 'error');
      }
    } catch (error) {
      showSnackbar('Delete error: ' + error.message, 'error');
    }
  };

  const handleEditModel = (slot) => {
    setSelectedSlot(slot);
    setModelModalOpen(true);
  };

  return (
    <ThemeProvider theme={darkTheme}>
      <CssBaseline />
      <Box sx={{ flexGrow: 1, minHeight: '100vh', bgcolor: 'background.default', overflowX: 'hidden', width: '100vw', maxWidth: '100%' }}>
        <AppBar position="static" elevation={0}>
          <Toolbar>
            <Box
              sx={{
                width: 40,
                height: 40,
                mr: 2,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                bgcolor: 'white',
                borderRadius: 1,
                p: 0.5,
              }}
            >
              <Box
                component="img"
                src="/android-chrome-192x192.png"
                alt="HoopiPi"
                sx={{ width: '100%', height: '100%', objectFit: 'contain' }}
              />
            </Box>
            <Typography variant="h6" component="div" sx={{ flexGrow: 1, fontSize: { xs: '1rem', sm: '1.25rem' } }}>
              HoopiPi - Neural Amp Modeler
            </Typography>
          </Toolbar>
        </AppBar>

        <Container
          maxWidth={false}
          disableGutters
          sx={{
            mt: { xs: 1, sm: 4 },
            mb: { xs: 2, sm: 4 },
            px: { xs: 1, sm: 2, md: 3 },
            maxWidth: { xs: '100vw', sm: '100%', md: '1200px' },
            overflow: 'hidden'
          }}
        >
          <Stack spacing={{ xs: 2, sm: 3 }} sx={{ width: '100%', maxWidth: '100%', overflow: 'hidden' }}>
            {/* Engine Status & Stereo Mode */}
            <Paper sx={{ p: { xs: 1.5, sm: 3 }, overflow: 'hidden', width: '100%', boxSizing: 'border-box' }}>
              <Typography variant="h5" gutterBottom sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }}>
                Engine Status
              </Typography>
              <SlotStatus
                status={status}
                loading={!status && loading}
                error={statusError}
                onMetricsClick={() => setMetricsHistoryOpen(true)}
                onStereoModeChange={handleStereoModeChange}
                onJackClick={() => setSettingsModalOpen(true)}
              />
            </Paper>

            {/* Stereo2Mono Mix Controls - Only shown in Stereo2Mono mode */}
            {status?.stereoMode === 'Stereo2Mono' && (
              <Stereo2MonoMixControls
                status={status}
                onMixLChange={handleStereo2MonoMixLChange}
                onMixRChange={handleStereo2MonoMixRChange}
              />
            )}

            {/* Processing Chain: Input → Noise Gate → NAM → EQ → Output → Reverb */}
            <Typography variant="h5" gutterBottom sx={{ px: { xs: 0, sm: 0 }, fontSize: { xs: '1.25rem', sm: '1.5rem' } }}>
              Audio Processing Chain
            </Typography>

            {/* Mobile: Stack vertically, Desktop: Side by side grid */}
            <Stack spacing={2} sx={{ display: { xs: 'flex', md: 'none' } }}>
              <InputGainControl
                status={status}
                onInputGainLChange={handleInputGainLChange}
                onInputGainRChange={handleInputGainRChange}
              />
              <NoiseGateControls
                status={status}
                onNoiseGateLChange={handleNoiseGateLChange}
                onNoiseGateRChange={handleNoiseGateRChange}
              />
              <NAMModelControl
                status={status}
                onActivateSlot={handleActivateSlot}
                onBypassModelToggle={handleBypassModelToggle}
                onEditModel={handleEditModel}
              />
              <EQControls
                status={status}
                onEQEnabledLChange={handleEQEnabledLChange}
                onEQEnabledRChange={handleEQEnabledRChange}
                onEQBassLChange={handleEQBassLChange}
                onEQMidLChange={handleEQMidLChange}
                onEQTrebleLChange={handleEQTrebleLChange}
                onEQBassRChange={handleEQBassRChange}
                onEQMidRChange={handleEQMidRChange}
                onEQTrebleRChange={handleEQTrebleRChange}
              />
              <OutputGainControl
                status={status}
                onOutputGainLChange={handleOutputGainLChange}
                onOutputGainRChange={handleOutputGainRChange}
              />
              <ReverbControls
                status={status}
                onReverbEnabledChange={handleReverbEnabledChange}
                onReverbRoomSizeChange={handleReverbRoomSizeChange}
                onReverbDecayTimeChange={handleReverbDecayTimeChange}
                onReverbDryChange={handleReverbDryChange}
                onReverbWetChange={handleReverbWetChange}
              />
            </Stack>

            {/* Desktop: Grid layout */}
            <Box sx={{ display: { xs: 'none', md: 'block' } }}>
              <Grid container spacing={3} alignItems="stretch">
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <InputGainControl
                    status={status}
                    onInputGainLChange={handleInputGainLChange}
                    onInputGainRChange={handleInputGainRChange}
                  />
                </Grid>
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <NoiseGateControls
                    status={status}
                    onNoiseGateLChange={handleNoiseGateLChange}
                    onNoiseGateRChange={handleNoiseGateRChange}
                  />
                </Grid>
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <NAMModelControl
                    status={status}
                    onActivateSlot={handleActivateSlot}
                    onBypassModelToggle={handleBypassModelToggle}
                    onEditModel={handleEditModel}
                  />
                </Grid>
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <EQControls
                    status={status}
                    onEQEnabledLChange={handleEQEnabledLChange}
                    onEQEnabledRChange={handleEQEnabledRChange}
                    onEQBassLChange={handleEQBassLChange}
                    onEQMidLChange={handleEQMidLChange}
                    onEQTrebleLChange={handleEQTrebleLChange}
                    onEQBassRChange={handleEQBassRChange}
                    onEQMidRChange={handleEQMidRChange}
                    onEQTrebleRChange={handleEQTrebleRChange}
                  />
                </Grid>
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <OutputGainControl
                    status={status}
                    onOutputGainLChange={handleOutputGainLChange}
                    onOutputGainRChange={handleOutputGainRChange}
                  />
                </Grid>
                <Grid item md={2} sx={{ display: 'flex' }}>
                  <ReverbControls
                    status={status}
                    onReverbEnabledChange={handleReverbEnabledChange}
                    onReverbRoomSizeChange={handleReverbRoomSizeChange}
                    onReverbDecayTimeChange={handleReverbDecayTimeChange}
                    onReverbDryChange={handleReverbDryChange}
                    onReverbWetChange={handleReverbWetChange}
                  />
                </Grid>
              </Grid>
            </Box>

            {/* Recordings */}
            <Recordings
              status={status}
              onStartRecording={handleStartRecording}
              onStopRecording={handleStopRecording}
              recordings={recordings}
              onRefresh={fetchRecordings}
              onDelete={handleDeleteRecording}
            />

            {/* Backing Track */}
            <BackingTrackControl />
          </Stack>
        </Container>

        {/* Model Selection Modal */}
        <Dialog
          open={modelModalOpen}
          onClose={() => setModelModalOpen(false)}
          maxWidth="md"
          fullWidth
        >
          <DialogTitle>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <Typography variant="h5">
                Select Model for Slot {selectedSlot}
              </Typography>
              <IconButton onClick={() => setModelModalOpen(false)} size="small">
                <Close />
              </IconButton>
            </Box>
          </DialogTitle>
          <DialogContent>
            <ModelList
              models={models}
              onLoadModel={async (path, slot) => {
                await handleLoadModel(path, slot);
                setModelModalOpen(false);
              }}
              loading={loading}
              onRefresh={fetchModels}
              selectedSlot={selectedSlot}
            />
          </DialogContent>
          <DialogActions>
            <Button
              variant="contained"
              component="label"
              startIcon={<Upload />}
              disabled={loading}
            >
              Add Models
              <input
                type="file"
                hidden
                accept=".zip"
                onChange={handleUpload}
              />
            </Button>
          </DialogActions>
        </Dialog>

        {/* Settings Modal */}
        <Dialog
          open={settingsModalOpen}
          onClose={() => setSettingsModalOpen(false)}
          maxWidth="sm"
          fullWidth
        >
          <DialogTitle>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <Typography variant="h5">
                Settings
              </Typography>
              <IconButton onClick={() => setSettingsModalOpen(false)} size="small">
                <Close />
              </IconButton>
            </Box>
          </DialogTitle>
          <DialogContent>
            <AudioDeviceSettings />
          </DialogContent>
        </Dialog>

        {/* Metrics History Modal */}
        <MetricsHistory
          open={metricsHistoryOpen}
          onClose={() => setMetricsHistoryOpen(false)}
          history={metricsHistory}
          modelChanges={modelChanges}
          bypassChanges={bypassChanges}
          status={status}
        />

        <Snackbar
          open={snackbar.open}
          autoHideDuration={6000}
          onClose={handleCloseSnackbar}
          anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
        >
          <Alert onClose={handleCloseSnackbar} severity={snackbar.severity} sx={{ width: '100%' }}>
            {snackbar.message}
          </Alert>
        </Snackbar>
      </Box>
    </ThemeProvider>
  );
}

export default App;
