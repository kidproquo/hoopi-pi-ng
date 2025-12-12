#include <httplib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <nlohmann/json.hpp>

// Alternative: use the RTNeural json directly if needed
// #include "../../dependencies/NeuralAudio/deps/RTNeural/modules/json/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// All directories in build folder for self-contained deployment
const std::string MODELS_DIR = "./models";
const std::string STATIC_DIR = "./web-ui";
const std::string RECORDINGS_DIR = "./recordings";
const std::string BACKING_TRACKS_DIR = "./backing-tracks";
const int PORT = 11995;

// List all .nam and .json model files (hierarchical structure)
json listModels() {
    json result = json::object();
    result["folders"] = json::array();
    result["files"] = json::array();

    try {
        if (!fs::exists(MODELS_DIR)) {
            fs::create_directories(MODELS_DIR);
        }

        // First pass: collect all subfolders
        std::set<std::string> folders;
        for (const auto& entry : fs::directory_iterator(MODELS_DIR)) {
            if (entry.is_directory()) {
                json folder;
                folder["name"] = entry.path().filename().string();
                folder["path"] = entry.path().filename().string();

                // Count models in this folder
                int modelCount = 0;
                for (const auto& modelEntry : fs::recursive_directory_iterator(entry.path())) {
                    if (modelEntry.is_regular_file()) {
                        std::string ext = modelEntry.path().extension().string();
                        if (ext == ".nam" || ext == ".json") {
                            modelCount++;
                        }
                    }
                }
                folder["modelCount"] = modelCount;
                result["folders"].push_back(folder);
            }
        }

        // Second pass: collect files in root directory (not in subfolders)
        for (const auto& entry : fs::directory_iterator(MODELS_DIR)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".nam" || ext == ".json") {
                    json model;
                    model["name"] = entry.path().filename().string();
                    model["path"] = entry.path().filename().string();
                    model["size"] = entry.file_size();
                    result["files"].push_back(model);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing models: " << e.what() << std::endl;
    }

    return result;
}

// Get WAV file duration in seconds
double getWavDuration(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return 0.0;
    }

    // Read WAV header
    char riff[4];
    uint32_t fileSize;
    char wave[4];

    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&fileSize), 4);
    file.read(wave, 4);

    if (std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") {
        return 0.0;
    }

    // Find fmt chunk
    uint32_t sampleRate = 0;
    uint16_t numChannels = 0;
    uint16_t bitsPerSample = 0;

    while (file) {
        char chunkId[4];
        uint32_t chunkSize;

        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);

        if (std::string(chunkId, 4) == "fmt ") {
            uint16_t audioFormat;
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.seekg(6, std::ios::cur); // Skip byteRate and blockAlign
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            file.seekg(chunkSize - 16, std::ios::cur); // Skip rest of chunk
        } else if (std::string(chunkId, 4) == "data") {
            // Calculate duration
            if (sampleRate > 0 && numChannels > 0 && bitsPerSample > 0) {
                uint32_t bytesPerSample = bitsPerSample / 8;
                uint32_t totalSamples = chunkSize / (numChannels * bytesPerSample);
                return static_cast<double>(totalSamples) / sampleRate;
            }
            break;
        } else {
            // Skip unknown chunk
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    return 0.0;
}

// List all .wav recording files
json listRecordings() {
    json recordings = json::array();

    try {
        if (!fs::exists(RECORDINGS_DIR)) {
            fs::create_directories(RECORDINGS_DIR);
        }

        for (const auto& entry : fs::directory_iterator(RECORDINGS_DIR)) {
            if (entry.is_regular_file() && entry.path().extension() == ".wav") {
                json recording;
                recording["filename"] = entry.path().filename().string();
                recording["size"] = entry.file_size();

                // Get file modification time
                auto ftime = fs::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                auto time_t = std::chrono::system_clock::to_time_t(sctp);

                char buf[64];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
                recording["date"] = buf;

                // Get WAV duration
                recording["duration"] = getWavDuration(entry.path().string());

                recordings.push_back(recording);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing recordings: " << e.what() << std::endl;
    }

    return recordings;
}

// Unzip uploaded file
bool unzipFile(const std::string& zipPath, const std::string& destPath) {
    std::string cmd = "unzip -o \"" + zipPath + "\" -d \"" + destPath + "\" 2>&1";
    int result = std::system(cmd.c_str());

    // Remove the zip file after extraction
    fs::remove(zipPath);

    return (result == 0);
}

// Send command to HoopiPi via IPC
json sendIPCCommand(const json& command, const std::string& socketPath = "/tmp/hoopi-pi.sock") {
    json response;

    // Create socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        response["success"] = false;
        response["error"] = "Failed to create socket";
        return response;
    }

    // Connect to HoopiPi
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        response["success"] = false;
        response["error"] = "Failed to connect to HoopiPi (is it running?)";
        close(sock);
        return response;
    }

    // Send command
    std::string cmdStr = command.dump();
    if (send(sock, cmdStr.c_str(), cmdStr.length(), 0) < 0) {
        response["success"] = false;
        response["error"] = "Failed to send command";
        close(sock);
        return response;
    }

    // Receive response
    char buffer[4096];
    ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    if (bytesRead <= 0) {
        response["success"] = false;
        response["error"] = "Failed to receive response";
        return response;
    }

    buffer[bytesRead] = '\0';

    try {
        response = json::parse(buffer);
    } catch (const json::exception& e) {
        response["success"] = false;
        response["error"] = std::string("Failed to parse response: ") + e.what();
    }

    return response;
}

