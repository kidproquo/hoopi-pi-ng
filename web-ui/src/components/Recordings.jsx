import { Paper, Typography, Button, Box, List, ListItem, ListItemIcon, ListItemText, IconButton, Divider, FormControlLabel, Checkbox } from '@mui/material';
import { AudioFile, PlayArrow, Download, Delete, Refresh, RadioButtonUnchecked, Stop } from '@mui/icons-material';
import { useState, useRef, useEffect } from 'react';

function Recordings({ status, onStartRecording, onStopRecording, recordings, onRefresh, onDelete }) {
  const [playingFile, setPlayingFile] = useState(null);
  const [localDuration, setLocalDuration] = useState(0);
  const [includeBackingTrack, setIncludeBackingTrack] = useState(false);
  const audioRef = useRef(null);
  const timerRef = useRef(null);
  const isRecording = status?.recording || false;

  // Sync local timer with server-side duration when recording status changes
  useEffect(() => {
    if (isRecording) {
      // Recording started or ongoing - sync with server time
      const serverDuration = Math.floor(status?.recordingDuration || 0);
      setLocalDuration(serverDuration);

      // Start local timer that increments every second
      timerRef.current = setInterval(() => {
        setLocalDuration(prev => prev + 1);
      }, 1000);
    } else {
      // Recording stopped - clear timer and reset
      if (timerRef.current) {
        clearInterval(timerRef.current);
        timerRef.current = null;
      }
      setLocalDuration(0);
    }

    // Cleanup on unmount or when recording status changes
    return () => {
      if (timerRef.current) {
        clearInterval(timerRef.current);
        timerRef.current = null;
      }
    };
  }, [isRecording, status?.recordingDuration]);

  // Load backing track recording setting on mount
  useEffect(() => {
    const loadBackingTrackSetting = async () => {
      try {
        const response = await fetch('/api/backing-tracks/include-in-recording');
        const data = await response.json();
        if (data.success) {
          setIncludeBackingTrack(data.enabled);
        }
      } catch (err) {
        console.error('Error loading backing track recording setting:', err);
      }
    };
    loadBackingTrackSetting();
  }, []);

  const formatDuration = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  const formatBytes = (bytes) => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
  };

  const handleToggleRecording = () => {
    if (isRecording) {
      onStopRecording();
    } else {
      // Prompt for recording name
      const filename = window.prompt('Enter recording name (without .wav extension):');
      if (filename !== null && filename.trim() !== '') {
        const trimmedFilename = filename.trim();
        const wavFilename = trimmedFilename.endsWith('.wav') ? trimmedFilename : `${trimmedFilename}.wav`;

        // Check if file already exists
        const existingFile = recordings.find(rec => rec.filename === wavFilename);
        if (existingFile) {
          const confirmed = window.confirm(
            `Warning: Recording "${wavFilename}" already exists!\n\n` +
            `This will overwrite the existing file (${formatDuration(Math.floor(existingFile.duration || 0))}, ${formatBytes(existingFile.size)}).\n\n` +
            `Do you want to continue?`
          );
          if (!confirmed) {
            return; // User cancelled, don't start recording
          }
        }

        onStartRecording(trimmedFilename);
      }
    }
  };

  const handlePlay = (filename) => {
    if (playingFile === filename) {
      // Stop playing
      if (audioRef.current) {
        audioRef.current.pause();
        audioRef.current.currentTime = 0;
      }
      setPlayingFile(null);
    } else {
      // Start playing
      setPlayingFile(filename);
      if (audioRef.current) {
        audioRef.current.src = `/api/recordings/${filename}`;
        audioRef.current.play();
      }
    }
  };

  const handleDownload = (filename) => {
    const link = document.createElement('a');
    link.href = `/api/recordings/${filename}`;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const handleDelete = async (filename) => {
    if (window.confirm(`Delete recording "${filename}"?`)) {
      await onDelete(filename);
      // Stop playing if this file was playing
      if (playingFile === filename) {
        setPlayingFile(null);
        if (audioRef.current) {
          audioRef.current.pause();
        }
      }
    }
  };

  const handleIncludeBackingTrackChange = async (event) => {
    const enabled = event.target.checked;
    setIncludeBackingTrack(enabled);
    try {
      await fetch('/api/backing-tracks/include-in-recording', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
      });
    } catch (err) {
      console.error('Error setting backing track recording option:', err);
      // Revert on error
      setIncludeBackingTrack(!enabled);
    }
  };

  // Sort recordings to show latest first
  const sortedRecordings = [...recordings].sort((a, b) => {
    // Compare by date/time in descending order (newest first)
    return new Date(b.date) - new Date(a.date);
  });

  return (
    <Paper sx={{ p: { xs: 2, sm: 3 }, display: 'flex', flexDirection: 'column', width: '100%', maxHeight: 600, overflow: 'hidden' }}>
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
        <Typography variant="h5">
          Recordings ({recordings.length})
        </Typography>
        <IconButton onClick={onRefresh} size="small">
          <Refresh />
        </IconButton>
      </Box>

      {/* Recording Controls */}
      <Box sx={{ mb: 2 }}>
        <Button
          variant={isRecording ? "contained" : "outlined"}
          color={isRecording ? "error" : "primary"}
          size="large"
          onClick={handleToggleRecording}
          startIcon={isRecording ? <Stop /> : <RadioButtonUnchecked />}
          fullWidth
          sx={{
            height: 60,
            fontSize: '1.1rem',
            ...(isRecording && {
              animation: 'pulse 2s infinite',
              '@keyframes pulse': {
                '0%, 100%': { opacity: 1 },
                '50%': { opacity: 0.7 }
              }
            })
          }}
        >
          {isRecording ? `Recording... ${formatDuration(localDuration)}` : 'Start Recording'}
        </Button>

        {isRecording && (
          <Box sx={{ mt: 1, textAlign: 'center' }}>
            <Typography variant="body1" sx={{ fontWeight: 'bold', mb: 0.5 }}>
              {status.recordingFile?.split('/').pop() || 'Unknown'}
            </Typography>
            {status.recordingDroppedFrames > 0 && (
              <Typography variant="caption" color="warning.main" sx={{ display: 'block' }}>
                Warning: {status.recordingDroppedFrames} frames dropped
              </Typography>
            )}
          </Box>
        )}

        {!isRecording && (
          <>
            <Typography variant="caption" color="text.secondary" sx={{ display: 'block', mt: 1, textAlign: 'center' }}>
              Click to start recording processed audio output to a stereo WAV file
            </Typography>
            <FormControlLabel
              control={
                <Checkbox
                  checked={includeBackingTrack}
                  onChange={handleIncludeBackingTrackChange}
                  size="small"
                />
              }
              label={
                <Typography variant="caption" color="text.secondary">
                  Include backing track in recording
                </Typography>
              }
              sx={{ mt: 1, ml: 0, display: 'flex', justifyContent: 'center' }}
            />
          </>
        )}
      </Box>

      <Divider sx={{ my: 2 }} />

      {/* Recordings List */}
      {recordings.length === 0 ? (
        <Typography variant="body2" color="text.secondary" sx={{ textAlign: 'center', py: 4 }}>
          No recordings yet. Start recording to create your first recording!
        </Typography>
      ) : (
        <>
          <List sx={{ flex: 1, overflow: 'auto', maxHeight: 400 }}>
            {sortedRecordings.map((recording) => (
              <ListItem
                key={recording.filename}
                sx={{
                  border: 1,
                  borderColor: 'divider',
                  borderRadius: 1,
                  mb: 1,
                  bgcolor: playingFile === recording.filename ? 'action.selected' : 'transparent'
                }}
              >
                <ListItemIcon>
                  <AudioFile />
                </ListItemIcon>

                <ListItemText
                  primary={recording.filename}
                  secondary={`${formatDuration(Math.floor(recording.duration || 0))} • ${formatBytes(recording.size)} • ${recording.date}`}
                  secondaryTypographyProps={{ component: 'div' }}
                />

                <IconButton
                  onClick={() => handlePlay(recording.filename)}
                  color={playingFile === recording.filename ? "primary" : "default"}
                  size="small"
                >
                  <PlayArrow />
                </IconButton>

                <IconButton
                  onClick={() => handleDownload(recording.filename)}
                  size="small"
                >
                  <Download />
                </IconButton>

                <IconButton
                  onClick={() => handleDelete(recording.filename)}
                  size="small"
                  color="error"
                >
                  <Delete />
                </IconButton>
              </ListItem>
            ))}
          </List>

          {/* Hidden audio element for playback */}
          <audio
            ref={audioRef}
            onEnded={() => setPlayingFile(null)}
            onError={() => setPlayingFile(null)}
          />
        </>
      )}
    </Paper>
  );
}

export default Recordings;
