import { Box, Dialog, DialogTitle, DialogContent, Typography, Slider, Stack, IconButton, Divider } from '@mui/material';
import { VolumeUp, VolumeDown, Close } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function GainControls({ open, onClose, status, onInputGainLChange, onInputGainRChange, onOutputGainLChange, onOutputGainRChange }) {
  const [inputGainL, setInputGainL] = useState(0);
  const [inputGainR, setInputGainR] = useState(0);
  const [outputGainL, setOutputGainL] = useState(0);
  const [outputGainR, setOutputGainR] = useState(0);
  const valuesLoaded = useRef(false);

  // Check if in True Stereo mode
  const isTrueStereo = status?.stereoMode === 'Stereo2Stereo';

  // Load values from status only once
  useEffect(() => {
    if (status && !valuesLoaded.current) {
      setInputGainL(status.inputGainL || 0);
      setInputGainR(status.inputGainR || 0);
      setOutputGainL(status.outputGainL || 0);
      setOutputGainR(status.outputGainR || 0);
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
    <Dialog open={open} onClose={onClose} maxWidth={isTrueStereo ? "md" : "sm"} fullWidth>
      <DialogTitle>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Typography variant="h5">
            {isTrueStereo ? 'Gain Controls (Per-Channel)' : 'Gain Controls'}
          </Typography>
          <IconButton onClick={onClose} size="small">
            <Close />
          </IconButton>
        </Box>
      </DialogTitle>
      <DialogContent>
        <Stack spacing={4} sx={{ mt: 2 }}>
          {/* Input Gains */}
          <Box>
            <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
              <VolumeDown /> Input Gain
            </Typography>
            <Stack
              direction={{ xs: 'row', sm: 'row' }}
              spacing={4}
              justifyContent="center"
              alignItems="flex-start"
              sx={{ mt: 2 }}
            >
              {/* Input L */}
              <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
                <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: isTrueStereo ? 'success.main' : 'inherit' }}>
                  {isTrueStereo ? 'Left (Guitar)' : 'Input'}
                </Typography>
                <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                  <Slider
                    orientation="vertical"
                    value={inputGainL}
                    onChange={handleInputLChange}
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
                <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
                  {inputGainL.toFixed(1)} dB
                </Typography>
              </Box>

              {/* Input R - Only in True Stereo */}
              {isTrueStereo && (
                <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
                  <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: 'info.main' }}>
                    Right (Mic)
                  </Typography>
                  <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                    <Slider
                      orientation="vertical"
                      value={inputGainR}
                      onChange={handleInputRChange}
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
                  <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
                    {inputGainR.toFixed(1)} dB
                  </Typography>
                </Box>
              )}
            </Stack>
          </Box>

          <Divider />

          {/* Output Gains */}
          <Box>
            <Typography variant="h6" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
              <VolumeUp /> Output Gain
            </Typography>
            <Stack
              direction={{ xs: 'row', sm: 'row' }}
              spacing={4}
              justifyContent="center"
              alignItems="flex-start"
              sx={{ mt: 2 }}
            >
              {/* Output L */}
              <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
                <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: isTrueStereo ? 'success.main' : 'inherit' }}>
                  {isTrueStereo ? 'Left (Guitar)' : 'Output'}
                </Typography>
                <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                  <Slider
                    orientation="vertical"
                    value={outputGainL}
                    onChange={handleOutputLChange}
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
                <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
                  {outputGainL.toFixed(1)} dB
                </Typography>
              </Box>

              {/* Output R - Only in True Stereo */}
              {isTrueStereo && (
                <Box sx={{ textAlign: 'center', width: { xs: '50%', sm: 'auto' } }}>
                  <Typography variant="caption" display="block" gutterBottom sx={{ fontWeight: 'bold', color: 'info.main' }}>
                    Right (Mic)
                  </Typography>
                  <Box sx={{ height: { xs: 150, sm: 200 }, display: 'flex', justifyContent: 'center', my: 2 }}>
                    <Slider
                      orientation="vertical"
                      value={outputGainR}
                      onChange={handleOutputRChange}
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
                  <Typography variant="h6" sx={{ fontWeight: 'bold' }}>
                    {outputGainR.toFixed(1)} dB
                  </Typography>
                </Box>
              )}
            </Stack>
          </Box>
        </Stack>
      </DialogContent>
    </Dialog>
  );
}

export default GainControls;
