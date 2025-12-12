import { Box, Paper, Typography, Slider, Stack } from '@mui/material';
import { Tune } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function Stereo2MonoMixControls({ status, onMixLChange, onMixRChange }) {
  const [mixL, setMixL] = useState(0.5);
  const [mixR, setMixR] = useState(0.5);
  const valuesLoaded = useRef(false);

  useEffect(() => {
    // Load values from status only once
    if (status && !valuesLoaded.current) {
      setMixL(status.stereo2MonoMixL !== undefined ? status.stereo2MonoMixL : 0.5);
      setMixR(status.stereo2MonoMixR !== undefined ? status.stereo2MonoMixR : 0.5);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleMixLChange = (event, newValue) => {
    setMixL(newValue);
    if (onMixLChange) {
      onMixLChange(newValue);
    }
  };

  const handleMixRChange = (event, newValue) => {
    setMixR(newValue);
    if (onMixRChange) {
      onMixRChange(newValue);
    }
  };

  const formatPercent = (value) => {
    return `${(value * 100).toFixed(0)}%`;
  };

  return (
    <Paper sx={{ p: { xs: 2, sm: 3 }, flex: 1, display: 'flex', flexDirection: 'column', width: '100%' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 3 }}>
        <Tune />
        <Typography variant="h5">
          Stereo Mix Levels
        </Typography>
      </Box>

      <Stack
        direction={{ xs: 'row', sm: 'row' }}
        spacing={4}
        justifyContent="center"
        alignItems="flex-start"
      >
        {/* Left Mix Level */}
        <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
          <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: 'success.main' }}>
            Left Input
          </Typography>
          <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
            <Slider
              orientation="vertical"
              value={mixL}
              onChange={handleMixLChange}
              min={0}
              max={1}
              step={0.05}
              marks={[
                { value: 0, label: '0%' },
                { value: 0.5, label: '50%' },
                { value: 1, label: '100%' }
              ]}
              valueLabelDisplay="auto"
              valueLabelFormat={formatPercent}
              sx={{ height: '100%' }}
            />
          </Box>
          <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
            {formatPercent(mixL)}
          </Typography>
        </Box>

        {/* Right Mix Level */}
        <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
          <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: 'info.main' }}>
            Right Input
          </Typography>
          <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
            <Slider
              orientation="vertical"
              value={mixR}
              onChange={handleMixRChange}
              min={0}
              max={1}
              step={0.05}
              marks={[
                { value: 0, label: '0%' },
                { value: 0.5, label: '50%' },
                { value: 1, label: '100%' }
              ]}
              valueLabelDisplay="auto"
              valueLabelFormat={formatPercent}
              sx={{ height: '100%' }}
            />
          </Box>
          <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
            {formatPercent(mixR)}
          </Typography>
        </Box>
      </Stack>

      <Typography variant="caption" color="text.secondary" display="block" sx={{ mt: 2, textAlign: 'center' }}>
        Mix left and right inputs before processing (Default: 50% each)
      </Typography>
    </Paper>
  );
}

export default Stereo2MonoMixControls;
