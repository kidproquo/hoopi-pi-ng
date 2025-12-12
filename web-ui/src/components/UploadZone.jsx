import { useState, useCallback } from 'react';
import { Box, Button, Typography, LinearProgress } from '@mui/material';
import { CloudUpload } from '@mui/icons-material';

export default function UploadZone({ onUpload, disabled }) {
  const [dragActive, setDragActive] = useState(false);
  const [uploading, setUploading] = useState(false);

  const handleDrag = useCallback((e) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  }, []);

  const handleDrop = useCallback(
    async (e) => {
      e.preventDefault();
      e.stopPropagation();
      setDragActive(false);

      if (disabled || uploading) return;

      const files = e.dataTransfer.files;
      if (files && files[0]) {
        const file = files[0];
        if (file.name.endsWith('.zip')) {
          setUploading(true);
          await onUpload(file);
          setUploading(false);
        } else {
          alert('Please upload a .zip file');
        }
      }
    },
    [disabled, uploading, onUpload]
  );

  const handleFileInput = useCallback(
    async (e) => {
      if (disabled || uploading) return;

      const files = e.target.files;
      if (files && files[0]) {
        const file = files[0];
        if (file.name.endsWith('.zip')) {
          setUploading(true);
          await onUpload(file);
          setUploading(false);
        } else {
          alert('Please upload a .zip file');
        }
      }
    },
    [disabled, uploading, onUpload]
  );

  return (
    <Box
      onDragEnter={handleDrag}
      onDragLeave={handleDrag}
      onDragOver={handleDrag}
      onDrop={handleDrop}
      sx={{
        border: '2px dashed',
        borderColor: dragActive ? 'primary.main' : 'divider',
        borderRadius: 2,
        p: 4,
        textAlign: 'center',
        bgcolor: dragActive ? 'action.hover' : 'transparent',
        transition: 'all 0.2s',
        cursor: disabled || uploading ? 'not-allowed' : 'pointer',
      }}
    >
      <CloudUpload sx={{ fontSize: 48, color: 'text.secondary', mb: 2 }} />
      <Typography variant="h6" gutterBottom>
        {uploading ? 'Uploading...' : 'Drop zip file here or click to browse'}
      </Typography>
      <Typography variant="body2" color="text.secondary" gutterBottom>
        Supported formats: .zip containing .nam or .json files
      </Typography>
      {uploading && <LinearProgress sx={{ mt: 2 }} />}
      {!uploading && (
        <Button
          component="label"
          variant="contained"
          startIcon={<CloudUpload />}
          sx={{ mt: 2 }}
          disabled={disabled}
        >
          Browse Files
          <input
            type="file"
            accept=".zip"
            hidden
            onChange={handleFileInput}
            disabled={disabled || uploading}
          />
        </Button>
      )}
    </Box>
  );
}
