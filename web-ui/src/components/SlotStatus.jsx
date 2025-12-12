import { Box, Typography, Chip, Stack, CircularProgress, Tooltip } from '@mui/material';
import { Speed, Warning, Thermostat, Memory, Cable, ErrorOutline, CheckCircle } from '@mui/icons-material';
import StereoModeSelector from './StereoModeSelector';

function SlotStatus({ status, loading, error, onMetricsClick, onStereoModeChange, onJackClick }) {
  if (loading && !status) {
    return (
      <Box display="flex" justifyContent="center" p={3}>
        <CircularProgress />
      </Box>
    );
  }

  if (error || (!status && !loading)) {
    return (
      <Box display="flex" justifyContent="center" p={3}>
        <Typography color="error">
          Engine not connected - {error || 'No response'}
        </Typography>
      </Box>
    );
  }

  if (!status) {
    return null;
  }

  // Determine JACK status display
  const getJackStatusProps = () => {
    const jackStatus = status.jackStatus || 'unknown';
    const jackError = status.jackError || '';

    switch (jackStatus) {
      case 'connected':
        return {
          label: 'JACK: Connected',
          color: 'success',
          icon: <CheckCircle />,
          tooltip: `Sample Rate: ${status.sampleRate || 0} Hz, Buffer: ${status.bufferSize || 0} frames, Latency: ${status.latencyMs?.toFixed(2) || 0} ms`,
        };
      case 'connecting':
        return {
          label: 'JACK: Connecting...',
          color: 'warning',
          icon: <Cable />,
          tooltip: 'Attempting to connect to JACK server',
        };
      case 'disconnected':
        return {
          label: 'JACK: Disconnected',
          color: 'error',
          icon: <ErrorOutline />,
          tooltip: 'Not connected to JACK server. Will retry automatically.',
        };
      case 'error':
        return {
          label: 'JACK: Error',
          color: 'error',
          icon: <ErrorOutline />,
          tooltip: jackError || 'JACK connection error',
        };
      default:
        return {
          label: 'JACK: Unknown',
          color: 'default',
          icon: <Cable />,
          tooltip: 'JACK status unknown',
        };
    }
  };

  const jackStatusProps = getJackStatusProps();

  return (
    <Box>
      <Stack direction="row" spacing={1} mb={2} flexWrap="wrap" useFlexGap>
        {/* JACK Connection Status */}
        <Tooltip title={jackStatusProps.tooltip} arrow>
          <Chip
            icon={jackStatusProps.icon}
            label={jackStatusProps.label}
            color={jackStatusProps.color}
            size="small"
            onClick={onJackClick}
            sx={{ cursor: onJackClick ? 'pointer' : 'default' }}
          />
        </Tooltip>

        <Chip
          label={status.bypass ? 'BYPASS ON' : 'PROCESSING'}
          color={status.bypass ? 'warning' : 'success'}
          size="small"
        />
        {status.processCpu !== undefined && (
          <Chip
            icon={<Speed />}
            label={`CPU: ${status.processCpu?.toFixed(1) || 0}%`}
            variant="outlined"
            size="small"
            onClick={onMetricsClick}
            sx={{ cursor: onMetricsClick ? 'pointer' : 'default' }}
            title="HoopiPi process CPU usage"
          />
        )}
        {status.cpuTemp !== undefined && status.cpuTemp >= 0 && (
          <Chip
            icon={<Thermostat />}
            label={`${status.cpuTemp?.toFixed(1) || 0}°C`}
            variant="outlined"
            size="small"
            color={status.cpuTemp > 70 ? 'error' : status.cpuTemp > 60 ? 'warning' : 'default'}
            onClick={onMetricsClick}
            sx={{ cursor: onMetricsClick ? 'pointer' : 'default' }}
          />
        )}
        {status.memoryUsage !== undefined && status.memoryUsage >= 0 && (
          <Chip
            icon={<Memory />}
            label={`RAM: ${status.memoryUsage?.toFixed(0) || 0} MB`}
            variant="outlined"
            size="small"
            onClick={onMetricsClick}
            sx={{ cursor: onMetricsClick ? 'pointer' : 'default' }}
          />
        )}
        {status.xruns !== undefined && (
          <Chip
            icon={<Warning />}
            label={`Xruns: ${status.xruns || 0}`}
            color={status.xruns > 0 ? 'error' : 'default'}
            variant="outlined"
            size="small"
            onClick={onMetricsClick}
            sx={{ cursor: onMetricsClick ? 'pointer' : 'default' }}
          />
        )}
      </Stack>

      {/* Stereo Mode Selector */}
      <StereoModeSelector
        stereoMode={status.stereoMode}
        onChange={onStereoModeChange}
      />
    </Box>
  );
}

export default SlotStatus;
