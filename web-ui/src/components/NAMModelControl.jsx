import { Paper, Typography, Box, Button, Stack, Chip, Switch, FormControlLabel, IconButton } from '@mui/material';
import { CheckCircle, RadioButtonUnchecked, Block, SwapHoriz, Edit } from '@mui/icons-material';

function NAMModelControl({ status, onActivateSlot, onBypassModelToggle, onEditModel }) {
  if (!status) {
    return (
      <Paper sx={{ p: { xs: 1.5, sm: 2 }, height: '100%', display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
        <Typography variant="h6" sx={{ fontSize: { xs: '0.95rem', sm: '1.25rem' } }}>NAM Model</Typography>
        <Box sx={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          <Typography variant="body2" color="text.secondary">
            Loading...
          </Typography>
        </Box>
      </Paper>
    );
  }

  const getSlotIcon = (slotIndex) => {
    if (!status.modelReady || !status.modelReady[slotIndex]) {
      return <Block color="disabled" fontSize="small" />;
    }
    if (status.activeModel === slotIndex) {
      return <CheckCircle color="success" fontSize="small" />;
    }
    return <RadioButtonUnchecked color="action" fontSize="small" />;
  };

  return (
    <Paper sx={{ p: { xs: 1.5, sm: 2 }, height: '100%', display: 'flex', flexDirection: 'column', width: '100%', boxSizing: 'border-box', overflow: 'hidden' }}>
      <Typography variant="h6" gutterBottom sx={{ fontSize: { xs: '0.95rem', sm: '1.25rem' } }}>NAM Model</Typography>

      {/* Bypass Toggle */}
      <FormControlLabel
        control={
          <Switch
            checked={!status.bypassModelL}
            onChange={onBypassModelToggle}
            color="success"
            size="small"
          />
        }
        label={status.bypassModelL ? "Bypassed" : "Active"}
        sx={{ mb: 1 }}
      />

      {/* Active Model Info */}
      {status.modelNames?.[status.activeModel] && (
        <Box sx={{ mb: 2, p: 1, bgcolor: 'action.hover', borderRadius: 1 }}>
          <Typography variant="body2" sx={{ fontFamily: 'monospace', fontSize: '0.75rem' }}>
            {status.modelNames[status.activeModel]}
          </Typography>
        </Box>
      )}

      {/* Slot Buttons */}
      <Stack spacing={1} sx={{ flex: 1 }}>
        {[0, 1].map((slotIndex) => {
          const isReady = status.modelReady && status.modelReady[slotIndex];
          const isActive = status.activeModel === slotIndex;
          const modelName = status.modelNames?.[slotIndex];

          return (
            <Box
              key={slotIndex}
              sx={{
                display: 'flex',
                alignItems: 'center',
                gap: 1,
                p: 1,
                border: 1,
                borderColor: isActive ? 'success.main' : 'divider',
                borderRadius: 1,
                bgcolor: isActive ? 'action.selected' : 'transparent',
              }}
            >
              {getSlotIcon(slotIndex)}

              <Box sx={{ flex: 1, minWidth: 0 }}>
                <Typography variant="caption" display="block">
                  Slot {slotIndex}
                </Typography>
                {modelName && (
                  <Typography variant="caption" color="text.secondary" sx={{ fontSize: '0.65rem' }} noWrap>
                    {modelName.split('/').pop()}
                  </Typography>
                )}
                {!isReady && (
                  <Typography variant="caption" color="text.secondary">
                    Empty
                  </Typography>
                )}
              </Box>

              {isReady && !isActive && (
                <Button
                  size="small"
                  variant="outlined"
                  onClick={() => onActivateSlot(slotIndex)}
                  sx={{ minWidth: 'auto', px: 1, fontSize: '0.7rem' }}
                >
                  Use
                </Button>
              )}

              {onEditModel && (
                <IconButton
                  size="small"
                  onClick={() => onEditModel(slotIndex)}
                  sx={{ p: 0.5 }}
                >
                  <Edit fontSize="small" />
                </IconButton>
              )}
            </Box>
          );
        })}
      </Stack>
    </Paper>
  );
}

export default NAMModelControl;
