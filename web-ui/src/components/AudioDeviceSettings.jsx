import { Box, Paper, Typography, FormControl, InputLabel, Select, MenuItem, Chip, Stack, CircularProgress, Alert, TextField, Button, Divider, IconButton } from '@mui/material';
import { Headphones, Mic, Settings, Refresh, RestartAlt } from '@mui/icons-material';
import { useState, useEffect } from 'react';

function AudioDeviceSettings({ onDeviceChange }) {
  const [devices, setDevices] = useState([]);
  const [currentDevice, setCurrentDevice] = useState(null);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [loading, setLoading] = useState(true);
  const [applying, setApplying] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);
  const [bufferSize, setBufferSize] = useState(128);
  const [bufferSizeInput, setBufferSizeInput] = useState('128');
  const [jackLogs, setJackLogs] = useState('Loading logs...');
  const [engineLogs, setEngineLogs] = useState('Loading logs...');
  const [refreshingLogs, setRefreshingLogs] = useState(false);
  const [refreshingEngineLogs, setRefreshingEngineLogs] = useState(false);
  const [restartingJack, setRestartingJack] = useState(false);
  const [restartingEngine, setRestartingEngine] = useState(false);

  useEffect(() => {
    fetchDevices();
    fetchCurrentDevice();
    fetchBufferSize();
    fetchJackLogs();
    fetchEngineLogs();
  }, []);

  const fetchDevices = async () => {
    try {
      const response = await fetch('/api/audio/devices');
      const data = await response.json();
      if (data.success) {
        setDevices(data.devices || []);
      } else {
        setError('Failed to fetch audio devices');
      }
    } catch (err) {
      setError('Error loading devices: ' + err.message);
    } finally {
      setLoading(false);
    }
  };

  const fetchCurrentDevice = async () => {
    try {
      const response = await fetch('/api/audio/current');
      const data = await response.json();
      if (data.success) {
        // Use playback device as the primary device identifier
        setCurrentDevice(data.playbackDevice);
        setSelectedDevice(data.playbackDevice);
      }
    } catch (err) {
      console.error('Error fetching current device:', err);
    }
  };

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
  };

  const fetchBufferSize = async () => {
    try {
      const response = await fetch('/api/jack/buffersize');
      const data = await response.json();
      if (data.success && data.bufferSize) {
        setBufferSize(data.bufferSize);
        setBufferSizeInput(data.bufferSize.toString());
      }
    } catch (err) {
      console.error('Error fetching buffer size:', err);
    }
  };

  const fetchJackLogs = async () => {
    setRefreshingLogs(true);
    try {
      const response = await fetch('/api/jack/logs');
      const data = await response.json();
      if (data.success && data.logs) {
        setJackLogs(data.logs);
      } else {
        setJackLogs('Unable to fetch JACK logs');
      }
    } catch (err) {
      setJackLogs('Error loading logs: ' + err.message);
    } finally {
      setRefreshingLogs(false);
    }
  };

  const fetchEngineLogs = async () => {
    setRefreshingEngineLogs(true);
    try {
      const response = await fetch('/api/engine/logs');
      const data = await response.json();
      if (data.success && data.logs) {
        setEngineLogs(data.logs);
      } else {
        setEngineLogs('Unable to fetch engine logs');
      }
    } catch (err) {
      setEngineLogs('Error loading logs: ' + err.message);
    } finally {
      setRefreshingEngineLogs(false);
    }
  };

  const handleRefreshLogs = () => {
    fetchJackLogs();
  };

  const handleRefreshEngineLogs = () => {
    fetchEngineLogs();
  };

  const handleRestartJack = async () => {
    setRestartingJack(true);
    setError(null);
    setSuccess(null);

    try {
      const response = await fetch('/api/jack/restart', {
        method: 'POST',
      });

      const data = await response.json();
      if (data.success) {
        setSuccess('JACK service restarted successfully');
        // Wait a moment for service to restart, then refresh logs
        await new Promise(resolve => setTimeout(resolve, 2000));
        await fetchJackLogs();
        await fetchEngineLogs();
      } else {
        setError(data.error || 'Failed to restart JACK service');
      }
    } catch (err) {
      setError('Error restarting JACK: ' + err.message);
    } finally {
      setRestartingJack(false);
    }
  };

  const handleRestartEngine = async () => {
    setRestartingEngine(true);
    setError(null);
    setSuccess(null);

    try {
      const response = await fetch('/api/engine/restart', {
        method: 'POST',
      });

      const data = await response.json();
      if (data.success) {
        setSuccess('HoopiPi engine service restarted successfully');
        // Wait a moment for service to restart, then refresh logs
        await new Promise(resolve => setTimeout(resolve, 2000));
        await fetchEngineLogs();
      } else {
        setError(data.error || 'Failed to restart HoopiPi engine service');
      }
    } catch (err) {
      setError('Error restarting engine: ' + err.message);
    } finally {
      setRestartingEngine(false);
    }
  };

  const handleApply = async () => {
    const newSize = parseInt(bufferSizeInput);
    if (isNaN(newSize) || newSize < 16 || newSize > 2048) {
      setError('Buffer size must be between 16 and 2048');
      return;
    }

    setApplying(true);
    setError(null);
    setSuccess(null);

    const deviceChanged = selectedDevice !== currentDevice;
    const bufferChanged = newSize !== bufferSize;

    if (!deviceChanged && !bufferChanged) {
      setApplying(false);
      return;
    }

    try {
      // Apply device change if needed
      if (deviceChanged) {
        const response = await fetch('/api/audio/device', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ device: selectedDevice }),
        });

        const data = await response.json();
        if (!data.success) {
          setError(data.error || 'Failed to change audio device');
          setApplying(false);
          return;
        }
        setCurrentDevice(selectedDevice);
      }

      // Apply buffer size change if needed
      if (bufferChanged) {
        const response = await fetch('/api/jack/buffersize', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ bufferSize: newSize }),
        });

        const data = await response.json();
        if (!data.success) {
          setError(data.error || 'Failed to change buffer size');
          setApplying(false);
          return;
        }
        setBufferSize(newSize);
      }

      // Wait for JACK to restart
      await new Promise(resolve => setTimeout(resolve, 2000));

      // Refresh logs
      await fetchJackLogs();
      await fetchEngineLogs();

      // Show success message
      const changes = [];
      if (deviceChanged) changes.push('device');
      if (bufferChanged) changes.push('buffer size');
      setSuccess(`Successfully updated ${changes.join(' and ')}. JACK has been restarted.`);

      if (onDeviceChange && deviceChanged) {
        onDeviceChange(selectedDevice);
      }

    } catch (err) {
      setError('Error applying changes: ' + err.message);
    } finally {
      setApplying(false);
    }
  };

  const getDeviceCapabilities = (device) => {
    const caps = [];
    if (device.playback) caps.push('Playback');
    if (device.capture) caps.push('Capture');
    return caps.join(' + ');
  };

  if (loading) {
    return (
      <Paper sx={{ p: 2 }}>
        <Box display="flex" alignItems="center" gap={2}>
          <Settings />
          <Typography variant="h6">Audio Device</Typography>
        </Box>
        <Box display="flex" justifyContent="center" p={3}>
          <CircularProgress />
        </Box>
      </Paper>
    );
  }

  const hasChanges = selectedDevice !== currentDevice || bufferSizeInput !== bufferSize.toString();

  return (
    <Paper sx={{ p: 2 }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
        <Settings />
        <Typography variant="h6">JACK Configuration</Typography>
      </Box>

      {error && (
        <Alert severity="error" sx={{ mb: 2 }} onClose={() => setError(null)}>
          {error}
        </Alert>
      )}

      {success && (
        <Alert severity="success" sx={{ mb: 2 }} onClose={() => setSuccess(null)}>
          {success}
        </Alert>
      )}

      {applying && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Applying changes and restarting JACK...
        </Alert>
      )}

      {/* Audio Device */}
      <FormControl fullWidth disabled={applying} sx={{ mb: 3 }}>
        <InputLabel id="audio-device-label">Audio Device</InputLabel>
        <Select
          labelId="audio-device-label"
          id="audio-device-select"
          value={selectedDevice || ''}
          label="Audio Device"
          onChange={handleDeviceChange}
        >
          {devices.map((device) => (
            <MenuItem key={device.id} value={device.id}>
              <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, width: '100%' }}>
                <Typography sx={{ flex: 1 }}>{device.name}</Typography>
                <Stack direction="row" spacing={0.5}>
                  {device.playback && (
                    <Chip
                      icon={<Headphones fontSize="small" />}
                      label="Playback"
                      size="small"
                      variant="outlined"
                      sx={{ height: 20 }}
                    />
                  )}
                  {device.capture && (
                    <Chip
                      icon={<Mic fontSize="small" />}
                      label="Capture"
                      size="small"
                      variant="outlined"
                      sx={{ height: 20 }}
                    />
                  )}
                </Stack>
              </Box>
            </MenuItem>
          ))}
        </Select>
      </FormControl>

      {/* Buffer Size */}
      <Box sx={{ mb: 3 }}>
        <TextField
          label="Buffer Size (frames)"
          type="number"
          value={bufferSizeInput}
          onChange={(e) => setBufferSizeInput(e.target.value)}
          disabled={applying}
          size="small"
          fullWidth
          inputProps={{ min: 16, max: 2048, step: 16 }}
          helperText="Lower = lower latency, higher CPU. Higher = higher latency, lower CPU."
        />
      </Box>

      {/* Apply Button */}
      <Box sx={{ mb: 3 }}>
        <Button
          variant="contained"
          onClick={handleApply}
          disabled={applying || !hasChanges}
          fullWidth
        >
          {applying ? 'Applying...' : 'Apply Changes'}
        </Button>
        {currentDevice && (
          <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>
            Current: {currentDevice}, {bufferSize} frames
            <br />
            Warning: Applying changes will restart JACK.
          </Typography>
        )}
      </Box>

      <Divider sx={{ my: 3 }} />

      {/* JACK Logs */}
      <Box>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 1 }}>
          <Typography variant="h6">
            JACK Logs
          </Typography>
          <IconButton
            size="small"
            onClick={handleRefreshLogs}
            disabled={refreshingLogs}
            title="Refresh logs"
          >
            <Refresh />
          </IconButton>
        </Box>
        <Box
          sx={{
            fontFamily: 'monospace',
            fontSize: '0.75rem',
            bgcolor: 'background.default',
            p: 2,
            borderRadius: 1,
            maxHeight: '300px',
            overflow: 'auto',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
          }}
        >
          {refreshingLogs ? 'Refreshing logs...' : jackLogs}
        </Box>
        <Button
          variant="outlined"
          color="warning"
          startIcon={<RestartAlt />}
          onClick={handleRestartJack}
          disabled={restartingJack}
          sx={{ mt: 2 }}
          fullWidth
        >
          {restartingJack ? 'Restarting JACK...' : 'Restart JACK Service'}
        </Button>
      </Box>

      <Divider sx={{ my: 3 }} />

      {/* HoopiPi Engine Logs */}
      <Box>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 1 }}>
          <Typography variant="h6">
            HoopiPi Engine Logs
          </Typography>
          <IconButton
            size="small"
            onClick={handleRefreshEngineLogs}
            disabled={refreshingEngineLogs}
            title="Refresh logs"
          >
            <Refresh />
          </IconButton>
        </Box>
        <Box
          sx={{
            fontFamily: 'monospace',
            fontSize: '0.75rem',
            bgcolor: 'background.default',
            p: 2,
            borderRadius: 1,
            maxHeight: '300px',
            overflow: 'auto',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
          }}
        >
          {refreshingEngineLogs ? 'Refreshing logs...' : engineLogs}
        </Box>
        <Button
          variant="outlined"
          color="warning"
          startIcon={<RestartAlt />}
          onClick={handleRestartEngine}
          disabled={restartingEngine}
          sx={{ mt: 2 }}
          fullWidth
        >
          {restartingEngine ? 'Restarting Engine...' : 'Restart HoopiPi Engine Service'}
        </Button>
      </Box>
    </Paper>
  );
}

export default AudioDeviceSettings;
