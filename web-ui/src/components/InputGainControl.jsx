import { Paper, Typography, Slider, Stack, Box } from '@mui/material';
import { VolumeUp } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function InputGainControl({ status, onInputGainLChange, onInputGainRChange }) {
  const [inputGainL, setInputGainL] = useState(0);
  const [inputGainR, setInputGainR] = useState(0);
  const valuesLoaded = useRef(false);

  const isTrueStereo = status?.stereoMode === 'Stereo2Stereo';

  useEffect(() => {
    // Load values from status only once
    if (status && !valuesLoaded.current) {
      setInputGainL(status.inputGainL || 0);
      setInputGainR(status.inputGainR || 0);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleInputLChange = (event, newValue) => {
    setInputGainL(newValue);
    if (onInputGainLChange) {
      onInputGainLChange(newValue);
    }
  };

  const handleInputRChange = (event, newValue) => {
    setInputGainR(newValue);
    if (onInputGainRChange) {
      onInputGainRChange(newValue);
    }
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 2 }, height: '100%', display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: { xs: 1.5, sm: 2 } }}>
        <VolumeUp sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }} />
        <Typography variant="h6" sx={{ fontSize: { xs: '0.95rem', sm: '1.25rem' } }}>Input Gain</Typography>
      </Box>

      {isTrueStereo ? (
        <Stack direction="row" spacing={2} sx={{ flex: 1, justifyContent: 'space-around' }}>
          <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
            <Typography variant="caption" gutterBottom>
              L: {inputGainL?.toFixed(1)} dB
            </Typography>
            <Slider
              value={inputGainL}
              onChange={handleInputLChange}
              min={-40}
              max={40}
              step={0.1}
              orientation="vertical"
              sx={{ flex: 1, minHeight: 120 }}
              marks={[
                { value: -40, label: '-40' },
                { value: 0, label: '0' },
                { value: 40, label: '+40' }
              ]}
            />
          </Box>
          <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
            <Typography variant="caption" gutterBottom>
              R: {inputGainR?.toFixed(1)} dB
            </Typography>
            <Slider
              value={inputGainR}
              onChange={handleInputRChange}
              min={-40}
              max={40}
              step={0.1}
              orientation="vertical"
              sx={{ flex: 1, minHeight: 120 }}
              marks={[
                { value: -40, label: '-40' },
                { value: 0, label: '0' },
                { value: 40, label: '+40' }
              ]}
            />
          </Box>
        </Stack>
      ) : (
        <Box sx={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
          <Typography variant="caption" gutterBottom>
            {inputGainL?.toFixed(1)} dB
          </Typography>
          <Slider
            value={inputGainL}
            onChange={handleInputLChange}
            min={-40}
            max={40}
            step={0.1}
            orientation="vertical"
            sx={{ flex: 1, minHeight: 120 }}
            marks={[
              { value: -40, label: '-40' },
              { value: 0, label: '0' },
              { value: 40, label: '+40' }
            ]}
          />
        </Box>
      )}
    </Paper>
  );
}

export default InputGainControl;