int main() {
    httplib::Server svr;

    // Enable CORS for all origins
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // Handle OPTIONS requests for CORS
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // GET /api/models - List all models (hierarchical)
    svr.Get("/api/models", [](const httplib::Request&, httplib::Response& res) {
        json response = listModels();
        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/models/folder/:folderName - List models in a specific folder
    svr.Get(R"(/api/models/folder/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string folderName = req.matches[1];
        json models = json::array();

        try {
            std::string folderPath = MODELS_DIR + "/" + folderName;

            // Validate folder exists
            if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
                json response;
                response["success"] = false;
                response["error"] = "Folder not found: " + folderName;
                res.status = 404;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // List all models in this folder (recursively)
            for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".nam" || ext == ".json") {
                        json model;
                        model["name"] = entry.path().filename().string();
                        // Path relative to the models directory (includes folder name)
                        model["path"] = fs::relative(entry.path(), MODELS_DIR).string();
                        model["size"] = entry.file_size();
                        models.push_back(model);
                    }
                }
            }
        } catch (const std::exception& e) {
            json response;
            response["success"] = false;
            response["error"] = std::string("Error listing folder: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        json response;
        response["folder"] = folderName;
        response["models"] = models;
        response["count"] = models.size();
        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/models/upload - Upload and extract model zip
    svr.Post("/api/models/upload", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        if (!req.has_file("file")) {
            response["success"] = false;
            response["error"] = "No file uploaded";
            res.status = 400;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        const auto& file = req.get_file_value("file");

        // Check if it's a zip file
        if (file.filename.length() < 4 ||
            file.filename.substr(file.filename.length() - 4) != ".zip") {
            response["success"] = false;
            response["error"] = "File must be a .zip archive";
            res.status = 400;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        // Extract folder name from zip filename (without .zip extension)
        std::string zipFilename = file.filename;
        std::string folderName = zipFilename.substr(0, zipFilename.length() - 4);
        std::string extractPath = MODELS_DIR + "/" + folderName;

        // Save uploaded file temporarily
        std::string tempZipPath = MODELS_DIR + "/temp_upload.zip";

        try {
            // Ensure models directory exists
            if (!fs::exists(MODELS_DIR)) {
                fs::create_directories(MODELS_DIR);
            }

            // Create subfolder for this zip
            if (!fs::exists(extractPath)) {
                fs::create_directories(extractPath);
            }

            // Write uploaded file
            std::ofstream ofs(tempZipPath, std::ios::binary);
            ofs.write(file.content.data(), file.content.size());
            ofs.close();

            // Extract zip into subfolder
            if (unzipFile(tempZipPath, extractPath)) {
                response["success"] = true;
                response["message"] = "Models uploaded and extracted successfully";
                response["filename"] = file.filename;
                response["models"] = listModels();
                res.status = 200;
            } else {
                response["success"] = false;
                response["error"] = "Failed to extract zip file";
                res.status = 500;
            }
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Upload failed: ") + e.what();
            res.status = 500;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/models/load - Load a specific model
    svr.Post("/api/models/load", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("modelPath")) {
                response["success"] = false;
                response["error"] = "Missing 'modelPath' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            std::string modelPath = body["modelPath"];
            std::string fullPath = fs::absolute(MODELS_DIR).string() + "/" + modelPath;

            // Check if model file exists
            if (!fs::exists(fullPath)) {
                response["success"] = false;
                response["error"] = "Model file not found: " + modelPath;
                res.status = 404;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Get slot from request (default 0)
            int slot = body.value("slot", 0);

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "loadModel";
            ipcCommand["modelPath"] = fullPath;
            ipcCommand["slot"] = slot;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/models/activate - Set active model slot
    svr.Post("/api/models/activate", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("slot")) {
                response["success"] = false;
                response["error"] = "Missing 'slot' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            int slot = body["slot"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setActiveModel";
            ipcCommand["slot"] = slot;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/status - Get engine status
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        json ipcCommand;
        ipcCommand["action"] = "getStatus";

        json response = sendIPCCommand(ipcCommand);
        res.status = response["success"] ? 200 : 500;
        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/models/activate-l - Set active model slot for left channel
    svr.Post("/api/models/activate-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("slot")) {
                response["success"] = false;
                response["error"] = "Missing 'slot' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            int slot = body["slot"];

            json ipcCommand;
            ipcCommand["action"] = "setActiveModelL";
            ipcCommand["slot"] = slot;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/models/activate-r - Set active model slot for right channel
    svr.Post("/api/models/activate-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("slot")) {
                response["success"] = false;
                response["error"] = "Missing 'slot' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            int slot = body["slot"];

            json ipcCommand;
            ipcCommand["action"] = "setActiveModelR";
            ipcCommand["slot"] = slot;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/bypass-model-l - Bypass NAM model for left channel
    svr.Post("/api/settings/bypass-model-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("bypass")) {
                response["success"] = false;
                response["error"] = "Missing 'bypass' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool bypass = body["bypass"];

            json ipcCommand;
            ipcCommand["action"] = "setBypassModelL";
            ipcCommand["bypass"] = bypass;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/bypass-model-r - Bypass NAM model for right channel
    svr.Post("/api/settings/bypass-model-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("bypass")) {
                response["success"] = false;
                response["error"] = "Missing 'bypass' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool bypass = body["bypass"];

            json ipcCommand;
            ipcCommand["action"] = "setBypassModelR";
            ipcCommand["bypass"] = bypass;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/input-gain - Set input gain
    svr.Post("/api/settings/input-gain", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setInputGain";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/output-gain - Set output gain
    svr.Post("/api/settings/output-gain", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setOutputGain";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/bypass-model - Set model bypass
    svr.Post("/api/settings/bypass-model", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("bypass")) {
                response["success"] = false;
                response["error"] = "Missing 'bypass' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool bypass = body["bypass"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setBypassModel";
            ipcCommand["bypass"] = bypass;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-enabled - Set EQ enabled
    svr.Post("/api/settings/eq-enabled", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setEQEnabled";
            ipcCommand["enabled"] = enabled;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-bass - Set EQ bass gain
    svr.Post("/api/settings/eq-bass", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setEQBass";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-mid - Set EQ mid gain
    svr.Post("/api/settings/eq-mid", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setEQMid";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-treble - Set EQ treble gain
    svr.Post("/api/settings/eq-treble", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setEQTreble";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/noise-gate-enabled - Set noise gate enabled
    svr.Post("/api/settings/noise-gate-enabled", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setNoiseGateEnabled";
            ipcCommand["enabled"] = enabled;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/noise-gate-threshold - Set noise gate threshold
    svr.Post("/api/settings/noise-gate-threshold", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("threshold")) {
                response["success"] = false;
                response["error"] = "Missing 'threshold' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float threshold = body["threshold"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setNoiseGateThreshold";
            ipcCommand["threshold"] = threshold;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/stereo-mode - Set stereo mode
    svr.Post("/api/settings/stereo-mode", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("mode")) {
                response["success"] = false;
                response["error"] = "Missing 'mode' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            std::string mode = body["mode"];

            json ipcCommand;
            ipcCommand["action"] = "setStereoMode";
            ipcCommand["mode"] = mode;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/stereo2mono-mix-l - Set Stereo2Mono left mix level
    svr.Post("/api/settings/stereo2mono-mix-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("level")) {
                response["success"] = false;
                response["error"] = "Missing 'level' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float level = body["level"];

            json ipcCommand;
            ipcCommand["action"] = "setStereo2MonoMixL";
            ipcCommand["level"] = level;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/stereo2mono-mix-r - Set Stereo2Mono right mix level
    svr.Post("/api/settings/stereo2mono-mix-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("level")) {
                response["success"] = false;
                response["error"] = "Missing 'level' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float level = body["level"];

            json ipcCommand;
            ipcCommand["action"] = "setStereo2MonoMixR";
            ipcCommand["level"] = level;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/input-gain-l - Set left input gain
    svr.Post("/api/settings/input-gain-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setInputGainL";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/input-gain-r - Set right input gain
    svr.Post("/api/settings/input-gain-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setInputGainR";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/output-gain-l - Set left output gain
    svr.Post("/api/settings/output-gain-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setOutputGainL";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/output-gain-r - Set right output gain
    svr.Post("/api/settings/output-gain-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setOutputGainR";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/noise-gate-l - Set left noise gate
    svr.Post("/api/settings/noise-gate-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled") || !body.contains("threshold")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' or 'threshold' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];
            float threshold = body["threshold"];

            json ipcCommand;
            ipcCommand["action"] = "setNoiseGateL";
            ipcCommand["enabled"] = enabled;
            ipcCommand["threshold"] = threshold;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/noise-gate-r - Set right noise gate
    svr.Post("/api/settings/noise-gate-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled") || !body.contains("threshold")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' or 'threshold' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];
            float threshold = body["threshold"];

            json ipcCommand;
            ipcCommand["action"] = "setNoiseGateR";
            ipcCommand["enabled"] = enabled;
            ipcCommand["threshold"] = threshold;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-enabled-l - Set left EQ enabled
    svr.Post("/api/settings/eq-enabled-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];

            json ipcCommand;
            ipcCommand["action"] = "setEQEnabledL";
            ipcCommand["enabled"] = enabled;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-enabled-r - Set right EQ enabled
    svr.Post("/api/settings/eq-enabled-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];

            json ipcCommand;
            ipcCommand["action"] = "setEQEnabledR";
            ipcCommand["enabled"] = enabled;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-bass-l - Set left EQ bass
    svr.Post("/api/settings/eq-bass-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQBassL";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-mid-l - Set left EQ mid
    svr.Post("/api/settings/eq-mid-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQMidL";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-treble-l - Set left EQ treble
    svr.Post("/api/settings/eq-treble-l", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQTrebleL";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-bass-r - Set right EQ bass
    svr.Post("/api/settings/eq-bass-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQBassR";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-mid-r - Set right EQ mid
    svr.Post("/api/settings/eq-mid-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQMidR";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/eq-treble-r - Set right EQ treble
    svr.Post("/api/settings/eq-treble-r", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("gain")) {
                response["success"] = false;
                response["error"] = "Missing 'gain' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float gain = body["gain"];

            json ipcCommand;
            ipcCommand["action"] = "setEQTrebleR";
            ipcCommand["gain"] = gain;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/recording/start - Start recording
    svr.Post("/api/recording/start", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);
            std::string filename = body.value("filename", "");

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "startRecording";
            ipcCommand["filename"] = filename;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/recording/stop - Stop recording
    svr.Post("/api/recording/stop", [](const httplib::Request&, httplib::Response& res) {
        json ipcCommand;
        ipcCommand["action"] = "stopRecording";

        json response = sendIPCCommand(ipcCommand);
        res.status = response["success"] ? 200 : 500;
        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/reverb-enabled - Set reverb enabled
    svr.Post("/api/settings/reverb-enabled", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("enabled")) {
                response["success"] = false;
                response["error"] = "Missing 'enabled' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            bool enabled = body["enabled"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setReverbEnabled";
            ipcCommand["enabled"] = enabled;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/reverb-room-size - Set reverb room size
    svr.Post("/api/settings/reverb-room-size", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("size")) {
                response["success"] = false;
                response["error"] = "Missing 'size' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float size = body["size"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setReverbRoomSize";
            ipcCommand["size"] = size;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/reverb-decay-time - Set reverb decay time
    svr.Post("/api/settings/reverb-decay-time", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("seconds")) {
                response["success"] = false;
                response["error"] = "Missing 'seconds' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float seconds = body["seconds"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setReverbDecayTime";
            ipcCommand["seconds"] = seconds;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/settings/reverb-mix - Set reverb dry/wet mix
    svr.Post("/api/settings/reverb-mix", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            auto body = json::parse(req.body);

            if (!body.contains("dry") || !body.contains("wet")) {
                response["success"] = false;
                response["error"] = "Missing 'dry' or 'wet' parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            float dry = body["dry"];
            float wet = body["wet"];

            // Send IPC command to HoopiPi
            json ipcCommand;
            ipcCommand["action"] = "setReverbMix";
            ipcCommand["dry"] = dry;
            ipcCommand["wet"] = wet;

            response = sendIPCCommand(ipcCommand);
            res.status = response["success"] ? 200 : 500;

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/recordings - List all recordings
    svr.Get("/api/recordings", [](const httplib::Request&, httplib::Response& res) {
        json response;
        response["success"] = true;
        response["recordings"] = listRecordings();

        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/recordings/:filename - Download/stream recording
    svr.Get(R"(/api/recordings/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.matches[1];
        std::string filepath = RECORDINGS_DIR + "/" + filename;

        // Validate filename (prevent directory traversal)
        if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos) {
            res.status = 400;
            json error;
            error["success"] = false;
            error["error"] = "Invalid filename";
            res.set_content(error.dump(2), "application/json");
            return;
        }

        // Check if file exists
        if (!fs::exists(filepath)) {
            res.status = 404;
            json error;
            error["success"] = false;
            error["error"] = "Recording not found";
            res.set_content(error.dump(2), "application/json");
            return;
        }

        // Serve WAV file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            res.status = 500;
            json error;
            error["success"] = false;
            error["error"] = "Failed to open file";
            res.set_content(error.dump(2), "application/json");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        res.set_content(content, "audio/wav");
        res.set_header("Content-Disposition", "inline; filename=\"" + filename + "\"");
    });

    // DELETE /api/recordings/:filename - Delete recording
    svr.Delete(R"(/api/recordings/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string filename = req.matches[1];
        std::string filepath = RECORDINGS_DIR + "/" + filename;

        // Validate filename (prevent directory traversal)
        if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos) {
            response["success"] = false;
            response["error"] = "Invalid filename";
            res.status = 400;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        // Check if file exists
        if (!fs::exists(filepath)) {
            response["success"] = false;
            response["error"] = "Recording not found";
            res.status = 404;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        // Delete file
        try {
            fs::remove(filepath);
            response["success"] = true;
            response["message"] = "Recording deleted";
            res.status = 200;
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Failed to delete file: ") + e.what();
            res.status = 500;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // === Backing Track Endpoints ===

    // GET /api/backing-tracks/list - List all audio files (WAV/MP3) from backing-tracks and recordings
    svr.Get("/api/backing-tracks/list", [](const httplib::Request&, httplib::Response& res) {
        json response;
        json files = json::array();

        try {
            // Create backing-tracks directory if it doesn't exist
            if (!fs::exists(BACKING_TRACKS_DIR)) {
                fs::create_directories(BACKING_TRACKS_DIR);
            }

            // List files from backing-tracks directory
            for (const auto& entry : fs::directory_iterator(BACKING_TRACKS_DIR)) {
                std::string ext = entry.path().extension().string();
                if (entry.is_regular_file() && (ext == ".wav" || ext == ".mp3")) {
                    json file;
                    file["name"] = entry.path().filename().string();
                    file["path"] = entry.path().string();
                    file["source"] = "backing-tracks";
                    file["size"] = entry.file_size();
                    files.push_back(file);
                }
            }

            // List files from recordings directory
            if (fs::exists(RECORDINGS_DIR)) {
                for (const auto& entry : fs::directory_iterator(RECORDINGS_DIR)) {
                    std::string ext = entry.path().extension().string();
                    if (entry.is_regular_file() && (ext == ".wav" || ext == ".mp3")) {
                        json file;
                        file["name"] = entry.path().filename().string();
                        file["path"] = entry.path().string();
                        file["source"] = "recordings";
                        file["size"] = entry.file_size();
                        files.push_back(file);
                    }
                }
            }

            response["success"] = true;
            response["files"] = files;
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Failed to list backing tracks: ") + e.what();
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/upload - Upload audio file (WAV or MP3)
    svr.Post("/api/backing-tracks/upload", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            // Create backing-tracks directory if it doesn't exist
            if (!fs::exists(BACKING_TRACKS_DIR)) {
                fs::create_directories(BACKING_TRACKS_DIR);
            }

            // Check if file was uploaded
            if (req.files.empty() || req.files.find("file") == req.files.end()) {
                response["success"] = false;
                response["error"] = "No file uploaded";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            const auto& file = req.files.find("file")->second;
            std::string filename = file.filename;

            // Validate audio file extension (WAV or MP3)
            bool isValidExtension = false;
            if (filename.length() >= 4) {
                std::string ext = filename.substr(filename.length() - 4);
                isValidExtension = (ext == ".wav" || ext == ".mp3");
            }

            if (!isValidExtension) {
                response["success"] = false;
                response["error"] = "Only WAV and MP3 files are supported";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Save file
            std::string filepath = BACKING_TRACKS_DIR + "/" + filename;
            std::ofstream ofs(filepath, std::ios::binary);
            ofs.write(file.content.c_str(), file.content.size());
            ofs.close();

            response["success"] = true;
            response["filename"] = filename;
            response["path"] = filepath;
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Upload failed: ") + e.what();
            res.status = 500;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/load - Load a backing track
    svr.Post("/api/backing-tracks/load", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            std::string filepath = body.value("filepath", "");

            if (filepath.empty()) {
                response["success"] = false;
                response["error"] = "Missing filepath parameter";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Send IPC command
            json ipc_response = sendIPCCommand(json{{"action", "loadBackingTrack"}, {"filepath", filepath}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Load failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/backing-tracks/play - Play backing track
    svr.Post("/api/backing-tracks/play", [](const httplib::Request&, httplib::Response& res) {
        json ipc_response = sendIPCCommand(json{{"action", "playBackingTrack"}});
        res.set_content(ipc_response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/stop - Stop backing track
    svr.Post("/api/backing-tracks/stop", [](const httplib::Request&, httplib::Response& res) {
        json ipc_response = sendIPCCommand(json{{"action", "stopBackingTrack"}});
        res.set_content(ipc_response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/pause - Pause backing track
    svr.Post("/api/backing-tracks/pause", [](const httplib::Request&, httplib::Response& res) {
        json ipc_response = sendIPCCommand(json{{"action", "pauseBackingTrack"}});
        res.set_content(ipc_response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/volume - Set backing track volume
    svr.Post("/api/backing-tracks/volume", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            float volume = body.value("volume", 0.7f);

            json ipc_response = sendIPCCommand(json{{"action", "setBackingTrackVolume"}, {"volume", volume}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Volume set failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/backing-tracks/loop - Set loop enabled/disabled
    svr.Post("/api/backing-tracks/loop", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            bool enabled = body.value("enabled", true);

            json ipc_response = sendIPCCommand(json{{"action", "setBackingTrackLoop"}, {"enabled", enabled}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Loop set failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/backing-tracks/include-in-recording - Set whether backing track is included in recordings
    svr.Post("/api/backing-tracks/include-in-recording", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            bool enabled = body.value("enabled", false);

            json ipc_response = sendIPCCommand(json{{"action", "setIncludeBackingTrackInRecording"}, {"enabled", enabled}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Set include in recording failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/backing-tracks/include-in-recording - Get whether backing track is included in recordings
    svr.Get("/api/backing-tracks/include-in-recording", [](const httplib::Request&, httplib::Response& res) {
        json ipc_response = sendIPCCommand(json{{"action", "getIncludeBackingTrackInRecording"}});
        res.set_content(ipc_response.dump(2), "application/json");
    });

    // POST /api/backing-tracks/start-position - Set start position in seconds
    svr.Post("/api/backing-tracks/start-position", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            float seconds = body.value("seconds", 0.0f);

            json ipc_response = sendIPCCommand(json{{"action", "setBackingTrackStartPosition"}, {"seconds", seconds}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Set start position failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/backing-tracks/stop-position - Set stop position in seconds (0 = end of file)
    svr.Post("/api/backing-tracks/stop-position", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            json body = json::parse(req.body);
            float seconds = body.value("seconds", 0.0f);

            json ipc_response = sendIPCCommand(json{{"action", "setBackingTrackStopPosition"}, {"seconds", seconds}});
            res.set_content(ipc_response.dump(2), "application/json");
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Set stop position failed: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/backing-tracks/status - Get backing track status
    svr.Get("/api/backing-tracks/status", [](const httplib::Request&, httplib::Response& res) {
        json ipc_response = sendIPCCommand(json{{"action", "getBackingTrackStatus"}});
        res.set_content(ipc_response.dump(2), "application/json");
    });

    // GET /api/audio/devices - List available audio devices
    svr.Get("/api/audio/devices", [](const httplib::Request&, httplib::Response& res) {
        json response;
        json devices = json::array();

        // Get playback devices from aplay -l
        FILE* playbackPipe = popen("aplay -l 2>/dev/null", "r");
        if (playbackPipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), playbackPipe)) {
                std::string line(buffer);
                // Parse lines like: "card 0: DaisySeed [DaisySeed], device 0:"
                if (line.find("card ") == 0) {
                    size_t cardPos = line.find("card ");
                    size_t colonPos = line.find(":");
                    size_t bracketStart = line.find("[");
                    size_t bracketEnd = line.find("]");

                    if (cardPos != std::string::npos && colonPos != std::string::npos &&
                        bracketStart != std::string::npos && bracketEnd != std::string::npos) {

                        std::string cardNumStr = line.substr(5, colonPos - 5);
                        cardNumStr = cardNumStr.substr(0, cardNumStr.find(" "));
                        std::string name = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);

                        json device;
                        device["id"] = "hw:" + cardNumStr;  // Use card number, not name
                        device["name"] = name + " (card " + cardNumStr + ")";
                        device["card"] = std::stoi(cardNumStr);
                        device["playback"] = true;
                        device["capture"] = false;  // Will be updated if found in arecord
                        devices.push_back(device);
                    }
                }
            }
            pclose(playbackPipe);
        }

        // Get capture devices from arecord -l and update existing devices
        FILE* capturePipe = popen("arecord -l 2>/dev/null", "r");
        if (capturePipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), capturePipe)) {
                std::string line(buffer);
                if (line.find("card ") == 0) {
                    size_t cardPos = line.find("card ");
                    size_t colonPos = line.find(":");
                    size_t bracketStart = line.find("[");
                    size_t bracketEnd = line.find("]");

                    if (cardPos != std::string::npos && colonPos != std::string::npos &&
                        bracketStart != std::string::npos && bracketEnd != std::string::npos) {

                        std::string cardNumStr = line.substr(5, colonPos - 5);
                        cardNumStr = cardNumStr.substr(0, cardNumStr.find(" "));
                        std::string name = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                        std::string deviceId = "hw:" + cardNumStr;

                        // Find and update existing device
                        bool found = false;
                        for (auto& dev : devices) {
                            if (dev["id"] == deviceId) {
                                dev["capture"] = true;
                                found = true;
                                break;
                            }
                        }

                        // If not found, add as capture-only device
                        if (!found) {
                            json device;
                            device["id"] = deviceId;
                            device["name"] = name + " (card " + cardNumStr + ")";
                            device["card"] = std::stoi(cardNumStr);
                            device["playback"] = false;
                            device["capture"] = true;
                            devices.push_back(device);
                        }
                    }
                }
            }
            pclose(capturePipe);
        }

        response["success"] = true;
        response["devices"] = devices;
        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/audio/current - Get current JACK audio device
    svr.Get("/api/audio/current", [](const httplib::Request&, httplib::Response& res) {
        json response;

        // Read ~/.jackdrc
        const char* home = getenv("HOME");
        if (!home) {
            response["success"] = false;
            response["error"] = "HOME environment variable not set";
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        std::string jackdrcPath = std::string(home) + "/.jackdrc";
        std::ifstream jackdrc(jackdrcPath);
        if (!jackdrc.is_open()) {
            response["success"] = false;
            response["error"] = "Could not read ~/.jackdrc";
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
            return;
        }

        std::string line;
        std::getline(jackdrc, line);
        jackdrc.close();

        // Parse JACK command line to extract device
        // Format: /usr/bin/jackd -dalsa -dhw:0 -r48000 -p128 -n2
        std::string device;
        size_t dPos = line.find("-dhw:");

        if (dPos != std::string::npos) {
            size_t start = dPos + 2;  // Skip "-d"
            size_t end = line.find(" ", start);
            if (end == std::string::npos) end = line.length();
            device = line.substr(start, end - start);
        }

        response["success"] = true;
        response["device"] = device;
        response["playbackDevice"] = device;  // For backwards compatibility
        response["captureDevice"] = device;
        response["jackdrc"] = line;
        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/audio/device - Set JACK audio device
    svr.Post("/api/audio/device", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            json reqData = json::parse(req.body);
            std::string deviceId = reqData.value("device", "");

            if (deviceId.empty()) {
                response["success"] = false;
                response["error"] = "Device ID required";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Get HOME directory
            const char* home = getenv("HOME");
            if (!home) {
                response["success"] = false;
                response["error"] = "HOME environment variable not set";
                res.status = 500;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Update ~/.jackdrc
            // Use simpler -d format for duplex mode (works with card numbers only)
            std::string jackdrcPath = std::string(home) + "/.jackdrc";
            std::string newJackdrc = "/usr/bin/jackd -dalsa -d" + deviceId + " -r48000 -p128 -n2";

            std::ofstream jackdrc(jackdrcPath);
            if (!jackdrc.is_open()) {
                response["success"] = false;
                response["error"] = "Could not write to ~/.jackdrc";
                res.status = 500;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            jackdrc << newJackdrc << std::endl;
            jackdrc.close();

            // Restart JACK service (don't start manually, let systemd do it)
            // The systemd service will read the updated ~/.jackdrc file
            system("systemctl --user restart hoopi-jack");
            usleep(1000000);  // Wait 1s for JACK to restart

            response["success"] = true;
            response["message"] = "Audio device updated and JACK restarted";
            response["device"] = deviceId;
            res.set_content(response.dump(2), "application/json");

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("JSON parse error: ") + e.what();
            res.status = 400;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/jack/buffersize - Get current JACK buffer size
    svr.Get("/api/jack/buffersize", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            const char* home = getenv("HOME");
            if (!home) {
                response["success"] = false;
                response["error"] = "HOME environment variable not set";
                res.status = 500;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            std::string jackdrcPath = std::string(home) + "/.jackdrc";
            std::ifstream jackdrc(jackdrcPath);
            if (!jackdrc.is_open()) {
                response["success"] = false;
                response["error"] = "Could not read ~/.jackdrc";
                response["bufferSize"] = 128;  // default
                res.set_content(response.dump(2), "application/json");
                return;
            }

            std::string line;
            int bufferSize = 128;  // default
            if (std::getline(jackdrc, line)) {
                // Parse buffer size from command line: -p<size>
                size_t pPos = line.find(" -p");
                if (pPos != std::string::npos) {
                    pPos += 3;  // skip " -p"
                    size_t endPos = line.find_first_not_of("0123456789", pPos);
                    if (endPos == std::string::npos) endPos = line.length();
                    std::string sizeStr = line.substr(pPos, endPos - pPos);
                    try {
                        bufferSize = std::stoi(sizeStr);
                    } catch (...) {
                        bufferSize = 128;
                    }
                }
            }
            jackdrc.close();

            response["success"] = true;
            response["bufferSize"] = bufferSize;
            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error reading buffer size: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/jack/buffersize - Set JACK buffer size
    svr.Post("/api/jack/buffersize", [](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            json reqData = json::parse(req.body);
            int bufferSize = reqData.value("bufferSize", 128);

            if (bufferSize < 16 || bufferSize > 2048) {
                response["success"] = false;
                response["error"] = "Buffer size must be between 16 and 2048";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            const char* home = getenv("HOME");
            if (!home) {
                response["success"] = false;
                response["error"] = "HOME environment variable not set";
                res.status = 500;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Read existing .jackdrc to get the device
            std::string jackdrcPath = std::string(home) + "/.jackdrc";
            std::ifstream jackdrcIn(jackdrcPath);
            std::string currentLine;
            std::string device = "hw:0";  // default

            if (jackdrcIn.is_open() && std::getline(jackdrcIn, currentLine)) {
                // Extract device from existing command (find the second -d, which is the device)
                size_t firstD = currentLine.find(" -d");
                if (firstD != std::string::npos) {
                    size_t secondD = currentLine.find(" -d", firstD + 3);
                    if (secondD != std::string::npos) {
                        secondD += 3;  // skip " -d"
                        size_t endPos = currentLine.find(" ", secondD);
                        if (endPos == std::string::npos) endPos = currentLine.length();
                        device = currentLine.substr(secondD, endPos - secondD);
                    }
                }
            }
            jackdrcIn.close();

            // Write updated .jackdrc with new buffer size
            std::string newJackdrc = "/usr/bin/jackd -dalsa -d" + device + " -r48000 -p" + std::to_string(bufferSize) + " -n2";
            std::ofstream jackdrc(jackdrcPath);
            if (!jackdrc.is_open()) {
                response["success"] = false;
                response["error"] = "Could not write to ~/.jackdrc";
                res.status = 500;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            jackdrc << newJackdrc << std::endl;
            jackdrc.close();

            // Restart JACK service
            system("systemctl --user restart hoopi-jack");
            usleep(1000000);  // Wait 1s for JACK to restart

            response["success"] = true;
            response["message"] = "Buffer size updated and JACK restarted";
            response["bufferSize"] = bufferSize;
            res.set_content(response.dump(2), "application/json");

        } catch (const json::exception& e) {
            response["success"] = false;
            response["error"] = std::string("JSON parse error: ") + e.what();
            res.status = 400;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/jack/logs - Get last 5 minutes of JACK logs
    svr.Get("/api/jack/logs", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            std::string logs;

            // Try user journal first
            FILE* pipe = popen("journalctl --user -u hoopi-jack --since '5 minutes ago' --no-pager 2>&1", "r");
            if (pipe) {
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    logs += buffer;
                }
                pclose(pipe);
            }

            // If user journal has no entries, try systemctl status for recent logs
            if (logs.empty() || logs.find("No journal files") != std::string::npos || logs.find("No entries") != std::string::npos) {
                logs.clear();
                pipe = popen("systemctl --user status hoopi-jack --no-pager -l 2>&1 | tail -50", "r");
                if (pipe) {
                    char buffer[256];
                    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        logs += buffer;
                    }
                    pclose(pipe);
                }
            }

            if (logs.empty()) {
                logs = "No JACK logs available.";
            }

            response["success"] = true;
            response["logs"] = logs;
            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error fetching logs: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/engine/logs - Get last 5 minutes of HoopiPi engine logs
    svr.Get("/api/engine/logs", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            std::string logs;

            // Try user journal first
            FILE* pipe = popen("journalctl --user -u hoopi-engine --since '5 minutes ago' --no-pager 2>&1", "r");
            if (pipe) {
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    logs += buffer;
                }
                pclose(pipe);
            }

            // If user journal has no entries, try systemctl status for recent logs
            if (logs.empty() || logs.find("No journal files") != std::string::npos || logs.find("No entries") != std::string::npos) {
                logs.clear();
                pipe = popen("systemctl --user status hoopi-engine --no-pager -l 2>&1 | tail -50", "r");
                if (pipe) {
                    char buffer[256];
                    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        logs += buffer;
                    }
                    pclose(pipe);
                }
            }

            if (logs.empty()) {
                logs = "No HoopiPi engine logs available.";
            }

            response["success"] = true;
            response["logs"] = logs;
            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error fetching logs: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/jack/restart - Restart JACK service
    svr.Post("/api/jack/restart", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            // Restart JACK service
            int result = system("systemctl --user restart hoopi-jack 2>&1");

            if (result == 0) {
                response["success"] = true;
                response["message"] = "JACK service restarted successfully";
            } else {
                response["success"] = false;
                response["error"] = "Failed to restart JACK service";
            }

            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error restarting JACK: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // POST /api/engine/restart - Restart HoopiPi engine service
    svr.Post("/api/engine/restart", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            // Restart HoopiPi engine service
            int result = system("systemctl --user restart hoopi-engine 2>&1");

            if (result == 0) {
                response["success"] = true;
                response["message"] = "HoopiPi engine service restarted successfully";
            } else {
                response["success"] = false;
                response["error"] = "Failed to restart HoopiPi engine service";
            }

            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error restarting engine: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api/system - Get system information
    svr.Get("/api/system", [](const httplib::Request&, httplib::Response& res) {
        json response;

        try {
            // Get Pi model from device tree
            std::string piModel = "Unknown";
            std::ifstream modelFile("/proc/device-tree/model");
            if (modelFile.is_open()) {
                std::getline(modelFile, piModel);
                modelFile.close();
                // Remove null terminator if present
                if (!piModel.empty() && piModel.back() == '\0') {
                    piModel.pop_back();
                }
            }

            // Get CPU model from /proc/cpuinfo
            std::string cpuModel = "Unknown";
            std::ifstream cpuFile("/proc/cpuinfo");
            if (cpuFile.is_open()) {
                std::string line;
                while (std::getline(cpuFile, line)) {
                    if (line.find("Model") == 0) {
                        size_t pos = line.find(':');
                        if (pos != std::string::npos) {
                            cpuModel = line.substr(pos + 1);
                            // Trim leading whitespace
                            cpuModel.erase(0, cpuModel.find_first_not_of(" \t"));
                        }
                        break;
                    }
                }
                cpuFile.close();
            }

            // Get total memory from /proc/meminfo
            long totalMemoryKB = 0;
            std::ifstream memFile("/proc/meminfo");
            if (memFile.is_open()) {
                std::string line;
                while (std::getline(memFile, line)) {
                    if (line.find("MemTotal:") == 0) {
                        std::istringstream iss(line);
                        std::string label;
                        iss >> label >> totalMemoryKB;
                        break;
                    }
                }
                memFile.close();
            }
            double totalMemoryMB = totalMemoryKB / 1024.0;

            // Get HoopiPi package version
            std::string packageVersion = "Unknown";
            FILE* pipe = popen("dpkg -l | grep hoopi-pi | awk '{print $3}'", "r");
            if (pipe) {
                char buffer[128];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    packageVersion = buffer;
                    // Remove trailing newline
                    if (!packageVersion.empty() && packageVersion.back() == '\n') {
                        packageVersion.pop_back();
                    }
                }
                pclose(pipe);
            }

            // Get build metadata from systemd service
            // Note: systemctl show doesn't expose custom X- properties, so we use cat
            std::string packageName = "Unknown";
            std::string buildDate = "Unknown";
            std::string buildArch = "Unknown";
            std::string buildCPU = "Unknown";
            std::string buildFlags = "Unknown";

            pipe = popen("systemctl --user cat hoopi-engine.service 2>/dev/null | grep '^X-HoopiPi-'", "r");
            if (pipe) {
                char buffer[512];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    std::string line(buffer);
                    // Remove trailing newline
                    if (!line.empty() && line.back() == '\n') {
                        line.pop_back();
                    }

                    if (line.find("X-HoopiPi-PackageName=") == 0) {
                        packageName = line.substr(22);
                    } else if (line.find("X-HoopiPi-BuildDate=") == 0) {
                        buildDate = line.substr(20);
                    } else if (line.find("X-HoopiPi-Architecture=") == 0) {
                        buildArch = line.substr(23);
                    } else if (line.find("X-HoopiPi-CPU=") == 0) {
                        buildCPU = line.substr(14);
                    } else if (line.find("X-HoopiPi-BuildFlags=") == 0) {
                        buildFlags = line.substr(21);
                    }
                }
                pclose(pipe);
            }

            // Get JACK configuration from .jackdrc
            std::string jackDevice = "Unknown";
            std::string jackDeviceName = "";
            int jackBufferSize = 0;
            std::string jackdrcPath = std::string(getenv("HOME")) + "/.jackdrc";
            std::ifstream jackdrcFile(jackdrcPath);
            if (jackdrcFile.is_open()) {
                std::string jackCmd;
                std::getline(jackdrcFile, jackCmd);
                jackdrcFile.close();

                // Parse device - look for second -d flag (first is -dalsa driver, second is -dhw:0 device)
                size_t firstD = jackCmd.find(" -d");
                if (firstD != std::string::npos) {
                    size_t secondD = jackCmd.find(" -d", firstD + 3);
                    if (secondD != std::string::npos) {
                        secondD += 3;  // skip " -d"
                        size_t endPos = jackCmd.find(" ", secondD);
                        if (endPos == std::string::npos) endPos = jackCmd.length();
                        jackDevice = jackCmd.substr(secondD, endPos - secondD);

                        // Get friendly device name from /proc/asound/cards
                        // Parse hw:X to extract card number
                        if (jackDevice.find("hw:") == 0) {
                            std::string cardNumStr = jackDevice.substr(3);
                            // Remove any subdevice specification (e.g., hw:0,0 -> 0)
                            size_t commaPos = cardNumStr.find(',');
                            if (commaPos != std::string::npos) {
                                cardNumStr = cardNumStr.substr(0, commaPos);
                            }

                            try {
                                int cardNum = std::stoi(cardNumStr);

                                // Read /proc/asound/cards to get friendly name
                                std::ifstream cardsFile("/proc/asound/cards");
                                if (cardsFile.is_open()) {
                                    std::string line;
                                    while (std::getline(cardsFile, line)) {
                                        // Look for line starting with card number
                                        std::istringstream iss(line);
                                        int lineCardNum;
                                        if (iss >> lineCardNum && lineCardNum == cardNum) {
                                            // Extract device name between [ and ]
                                            size_t start = line.find('[');
                                            size_t end = line.find(']');
                                            if (start != std::string::npos && end != std::string::npos && end > start) {
                                                jackDeviceName = line.substr(start + 1, end - start - 1);
                                                // Trim whitespace
                                                jackDeviceName.erase(0, jackDeviceName.find_first_not_of(" \t"));
                                                jackDeviceName.erase(jackDeviceName.find_last_not_of(" \t") + 1);
                                            }
                                            break;
                                        }
                                    }
                                    cardsFile.close();
                                }
                            } catch (...) {
                                // Invalid card number, leave jackDeviceName empty
                            }
                        }
                    }
                }

                // Parse buffer size (-p flag)
                size_t bufferPos = jackCmd.find(" -p");
                if (bufferPos != std::string::npos) {
                    bufferPos += 3;  // skip " -p"
                    size_t endPos = jackCmd.find(" ", bufferPos);
                    if (endPos == std::string::npos) endPos = jackCmd.length();
                    std::string bufferStr = jackCmd.substr(bufferPos, endPos - bufferPos);
                    try {
                        jackBufferSize = std::stoi(bufferStr);
                    } catch (...) {
                        jackBufferSize = 0;
                    }
                }
            }

            response["success"] = true;
            response["piModel"] = piModel;
            response["cpuModel"] = cpuModel;
            response["totalMemoryMB"] = totalMemoryMB;
            response["packageVersion"] = packageVersion;
            response["packageName"] = packageName;
            response["buildDate"] = buildDate;
            response["buildArch"] = buildArch;
            response["buildCPU"] = buildCPU;
            response["buildFlags"] = buildFlags;
            response["jackDevice"] = jackDevice;
            response["jackDeviceName"] = jackDeviceName;
            response["jackBufferSize"] = jackBufferSize;
            res.set_content(response.dump(2), "application/json");

        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Error fetching system info: ") + e.what();
            res.status = 500;
            res.set_content(response.dump(2), "application/json");
        }
    });

    // GET /api - API info
    svr.Get("/api", [](const httplib::Request&, httplib::Response& res) {
        json info;
        info["name"] = "HoopiPi API Server";
        info["version"] = "0.1.0";
        info["endpoints"] = {
            {"GET /api", "API information"},
            {"GET /api/status", "Get engine status"},
            {"GET /api/models", "List all available models"},
            {"POST /api/models/upload", "Upload and extract model zip file"},
            {"POST /api/models/load", "Load a specific model into HoopiPi"},
            {"POST /api/models/activate", "Set active model slot"},
            {"POST /api/settings/input-gain", "Set input gain"},
            {"POST /api/settings/output-gain", "Set output gain"},
            {"POST /api/settings/bypass-model", "Bypass NAM model (keep signal chain)"},
            {"POST /api/settings/eq-enabled", "Enable/disable EQ"},
            {"POST /api/settings/eq-bass", "Set EQ bass gain"},
            {"POST /api/settings/eq-mid", "Set EQ mid gain"},
            {"POST /api/settings/eq-treble", "Set EQ treble gain"},
            {"POST /api/settings/noise-gate-enabled", "Enable/disable noise gate"},
            {"POST /api/settings/noise-gate-threshold", "Set noise gate threshold"},
            {"POST /api/recording/start", "Start recording to WAV file"},
            {"POST /api/recording/stop", "Stop recording"},
            {"GET /api/recordings", "List all recordings"},
            {"GET /api/recordings/:filename", "Download/stream recording"},
            {"DELETE /api/recordings/:filename", "Delete recording"},
            {"GET /api/backing-tracks/list", "List all backing tracks (from both backing-tracks and recordings directories)"},
            {"POST /api/backing-tracks/upload", "Upload WAV file as backing track"},
            {"POST /api/backing-tracks/load", "Load a backing track by filepath"},
            {"POST /api/backing-tracks/play", "Play backing track"},
            {"POST /api/backing-tracks/stop", "Stop backing track"},
            {"POST /api/backing-tracks/pause", "Pause backing track"},
            {"POST /api/backing-tracks/volume", "Set backing track volume (0.0-1.0)"},
            {"POST /api/backing-tracks/loop", "Enable/disable backing track loop"},
            {"POST /api/backing-tracks/include-in-recording", "Enable/disable backing track in recordings"},
            {"GET /api/backing-tracks/include-in-recording", "Get whether backing track is included in recordings"},
            {"POST /api/backing-tracks/start-position", "Set start position in seconds"},
            {"POST /api/backing-tracks/stop-position", "Set stop position in seconds (0 = end of file)"},
            {"GET /api/backing-tracks/status", "Get backing track status (includes start/stop positions)"}
        };

        res.set_content(info.dump(2), "application/json");
    });

    // Serve static web UI files (must be last to not override API routes)
    if (fs::exists(STATIC_DIR)) {
        svr.set_mount_point("/", STATIC_DIR);

        // Serve index.html for root path
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            std::string indexPath = STATIC_DIR + "/index.html";
            if (fs::exists(indexPath)) {
                std::ifstream file(indexPath);
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                res.set_content(content, "text/html");
            } else {
                res.status = 404;
                res.set_content("index.html not found", "text/plain");
            }
        });

        std::cout << "Web UI directory: " << fs::absolute(STATIC_DIR) << std::endl;
    } else {
        std::cout << "Warning: Web UI directory not found: " << STATIC_DIR << std::endl;
        std::cout << "Run 'npm run build' in web-ui/ to build the frontend" << std::endl;
    }

    std::cout << "HoopiPi API Server starting on port " << PORT << "..." << std::endl;
    std::cout << "Models directory: " << fs::absolute(MODELS_DIR) << std::endl;
    std::cout << "\nEndpoints:" << std::endl;
    std::cout << "  GET  /                    - Web UI (if built)" << std::endl;
    std::cout << "  GET  /api                 - API info" << std::endl;
    std::cout << "  GET  /api/models          - List models" << std::endl;
    std::cout << "  POST /api/models/upload   - Upload model zip" << std::endl;
    std::cout << "  POST /api/models/load     - Load model" << std::endl;
    std::cout << std::endl;

    svr.listen("0.0.0.0", PORT);

    return 0;
}
