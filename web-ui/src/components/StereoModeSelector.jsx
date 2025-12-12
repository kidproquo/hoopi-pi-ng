import { Box, FormControl, InputLabel, Select, MenuItem, Typography } from '@mui/material';

function StereoModeSelector({ stereoMode, onChange }) {
  const handleChange = (event) => {
    if (onChange) {
      onChange(event.target.value);
    }
  };

  const getModeDescription = (mode) => {
    switch (mode) {
      case 'LeftMono2Stereo':
        return 'Process left input → output to both L/R';
      case 'Stereo2Stereo':
        return 'Process left and right independently';
      case 'RightMono2Stereo':
        return 'Process right input → output to both L/R';
      case 'Stereo2Mono':
        return 'Mix left + right inputs → process as mono → output to both L/R';
      default:
        return '';
    }
  };

  return (
    <Box>
      <FormControl fullWidth size="small">
        <InputLabel id="stereo-mode-label">Stereo Mode</InputLabel>
        <Select
          labelId="stereo-mode-label"
          id="stereo-mode-select"
          value={stereoMode || 'LeftMono2Stereo'}
          label="Stereo Mode"
          onChange={handleChange}
        >
          <MenuItem value="LeftMono2Stereo">Left Mono → Stereo</MenuItem>
          <MenuItem value="Stereo2Stereo">True Stereo</MenuItem>
          <MenuItem value="RightMono2Stereo">Right Mono → Stereo</MenuItem>
          <MenuItem value="Stereo2Mono">Stereo → Mono Mix</MenuItem>
        </Select>
      </FormControl>
      <Typography variant="caption" color="text.secondary" sx={{ mt: 0.5, display: 'block' }}>
        {getModeDescription(stereoMode || 'LeftMono2Stereo')}
      </Typography>
    </Box>
  );
}

export default StereoModeSelector;
