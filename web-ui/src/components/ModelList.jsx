import {
  List,
  ListItem,
  ListItemButton,
  ListItemText,
  ListItemIcon,
  IconButton,
  Typography,
  Box,
  CircularProgress,
  TextField,
  InputAdornment,
  Breadcrumbs,
  Link,
} from '@mui/material';
import { AudioFile, Refresh, Search, Clear, Folder, FolderOpen, Home } from '@mui/icons-material';
import { useState, useMemo, useEffect } from 'react';
import { listFolderModels } from '../api';

function formatFileSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

export default function ModelList({ models, onLoadModel, loading, onRefresh, selectedSlot }) {
  const [searchTerm, setSearchTerm] = useState('');
  const [currentFolder, setCurrentFolder] = useState(null);
  const [folderModels, setFolderModels] = useState([]);
  const [loadingFolder, setLoadingFolder] = useState(false);

  // Extract folders and files from models prop
  const { folders, files } = useMemo(() => {
    if (models && models.folders && models.files) {
      return { folders: models.folders, files: models.files };
    }
    // Fallback for old format
    return { folders: [], files: Array.isArray(models) ? models : [] };
  }, [models]);

  // Get all models for search (from current view)
  const allModels = useMemo(() => {
    if (currentFolder) {
      return folderModels;
    }
    return files;
  }, [currentFolder, folderModels, files]);

  // Filter models based on search term
  const filteredModels = useMemo(() => {
    if (!searchTerm.trim()) return allModels;

    const search = searchTerm.toLowerCase();
    return allModels.filter(model =>
      model.name.toLowerCase().includes(search) ||
      model.path.toLowerCase().includes(search)
    );
  }, [allModels, searchTerm]);

  const handleFolderClick = async (folder) => {
    setLoadingFolder(true);
    setCurrentFolder(folder.name);
    setSearchTerm(''); // Clear search when entering folder
    try {
      const response = await listFolderModels(folder.name);
      setFolderModels(response.models || []);
    } catch (error) {
      console.error('Failed to load folder:', error);
      setFolderModels([]);
    } finally {
      setLoadingFolder(false);
    }
  };

  const handleBackToRoot = () => {
    setCurrentFolder(null);
    setFolderModels([]);
    setSearchTerm('');
  };

  const handleModelClick = (model) => {
    if (onLoadModel) {
      onLoadModel(model.path, selectedSlot);
    }
  };

  return (
    <>
      <Box sx={{ display: 'flex', gap: 2, mb: 2, alignItems: 'center' }}>
        <TextField
          fullWidth
          size="small"
          placeholder={currentFolder ? `Search in ${currentFolder}...` : "Search models..."}
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          InputProps={{
            startAdornment: (
              <InputAdornment position="start">
                <Search />
              </InputAdornment>
            ),
            endAdornment: searchTerm && (
              <InputAdornment position="end">
                <IconButton
                  size="small"
                  onClick={() => setSearchTerm('')}
                  edge="end"
                >
                  <Clear />
                </IconButton>
              </InputAdornment>
            ),
          }}
        />
        <IconButton onClick={onRefresh} disabled={loading} size="small">
          <Refresh />
        </IconButton>
      </Box>

      {/* Breadcrumbs */}
      {currentFolder && (
        <Breadcrumbs sx={{ mb: 2 }}>
          <Link
            component="button"
            variant="body2"
            onClick={handleBackToRoot}
            sx={{ display: 'flex', alignItems: 'center', gap: 0.5, cursor: 'pointer' }}
          >
            <Home fontSize="small" />
            All Models
          </Link>
          <Typography color="text.primary" sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
            <FolderOpen fontSize="small" />
            {currentFolder}
          </Typography>
        </Breadcrumbs>
      )}

      {(loading && !currentFolder) || loadingFolder ? (
        <Box sx={{ display: 'flex', justifyContent: 'center', p: 4 }}>
          <CircularProgress />
        </Box>
      ) : filteredModels.length === 0 && folders.length === 0 && !currentFolder ? (
        <Box sx={{ textAlign: 'center', p: 4 }}>
          <Typography variant="body2" color="text.secondary">
            {searchTerm ? `No models found matching "${searchTerm}"` : 'No models found. Upload some models to get started.'}
          </Typography>
        </Box>
      ) : (
        <Box>
          {searchTerm && (
            <Typography variant="caption" color="text.secondary" sx={{ mb: 1, display: 'block' }}>
              Found {filteredModels.length} {currentFolder ? `in ${currentFolder}` : 'models'}
            </Typography>
          )}
          <List sx={{ maxHeight: 500, overflow: 'auto' }}>
            {/* Show folders only in root view and when not searching */}
            {!currentFolder && !searchTerm && folders.map((folder, index) => (
              <ListItem key={`folder-${index}`} disablePadding>
                <ListItemButton onClick={() => handleFolderClick(folder)}>
                  <ListItemIcon>
                    <Folder color="primary" />
                  </ListItemIcon>
                  <ListItemText
                    primary={folder.name}
                    secondary={`${folder.modelCount} model${folder.modelCount !== 1 ? 's' : ''}`}
                  />
                </ListItemButton>
              </ListItem>
            ))}

            {/* Show models */}
            {filteredModels.map((model, index) => (
              <ListItem key={`model-${index}`} disablePadding>
                <ListItemButton onClick={() => handleModelClick(model)}>
                  <ListItemIcon>
                    <AudioFile />
                  </ListItemIcon>
                  <ListItemText
                    primary={model.name}
                    secondary={
                      <Box sx={{ display: 'flex', gap: 1, alignItems: 'center', mt: 0.5 }}>
                        <Typography variant="caption" color="text.secondary">
                          {formatFileSize(model.size)}
                        </Typography>
                        {currentFolder && model.path !== model.name && (
                          <Typography variant="caption" color="text.secondary">
                            • {model.path.split('/').slice(1, -1).join('/')}
                          </Typography>
                        )}
                      </Box>
                    }
                    secondaryTypographyProps={{ component: 'div' }}
                  />
                </ListItemButton>
              </ListItem>
            ))}
          </List>
        </Box>
      )}
    </>
  );
}
