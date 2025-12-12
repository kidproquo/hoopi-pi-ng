import { Paper, Typography, Slider, Stack, Box } from '@mui/material';
import { VolumeDown } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function OutputGainControl({ status, onOutputGainLChange, onOutputGainRChange }) {
  const [outputGainL, setOutputGainL] = useState(0);
  const [outputGainR, setOutputGainR] = useState(0);
  const valuesLoaded = useRef(false);

  const isTrueStereo = status?.stereoMode === 'Stereo2Stereo';

  useEffect(() => {
    // Load values from status only once
    if (status && !valuesLoaded.current) {
      setOutputGainL(status.outputGainL || 0);
      setOutputGainR(status.outputGainR || 0);
      valuesLoaded.current = true;
    }
  }, [status]);

  const handleOutputLChange = (event, newValue) => {
    setOutputGainL(newValue);
    if (onOutputGainLChange) {
      onOutputGainLChange(newValue);
    }
  };

  const handleOutputRChange = (event, newValue) => {
    setOutputGainR(newValue);
    if (onOutputGainRChange) {
      onOutputGainRChange(newValue);
    }
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 2 }, height: '100%', display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: { xs: 1.5, sm: 2 } }}>
        <VolumeDown sx={{ fontSize: { xs: '1.25rem', sm: '1.5rem' } }} />
        <Typography variant="h6" sx={{ fontSize: { xs: '0.95rem', sm: '1.25rem' } }}>Output Gain</Typography>
      </Box>

      {isTrueStereo ? (
        <Stack direction="row" spacing={2} sx={{ flex: 1, justifyContent: 'space-around' }}>
          <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>
            <Typography variant="caption" gutterBottom>
              L: {outputGainL?.toFixed(1)} dB
            </Typography>
            <Slider
              value={outputGainL}
              onChange={handleOutputLChange}
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
              R: {outputGainR?.toFixed(1)} dB
            </Typography>
            <Slider
              value={outputGainR}
              onChange={handleOutputRChange}
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
            {outputGainL?.toFixed(1)} dB
          </Typography>
          <Slider
            value={outputGainL}
            onChange={handleOutputLChange}
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

export default OutputGainControl;
