import { Box, Card, CardContent, Typography, IconButton, Slider,
         Button, List, ListItem, ListItemText, LinearProgress, Stack, Chip, TextField } from '@mui/material';
import { PlayArrow, Stop, Pause, Loop, VolumeUp, Upload, Refresh } from '@mui/icons-material';
import { useState, useEffect, useRef } from 'react';

function BackingTrackControl() {
  const [files, setFiles] = useState([]);
  const [currentTrack, setCurrentTrack] = useState(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const [loopEnabled, setLoopEnabled] = useState(true);
  const [volume, setVolume] = useState(0.7);
  const [progress, setProgress] = useState(0);
  const [loading, setLoading] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [startInput, setStartInput] = useState('0');
  const [stopInput, setStopInput] = useState('0');
  const settingsLoaded = useRef(false);

  useEffect(() => {
    loadFileList();
    // Poll status every second
    const interval = setInterval(updateStatus, 1000);
    return () => clearInterval(interval);
  }, []);

  const loadFileList = async () => {
    try {
      const response = await fetch('/api/backing-tracks/list');
      const data = await response.json();
      if (data.success) {
        setFiles(data.files || []);
      }
    } catch (err) {
      console.error('Error loading backing track list:', err);
    }
  };

  const updateStatus = async () => {
    try {
      const response = await fetch('/api/backing-tracks/status');
      const data = await response.json();
      if (data.success) {
        // Always update playback state and progress (these reflect actual playback)
        setIsPlaying(data.playing);
        if (data.loaded && data.duration > 0) {
          const progressPercent = (data.position / data.duration) * 100;
          setProgress(progressPercent);

          // Load settings (volume, loop, positions) only once when track first loads
          if (!settingsLoaded.current) {
            setVolume(data.volume);
            setLoopEnabled(data.looping);
            if (data.startPosition !== undefined && data.stopPosition !== undefined) {
              setStartInput(data.startPosition.toFixed(1));
              setStopInput(data.stopPosition.toFixed(1));
            }
            settingsLoaded.current = true;
          }
        } else {
          setProgress(0);
          // Reset flag when no track loaded
          if (settingsLoaded.current) {
            setStartInput('0');
            setStopInput('0');
            settingsLoaded.current = false;
          }
        }
      }
    } catch (err) {
      console.error('Error updating backing track status:', err);
    }
  };

  const handleUpload = async (event) => {
    const file = event.target.files[0];
    if (!file) return;

    // Validate audio file (WAV or MP3)
    const fileName = file.name.toLowerCase();
    if (!fileName.endsWith('.wav') && !fileName.endsWith('.mp3')) {
      alert('Only WAV and MP3 files are supported');
      return;
    }

    setUploading(true);
    const formData = new FormData();
    formData.append('file', file);

    try {
      const response = await fetch('/api/backing-tracks/upload', {
        method: 'POST',
        body: formData
      });

      const data = await response.json();
      if (data.success) {
        await loadFileList();
      } else {
        alert('Upload failed: ' + (data.error || 'Unknown error'));
      }
    } catch (err) {
      alert('Upload error: ' + err.message);
    } finally {
      setUploading(false);
      event.target.value = ''; // Reset file input
    }
  };

  const handleLoadTrack = async (file) => {
    setLoading(true);
    // Reset settings flag when loading new track
    settingsLoaded.current = false;

    try {
      const response = await fetch('/api/backing-tracks/load', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ filepath: file.path })
      });

      const data = await response.json();
      if (data.success) {
        setCurrentTrack({
          filename: data.filename,
          duration: data.duration,
          channels: data.channels,
          sampleRate: data.sampleRate
        });
        // Settings (volume, loop, positions) will be loaded from backend on next status poll
      } else {
        alert('Failed to load track: ' + (data.error || 'Unknown error'));
      }
    } catch (err) {
      alert('Load error: ' + err.message);
    } finally {
      setLoading(false);
    }
  };

  const handlePlay = async () => {
    try {
      await fetch('/api/backing-tracks/play', { method: 'POST' });
      // isPlaying will be updated by status poll
    } catch (err) {
      console.error('Play error:', err);
    }
  };

  const handleStop = async () => {
    try {
      await fetch('/api/backing-tracks/stop', { method: 'POST' });
      // isPlaying and progress will be updated by status poll
    } catch (err) {
      console.error('Stop error:', err);
    }
  };

  const handlePause = async () => {
    try {
      await fetch('/api/backing-tracks/pause', { method: 'POST' });
      // isPlaying will be updated by status poll
    } catch (err) {
      console.error('Pause error:', err);
    }
  };

  const handleVolumeChange = async (event, newValue) => {
    setVolume(newValue);
    try {
      await fetch('/api/backing-tracks/volume', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ volume: newValue })
      });
    } catch (err) {
      console.error('Volume error:', err);
    }
  };

  const handleLoopToggle = async () => {
    const newLoop = !loopEnabled;
    setLoopEnabled(newLoop);
    try {
      await fetch('/api/backing-tracks/loop', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: newLoop })
      });
    } catch (err) {
      console.error('Loop error:', err);
    }
  };

  const handleStartInputChange = async (newValue) => {
    setStartInput(newValue);
    const seconds = parseFloat(newValue);
    if (isNaN(seconds) || seconds < 0) return;

    try {
      await fetch('/api/backing-tracks/start-position', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ seconds })
      });
    } catch (err) {
      console.error('Start position error:', err);
    }
  };

  const handleStopInputChange = async (newValue) => {
    setStopInput(newValue);
    const seconds = parseFloat(newValue);
    if (isNaN(seconds) || seconds < 0) return;

    try {
      await fetch('/api/backing-tracks/stop-position', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ seconds })
      });
    } catch (err) {
      console.error('Stop position error:', err);
    }
  };

  const formatDuration = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  return (
    <Card>
      <CardContent>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6">
            Backing Track
          </Typography>
          <IconButton onClick={loadFileList} size="small" title="Refresh file list">
            <Refresh />
          </IconButton>
        </Box>

        {/* File Upload */}
        <Box sx={{ mb: 2 }}>
          <input
            accept=".wav,.mp3"
            style={{ display: 'none' }}
            id="backing-track-upload"
            type="file"
            onChange={handleUpload}
            disabled={uploading}
          />
          <label htmlFor="backing-track-upload">
            <Button
              variant="outlined"
              component="span"
              startIcon={<Upload />}
              disabled={uploading}
              fullWidth
            >
              {uploading ? 'Uploading...' : 'Upload Audio File'}
            </Button>
          </label>
        </Box>

        {/* File List */}
        <Box sx={{ mb: 2, maxHeight: 200, overflow: 'auto' }}>
          <Typography variant="subtitle2" color="text.secondary" gutterBottom>
            Available Files
          </Typography>
          <List dense>
            {files.length === 0 ? (
              <Typography variant="body2" color="text.secondary" sx={{ p: 1 }}>
                No backing tracks available. Upload a WAV file or record something first.
              </Typography>
            ) : (
              files.map((file) => (
                <ListItem
                  key={file.path}
                  component="div"
                  selected={currentTrack?.filename === file.name}
                  onClick={() => handleLoadTrack(file)}
                  disabled={loading}
                  sx={{ cursor: 'pointer' }}
                >
                  <ListItemText
                    primary={file.name}
                    secondary={
                      <Stack direction="row" spacing={1} alignItems="center">
                        <Chip label={file.source} size="small" variant="outlined" />
                        <Typography variant="caption">
                          {(file.size / 1024 / 1024).toFixed(2)} MB
                        </Typography>
                      </Stack>
                    }
                    secondaryTypographyProps={{ component: 'div' }}
                  />
                </ListItem>
              ))
            )}
          </List>
        </Box>

        {/* Current Track Info & Controls */}
        {currentTrack && (
          <Box sx={{ mt: 2, p: 2, bgcolor: 'background.default', borderRadius: 1 }}>
            <Typography variant="body2" color="text.secondary" gutterBottom>
              Now Playing
            </Typography>
            <Typography variant="subtitle1" gutterBottom>
              {currentTrack.filename}
            </Typography>

            {currentTrack.duration && (
              <Typography variant="caption" color="text.secondary">
                Duration: {formatDuration(currentTrack.duration)} •
                {currentTrack.channels === 1 ? ' Mono' : ' Stereo'} •
                {' '}{currentTrack.sampleRate / 1000} kHz
              </Typography>
            )}

            {/* Start/Stop Position Controls */}
            <Box sx={{ display: 'flex', gap: 2, mt: 2 }}>
              <TextField
                label="Start (seconds)"
                type="number"
                value={startInput}
                onChange={(e) => handleStartInputChange(e.target.value)}
                size="small"
                inputProps={{ step: 0.1, min: 0, max: currentTrack.duration }}
                sx={{ flex: 1 }}
              />
              <TextField
                label="Stop (seconds, 0=end)"
                type="number"
                value={stopInput}
                onChange={(e) => handleStopInputChange(e.target.value)}
                size="small"
                inputProps={{ step: 0.1, min: 0, max: currentTrack.duration }}
                sx={{ flex: 1 }}
              />
            </Box>

            {/* Progress Bar */}
            <LinearProgress
              variant="determinate"
              value={progress}
              sx={{ my: 2 }}
            />

            {/* Playback Controls */}
            <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
              <IconButton
                onClick={isPlaying ? handlePause : handlePlay}
                color="primary"
                size="large"
              >
                {isPlaying ? <Pause fontSize="large" /> : <PlayArrow fontSize="large" />}
              </IconButton>

              <IconButton onClick={handleStop}>
                <Stop />
              </IconButton>

              <IconButton
                onClick={handleLoopToggle}
                color={loopEnabled ? 'primary' : 'default'}
                title={loopEnabled ? 'Loop enabled' : 'Loop disabled'}
              >
                <Loop />
              </IconButton>

              <Box sx={{ flex: 1 }} />

              <VolumeUp />
              <Slider
                value={volume}
                onChange={handleVolumeChange}
                min={0}
                max={1}
                step={0.01}
                sx={{ width: 120 }}
                valueLabelDisplay="auto"
                valueLabelFormat={(value) => `${Math.round(value * 100)}%`}
              />
            </Box>
          </Box>
        )}

        {!currentTrack && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 2, textAlign: 'center' }}>
            Select a file to load
          </Typography>
        )}
      </CardContent>
    </Card>
  );
}

export default BackingTrackControl;
