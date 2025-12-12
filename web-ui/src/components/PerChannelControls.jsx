import {
  Dialog,
  DialogTitle,
  DialogContent,
  Box,
  Typography,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Switch,
  FormControlLabel,
  Stack,
  Paper,
  Divider,
  IconButton
} from '@mui/material';
import { Close } from '@mui/icons-material';

function PerChannelControls({
  open,
  onClose,
  status,
  onActiveSlotLChange,
  onActiveSlotRChange,
  onBypassModelLToggle,
  onBypassModelRToggle
}) {
  if (!status) return null;

  const handleSlotLChange = (event) => {
    if (onActiveSlotLChange) {
      onActiveSlotLChange(event.target.value);
    }
  };

  const handleSlotRChange = (event) => {
    if (onActiveSlotRChange) {
      onActiveSlotRChange(event.target.value);
    }
  };

  const handleBypassLToggle = (event) => {
    if (onBypassModelLToggle) {
      onBypassModelLToggle(event.target.checked);
    }
  };

  const handleBypassRToggle = (event) => {
    if (onBypassModelRToggle) {
      onBypassModelRToggle(event.target.checked);
    }
  };

  const getModelName = (slot) => {
    if (status.modelNames && status.modelNames[slot]) {
      return status.modelNames[slot];
    }
    return 'No model loaded';
  };

  return (
    <Dialog open={open} onClose={onClose} maxWidth="sm" fullWidth>
      <DialogTitle>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Typography variant="h5">NAM Control (Left Channel)</Typography>
          <IconButton onClick={onClose} size="small">
            <Close />
          </IconButton>
        </Box>
      </DialogTitle>
      <DialogContent>
        <Stack spacing={3} sx={{ mt: 2 }}>
          {/* Left Channel Controls */}
          <Paper elevation={2} sx={{ p: 2, backgroundColor: 'rgba(76, 175, 80, 0.1)' }}>
            <Typography variant="h6" gutterBottom color="success.main">
              NAM Processing (Guitar/Input 1)
            </Typography>

            <FormControl fullWidth size="small" sx={{ mt: 2 }}>
              <InputLabel>Active Slot</InputLabel>
              <Select
                value={status.activeModelL ?? 0}
                label="Active Slot"
                onChange={handleSlotLChange}
              >
                <MenuItem value={0}>
                  Slot 0: {getModelName(0)}
                </MenuItem>
                <MenuItem value={1}>
                  Slot 1: {getModelName(1)}
                </MenuItem>
              </Select>
            </FormControl>

            <FormControlLabel
              control={
                <Switch
                  checked={status.bypassModelL || false}
                  onChange={handleBypassLToggle}
                  color="warning"
                />
              }
              label={
                <Typography variant="body2">
                  {status.bypassModelL ? 'NAM Bypassed (Direct Signal)' : 'NAM Active (Processing)'}
                </Typography>
              }
              sx={{ mt: 2 }}
            />
          </Paper>

          {/* Info Box */}
          <Box sx={{ p: 2, backgroundColor: 'rgba(255, 255, 255, 0.05)', borderRadius: 1 }}>
            <Typography variant="caption" color="text.secondary">
              <strong>Note:</strong> NAM processing is only available for the left channel (Input 1).
              <br />• Left: Guitar/instrument with NAM amp modeling
              <br />• Right: Mic/line (direct signal, no NAM)
              <br />• Use Stereo Mode "True Stereo" to process both inputs independently
              <br />• Gains, Noise Gate, and EQ are available for both channels
            </Typography>
          </Box>
        </Stack>
      </DialogContent>
    </Dialog>
  );
}

export default PerChannelControls;
