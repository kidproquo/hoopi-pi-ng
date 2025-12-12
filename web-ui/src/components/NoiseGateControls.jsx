import { Box, Paper, Typography, Slider, Switch, FormControlLabel, Divider, Stack } from '@mui/material';
import { VolumeOff } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function NoiseGateControls({
  status,
  onNoiseGateLChange,
  onNoiseGateRChange
}) {
  const [enabledL, setEnabledL] = useState(false);
  const [thresholdL, setThresholdL] = useState(-40);
  const [enabledR, setEnabledR] = useState(false);
  const [thresholdR, setThresholdR] = useState(-40);
  const valuesLoaded = useRef(false);

  // Check if in True Stereo mode
  const isTrueStereo = status?.stereoMode === 'Stereo2Stereo';

  useEffect(() => {
    // Load values from status only once
    if (status && !valuesLoaded.current) {
      setEnabledL(status.noiseGateEnabledL || false);
      setThresholdL(status.noiseGateThresholdL || -40);
      setEnabledR(status.noiseGateEnabledR || false);
      setThresholdR(status.noiseGateThresholdR || -40);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleEnabledLChange = (event) => {
    const newValue = event.target.checked;
    setEnabledL(newValue);
    if (onNoiseGateLChange) {
      onNoiseGateLChange(newValue, thresholdL);
    }
  };

  const handleThresholdLChange = (event, newValue) => {
    setThresholdL(newValue);
    if (onNoiseGateLChange) {
      onNoiseGateLChange(enabledL, newValue);
    }
  };

  const handleEnabledRChange = (event) => {
    const newValue = event.target.checked;
    setEnabledR(newValue);
    if (onNoiseGateRChange) {
      onNoiseGateRChange(newValue, thresholdR);
    }
  };

  const handleThresholdRChange = (event, newValue) => {
    setThresholdR(newValue);
    if (onNoiseGateRChange) {
      onNoiseGateRChange(enabledR, newValue);
    }
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 3 }, flex: 1, display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: { xs: 2, sm: 3 } }}>
        <VolumeOff sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }} />
        <Typography variant="h5" sx={{ fontSize: { xs: '1rem', sm: '1.5rem' } }}>
          Noise Gate
        </Typography>
      </Box>

      {isTrueStereo ? (
        <Stack direction="row" spacing={2} sx={{ flex: 1 }}>
          {/* Left Channel */}
          <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
            <Typography variant="caption" sx={{ mb: 1, color: 'success.main', fontWeight: 'bold' }}>
              L (Guitar)
            </Typography>

            <FormControlLabel
              control={
                <Switch
                  checked={enabledL}
                  onChange={handleEnabledLChange}
                  color="primary"
                  size="small"
                />
              }
              label=""
              sx={{ mb: 1 }}
            />

            <Box sx={{ textAlign: 'center', opacity: enabledL ? 1 : 0.5, display: 'flex', flexDirection: 'column' }}>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontSize: '0.65rem' }}>
                Threshold
              </Typography>
              <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1 }}>
                <Slider
                  orientation="vertical"
                  value={thresholdL}
                  onChange={handleThresholdLChange}
                  disabled={!enabledL}
                  min={-60}
                  max={0}
                  step={1}
                  marks={[
                    { value: -60, label: '-60' },
                    { value: 0, label: '0' }
                  ]}
                  valueLabelDisplay="auto"
                  valueLabelFormat={(value) => `${value.toFixed(0)} dB`}
                  sx={{ height: '100%' }}
                />
              </Box>
              <Typography variant="caption" sx={{ fontWeight: 'bold', fontSize: '0.7rem' }}>
                {thresholdL.toFixed(0)} dB
              </Typography>
            </Box>
          </Box>

          {/* Right Channel */}
          <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
            <Typography variant="caption" sx={{ mb: 1, color: 'info.main', fontWeight: 'bold' }}>
              R (Mic)
            </Typography>

            <FormControlLabel
              control={
                <Switch
                  checked={enabledR}
                  onChange={handleEnabledRChange}
                  color="primary"
                  size="small"
                />
              }
              label=""
              sx={{ mb: 1 }}
            />

            <Box sx={{ textAlign: 'center', opacity: enabledR ? 1 : 0.5, flex: 1, display: 'flex', flexDirection: 'column' }}>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontSize: '0.65rem' }}>
                Threshold
              </Typography>
              <Box sx={{ flex: 1, display: 'flex', justifyContent: 'center', my: 1, minHeight: 120 }}>
                <Slider
                  orientation="vertical"
                  value={thresholdR}
                  onChange={handleThresholdRChange}
                  disabled={!enabledR}
                  min={-60}
                  max={0}
                  step={1}
                  marks={[
                    { value: -60, label: '-60' },
                    { value: 0, label: '0' }
                  ]}
                  valueLabelDisplay="auto"
                  valueLabelFormat={(value) => `${value.toFixed(0)} dB`}
                  sx={{ height: '100%' }}
                />
              </Box>
              <Typography variant="caption" sx={{ fontWeight: 'bold', fontSize: '0.7rem' }}>
                {thresholdR.toFixed(0)} dB
              </Typography>
            </Box>
          </Box>
        </Stack>
      ) : (
        <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
          <FormControlLabel
            control={
              <Switch
                checked={enabledL}
                onChange={handleEnabledLChange}
                color="primary"
              />
            }
            label={enabledL ? "Enabled" : "Disabled"}
            sx={{ mb: 2 }}
          />

          <Box sx={{ textAlign: 'center', opacity: enabledL ? 1 : 0.5, display: 'flex', flexDirection: 'column', width: '100%' }}>
            <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold' }}>
              Threshold
            </Typography>
            <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 2 }}>
              <Slider
                orientation="vertical"
                value={thresholdL}
                onChange={handleThresholdLChange}
                disabled={!enabledL}
                min={-60}
                max={0}
                step={1}
                marks={[
                  { value: -60, label: '-60' },
                  { value: -40, label: '-40' },
                  { value: -20, label: '-20' },
                  { value: 0, label: '0' }
                ]}
                valueLabelDisplay="auto"
                valueLabelFormat={(value) => `${value.toFixed(0)} dB`}
                sx={{ height: '100%' }}
              />
            </Box>
            <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
              {thresholdL.toFixed(0)} dB
            </Typography>
          </Box>
        </Box>
      )}

      <Typography variant="caption" color="text.secondary" display="block" sx={{ mt: 2, textAlign: 'center' }}>
        Silence audio below threshold
      </Typography>
    </Paper>
  );
}

export default NoiseGateControls;
