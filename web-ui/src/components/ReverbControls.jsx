import { Box, Paper, Typography, Slider, Stack, Switch, FormControlLabel, Divider } from '@mui/material';
import { WaterDrop } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function ReverbControls({
  status,
  onReverbEnabledChange,
  onReverbRoomSizeChange,
  onReverbDecayTimeChange,
  onReverbDryChange,
  onReverbWetChange
}) {
  const [enabled, setEnabled] = useState(false);
  const [roomSize, setRoomSize] = useState(0.3);
  const [decayTime, setDecayTime] = useState(2.0);
  const [dry, setDry] = useState(1.0);
  const [wet, setWet] = useState(0.3);
  const valuesLoaded = useRef(false);

  useEffect(() => {
    // Load values from status only once
    if (status && !valuesLoaded.current) {
      setEnabled(status.reverbEnabled || false);
      setRoomSize(status.reverbRoomSize ?? 0.3);
      setDecayTime(status.reverbDecayTime ?? 2.0);
      setDry(status.reverbDry ?? 1.0);
      setWet(status.reverbWet ?? 0.3);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleEnabledChange = (event) => {
    const newValue = event.target.checked;
    setEnabled(newValue);
    if (onReverbEnabledChange) {
      onReverbEnabledChange(newValue);
    }
  };

  const handleRoomSizeChange = (event, newValue) => {
    setRoomSize(newValue);
    if (onReverbRoomSizeChange) {
      onReverbRoomSizeChange(newValue);
    }
  };

  const handleDecayTimeChange = (event, newValue) => {
    setDecayTime(newValue);
    if (onReverbDecayTimeChange) {
      onReverbDecayTimeChange(newValue);
    }
  };

  const handleDryChange = (event, newValue) => {
    setDry(newValue);
    if (onReverbDryChange) {
      onReverbDryChange(newValue);
    }
  };

  const handleWetChange = (event, newValue) => {
    setWet(newValue);
    if (onReverbWetChange) {
      onReverbWetChange(newValue);
    }
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 2 }, width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: { xs: 1.5, sm: 2 } }}>
        <WaterDrop sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }} />
        <Typography variant="h6" sx={{ fontSize: { xs: '0.95rem', sm: '1.25rem' } }}>Reverb</Typography>
      </Box>

      <FormControlLabel
        control={
          <Switch
            checked={enabled}
            onChange={handleEnabledChange}
            color="primary"
          />
        }
        label="Enable"
        sx={{ mb: 2 }}
      />

      <Divider sx={{ my: 2 }} />

      <Stack spacing={3}>
        <Box>
          <Typography gutterBottom>
            Room Size: {(roomSize * 100).toFixed(0)}%
          </Typography>
          <Slider
            value={roomSize}
            onChange={handleRoomSizeChange}
            min={0.0}
            max={1.0}
            step={0.01}
            disabled={!enabled}
            marks={[
              { value: 0.0, label: 'Small' },
              { value: 0.5, label: 'Med' },
              { value: 1.0, label: 'Large' }
            ]}
          />
        </Box>

        <Box>
          <Typography gutterBottom>
            Decay Time: {decayTime.toFixed(1)}s
          </Typography>
          <Slider
            value={decayTime}
            onChange={handleDecayTimeChange}
            min={0.1}
            max={10.0}
            step={0.1}
            disabled={!enabled}
            marks={[
              { value: 0.1, label: '0.1s' },
              { value: 2.0, label: '2s' },
              { value: 5.0, label: '5s' },
              { value: 10.0, label: '10s' }
            ]}
          />
        </Box>

        <Box>
          <Typography gutterBottom>
            Dry: {(dry * 100).toFixed(0)}%
          </Typography>
          <Slider
            value={dry}
            onChange={handleDryChange}
            min={0.0}
            max={1.0}
            step={0.01}
            disabled={!enabled}
          />
        </Box>

        <Box>
          <Typography gutterBottom>
            Wet: {(wet * 100).toFixed(0)}%
          </Typography>
          <Slider
            value={wet}
            onChange={handleWetChange}
            min={0.0}
            max={1.0}
            step={0.01}
            disabled={!enabled}
          />
        </Box>
      </Stack>
    </Paper>
  );
}

export default ReverbControls;
