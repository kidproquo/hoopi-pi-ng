import { Box, Paper, Typography, Slider, Stack, Switch, FormControlLabel, Divider } from '@mui/material';
import { Equalizer } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function EQControls({
  status,
  onEQEnabledLChange,
  onEQEnabledRChange,
  onEQBassLChange,
  onEQMidLChange,
  onEQTrebleLChange,
  onEQBassRChange,
  onEQMidRChange,
  onEQTrebleRChange
}) {
  const [eqEnabledL, setEQEnabledL] = useState(false);
  const [eqEnabledR, setEQEnabledR] = useState(false);
  const [bassL, setBassL] = useState(0);
  const [midL, setMidL] = useState(0);
  const [trebleL, setTrebleL] = useState(0);
  const [bassR, setBassR] = useState(0);
  const [midR, setMidR] = useState(0);
  const [trebleR, setTrebleR] = useState(0);
  const valuesLoaded = useRef(false);

  // Check if in True Stereo mode
  const isTrueStereo = status?.stereoMode === 'Stereo2Stereo';

  // Load values from status only once
  useEffect(() => {
    if (status && !valuesLoaded.current) {
      setEQEnabledL(status.eqEnabledL || false);
      setEQEnabledR(status.eqEnabledR || false);
      setBassL(status.eqBassL || 0);
      setMidL(status.eqMidL || 0);
      setTrebleL(status.eqTrebleL || 0);
      setBassR(status.eqBassR || 0);
      setMidR(status.eqMidR || 0);
      setTrebleR(status.eqTrebleR || 0);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleEnabledLChange = (event) => {
    const newValue = event.target.checked;
    setEQEnabledL(newValue);
    if (onEQEnabledLChange) {
      onEQEnabledLChange(newValue);
    }
  };

  const handleEnabledRChange = (event) => {
    const newValue = event.target.checked;
    setEQEnabledR(newValue);
    if (onEQEnabledRChange) {
      onEQEnabledRChange(newValue);
    }
  };

  const handleBassLChange = (event, newValue) => {
    setBassL(newValue);
    if (onEQBassLChange) {
      onEQBassLChange(newValue);
    }
  };

  const handleMidLChange = (event, newValue) => {
    setMidL(newValue);
    if (onEQMidLChange) {
      onEQMidLChange(newValue);
    }
  };

  const handleTrebleLChange = (event, newValue) => {
    setTrebleL(newValue);
    if (onEQTrebleLChange) {
      onEQTrebleLChange(newValue);
    }
  };

  const handleBassRChange = (event, newValue) => {
    setBassR(newValue);
    if (onEQBassRChange) {
      onEQBassRChange(newValue);
    }
  };

  const handleMidRChange = (event, newValue) => {
    setMidR(newValue);
    if (onEQMidRChange) {
      onEQMidRChange(newValue);
    }
  };

  const handleTrebleRChange = (event, newValue) => {
    setTrebleR(newValue);
    if (onEQTrebleRChange) {
      onEQTrebleRChange(newValue);
    }
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 3 }, flex: 1, display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: { xs: 2, sm: 3 } }}>
        <Equalizer sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }} />
        <Typography variant="h5" sx={{ fontSize: { xs: '1rem', sm: '1.5rem' } }}>
          3-Band EQ
        </Typography>
      </Box>

      {isTrueStereo ? (
        <Stack direction="row" spacing={2} sx={{ minHeight: { xs: 180, sm: 220 } }}>
          {/* Left Channel EQ */}
          <Box sx={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 1 }}>
              <Typography variant="caption" sx={{ color: 'success.main', fontWeight: 'bold' }}>
                L (Guitar)
              </Typography>
              <Switch
                checked={eqEnabledL}
                onChange={handleEnabledLChange}
                color="primary"
                size="small"
              />
            </Box>
            <Stack
              direction="row"
              spacing={1}
              justifyContent="space-around"
              sx={{ opacity: eqEnabledL ? 1 : 0.5, flex: 1 }}
            >
              {/* Bass L */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Bass
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={bassL}
                    onChange={handleBassLChange}
                    disabled={!eqEnabledL}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {bassL.toFixed(1)}
                </Typography>
              </Box>

              {/* Mid L */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Mid
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={midL}
                    onChange={handleMidLChange}
                    disabled={!eqEnabledL}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {midL.toFixed(1)}
                </Typography>
              </Box>

              {/* Treble L */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Treble
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={trebleL}
                    onChange={handleTrebleLChange}
                    disabled={!eqEnabledL}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {trebleL.toFixed(1)}
                </Typography>
              </Box>
            </Stack>
          </Box>

          <Divider orientation="vertical" flexItem />

          {/* Right Channel EQ */}
          <Box sx={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 1 }}>
              <Typography variant="caption" sx={{ color: 'info.main', fontWeight: 'bold' }}>
                R (Mic)
              </Typography>
              <Switch
                checked={eqEnabledR}
                onChange={handleEnabledRChange}
                color="primary"
                size="small"
              />
            </Box>
            <Stack
              direction="row"
              spacing={1}
              justifyContent="space-around"
              sx={{ opacity: eqEnabledR ? 1 : 0.5, flex: 1 }}
            >
              {/* Bass R */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Bass
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={bassR}
                    onChange={handleBassRChange}
                    disabled={!eqEnabledR}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {bassR.toFixed(1)}
                </Typography>
              </Box>

              {/* Mid R */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Mid
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={midR}
                    onChange={handleMidRChange}
                    disabled={!eqEnabledR}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {midR.toFixed(1)}
                </Typography>
              </Box>

              {/* Treble R */}
              <Box sx={{ textAlign: 'center', display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
                <Typography variant="caption" display="block" sx={{ fontSize: '0.65rem', fontWeight: 'bold' }}>
                  Treble
                </Typography>
                <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 1, width: '100%' }}>
                  <Slider
                    orientation="vertical"
                    value={trebleR}
                    onChange={handleTrebleRChange}
                    disabled={!eqEnabledR}
                    min={-20}
                    max={20}
                    step={0.5}
                    marks={[
                      { value: -20, label: '-20' },
                      { value: 0, label: '0' },
                      { value: 20, label: '20' }
                    ]}
                    valueLabelDisplay="auto"
                    valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                    sx={{ height: '100%' }}
                  />
                </Box>
                <Typography variant="caption" sx={{ fontSize: '0.65rem' }}>
                  {trebleR.toFixed(1)}
                </Typography>
              </Box>
            </Stack>
          </Box>
        </Stack>
      ) : (
        <Box>
          <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2 }}>
            <FormControlLabel
              control={
                <Switch
                  checked={eqEnabledL}
                  onChange={handleEnabledLChange}
                  color="primary"
                />
              }
              label={eqEnabledL ? "Enabled" : "Disabled"}
            />
          </Box>
          <Stack
            direction="row"
            spacing={3}
            justifyContent="center"
            alignItems="flex-start"
            sx={{ opacity: eqEnabledL ? 1 : 0.5 }}
          >
            {/* Bass L */}
            <Box sx={{ textAlign: 'center' }}>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold' }}>
                Bass
              </Typography>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontSize: '0.65rem', color: 'text.secondary' }}>
                120 Hz
              </Typography>
              <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                <Slider
                  orientation="vertical"
                  value={bassL}
                  onChange={handleBassLChange}
                  disabled={!eqEnabledL}
                  min={-20}
                  max={20}
                  step={0.5}
                  marks={[
                    { value: -20, label: '-20' },
                    { value: 0, label: '0' },
                    { value: 20, label: '20' }
                  ]}
                  valueLabelDisplay="auto"
                  valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                  sx={{ height: '100%' }}
                />
              </Box>
              <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                {bassL.toFixed(1)} dB
              </Typography>
            </Box>

            {/* Mid L */}
            <Box sx={{ textAlign: 'center' }}>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold' }}>
                Mid
              </Typography>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontSize: '0.65rem', color: 'text.secondary' }}>
                750 Hz
              </Typography>
              <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                <Slider
                  orientation="vertical"
                  value={midL}
                  onChange={handleMidLChange}
                  disabled={!eqEnabledL}
                  min={-20}
                  max={20}
                  step={0.5}
                  marks={[
                    { value: -20, label: '-20' },
                    { value: 0, label: '0' },
                    { value: 20, label: '20' }
                  ]}
                  valueLabelDisplay="auto"
                  valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                  sx={{ height: '100%' }}
                />
              </Box>
              <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                {midL.toFixed(1)} dB
              </Typography>
            </Box>

            {/* Treble L */}
            <Box sx={{ textAlign: 'center' }}>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold' }}>
                Treble
              </Typography>
              <Typography variant="caption" display="block" gutterBottom sx={{ fontSize: '0.65rem', color: 'text.secondary' }}>
                3 kHz
              </Typography>
              <Box sx={{ height: { xs: 120, sm: 150 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                <Slider
                  orientation="vertical"
                  value={trebleL}
                  onChange={handleTrebleLChange}
                  disabled={!eqEnabledL}
                  min={-20}
                  max={20}
                  step={0.5}
                  marks={[
                    { value: -20, label: '-20' },
                    { value: 0, label: '0' },
                    { value: 20, label: '20' }
                  ]}
                  valueLabelDisplay="auto"
                  valueLabelFormat={(value) => `${value.toFixed(1)} dB`}
                  sx={{ height: '100%' }}
                />
              </Box>
              <Typography variant="body2" sx={{ fontWeight: 'bold' }}>
                {trebleL.toFixed(1)} dB
              </Typography>
            </Box>
          </Stack>
        </Box>
      )}
    </Paper>
  );
}

export default EQControls;
