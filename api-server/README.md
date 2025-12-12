# HoopiPi API Server

REST API server for managing neural amp models in HoopiPi.

## Features

- Upload model zip files and automatically extract them
- List all available models (.nam and .json files)
- Load models into HoopiPi (IPC integration pending)

## Building

The API server is built automatically with the main HoopiPi project:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make HoopiPiAPI
```

## Running

```bash
cd build
./api-server/HoopiPiAPI
```

The server starts on port 11995 by default.

## API Endpoints

### GET /

Returns API information and available endpoints.

**Response:**
```json
{
  "name": "HoopiPi API Server",
  "version": "0.1.0",
  "endpoints": {
    "GET /api/models": "List all available models",
    "POST /api/models/upload": "Upload and extract model zip file",
    "POST /api/models/load": "Load a specific model into HoopiPi"
  }
}
```

### GET /api/models

List all available model files in the models directory.

**Response:**
```json
{
  "count": 1,
  "models": [
    {
      "name": "model.nam",
      "path": "subfolder/model.nam",
      "size": 283622
    }
  ]
}
```

### POST /api/models/upload

Upload a zip file containing model files. The zip will be automatically extracted to the models directory.

**Request:**
- Content-Type: `multipart/form-data`
- Form field: `file` (the zip file)

**Example with curl:**
```bash
curl -X POST -F "file=@models.zip" http://localhost:11995/api/models/upload
```

**Response (success):**
```json
{
  "success": true,
  "message": "Models uploaded and extracted successfully",
  "filename": "models.zip",
  "models": [...]
}
```

**Response (error):**
```json
{
  "success": false,
  "error": "File must be a .zip archive"
}
```

### POST /api/models/load

Request to load a specific model into HoopiPi.

**Request:**
- Content-Type: `application/json`
- Body:
```json
{
  "modelPath": "path/to/model.nam"
}
```

**Example with curl:**
```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"modelPath":"Fender_Super_Reverb.nam"}' \
  http://localhost:11995/api/models/load
```

**Response:**
```json
{
  "success": true,
  "message": "Model load requested (IPC not yet implemented)",
  "modelPath": "/full/path/to/model.nam"
}
```

## CORS

CORS is enabled for all origins to allow web-based frontends.

## Models Directory

Models are stored in `../models/` relative to the API server executable. The directory structure is preserved when extracting zip files, and subdirectories are supported.

## TODO

- [ ] Implement IPC to communicate with running HoopiPi process
- [ ] Add authentication/authorization
- [ ] Add model validation before loading
- [ ] Support for deleting models
- [ ] WebSocket support for real-time status updates
- [ ] Model metadata caching
