#pragma once

#include "Engine.h"
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace HoopiPi {

class JackBackend;  // Forward declaration
class BackingTrack;  // Forward declaration

/**
 * @brief Simple IPC server using Unix domain sockets
 *
 * Accepts commands from the API server to control HoopiPi remotely.
 */
class IPCServer {
public:
    using StatusChangeCallback = std::function<void()>;

    IPCServer(Engine* engine);
    ~IPCServer();

    /**
     * @brief Start the IPC server
     * @param socketPath Path to Unix domain socket (default: /tmp/hoopi-pi.sock)
     * @return true if started successfully
     */
    bool start(const std::string& socketPath = "/tmp/hoopi-pi.sock");

    /**
     * @brief Stop the IPC server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return m_running.load(); }

    /**
     * @brief Set backend reference for status queries
     * @param backend Pointer to JackBackend instance
     */
    void setBackend(JackBackend* backend);

    /**
     * @brief Set backing track reference for control
     * @param backingTrack Pointer to BackingTrack instance
     */
    void setBackingTrack(BackingTrack* backingTrack);

    /**
     * @brief Set callback to be called when settings change
     * @param callback Function to call on status changes
     */
    void setStatusChangeCallback(StatusChangeCallback callback);

private:
    void serverLoop();
    void handleClient(int clientSocket);
    std::string handleCommand(const std::string& command);
    void notifyStatusChange();

    Engine* m_engine;
    JackBackend* m_backend;
    BackingTrack* m_backingTrack;
    int m_serverSocket;
    std::string m_socketPath;
    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_serverThread;
    StatusChangeCallback m_statusChangeCallback;
};

} // namespace HoopiPi
