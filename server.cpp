#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <cstring>
#include <atomic>
#include <portaudio.h>

#ifdef _WIN32
#include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

const int SAMPLE_RATE = 48000;
const int FRAMES_PER_BUFFER = 1024;
const int CHANNELS = 1;
const int PORT = 8888;

#ifdef _WIN32
typedef SOCKET SocketType;
    #define INVALID_SOCKET_HANDLE INVALID_SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SOCKET_ERROR_CODE SOCKET_ERROR
#else
typedef int SocketType;
#define INVALID_SOCKET_HANDLE -1
#define CLOSE_SOCKET(s) close(s)
#define SOCKET_ERROR_CODE -1
#endif

struct Client {
    SocketType socket;
    std::string name;
    std::thread receive_thread;
};

class VoiceServer {
private:
    std::vector<std::unique_ptr<Client>> clients;
    std::mutex clients_mutex;
    SocketType server_socket;
    std::atomic<bool> is_running;
    std::thread accept_thread;

public:
    VoiceServer() : server_socket(INVALID_SOCKET_HANDLE), is_running(false) {}

    bool initialize() {
#ifdef _WIN32
        WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                std::cerr << "WSAStartup failed" << std::endl;
                return false;
            }
#endif

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET_HANDLE) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }


        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_CODE) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }

        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR_CODE) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }

        std::cout << "Server started on port " << PORT << std::endl;
        return true;
    }

    void start() {
        is_running = true;
        accept_thread = std::thread(&VoiceServer::acceptClients, this);
    }

    void stop() {
        is_running = false;


        if (server_socket != INVALID_SOCKET_HANDLE) {
            CLOSE_SOCKET(server_socket);
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto& client : clients) {
                if (client->socket != INVALID_SOCKET_HANDLE) {
                    CLOSE_SOCKET(client->socket);
                }
            }
            clients.clear();
        }

        if (accept_thread.joinable()) {
            accept_thread.join();
        }

#ifdef _WIN32
        WSACleanup();
#endif
    }

private:
    void acceptClients() {
        while (is_running) {
            sockaddr_in client_addr;
#ifdef _WIN32
            int addr_len = sizeof(client_addr);
#else
            socklen_t addr_len = sizeof(client_addr);
#endif

            SocketType client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);

            if (client_socket == INVALID_SOCKET_HANDLE) {
                if (is_running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }


            char name_buffer[256];
            int bytes_received = recv(client_socket, name_buffer, sizeof(name_buffer) - 1, 0);
            if (bytes_received > 0) {
                name_buffer[bytes_received] = '\0';

                auto client = std::make_unique<Client>();
                client->socket = client_socket;
                client->name = name_buffer;

                std::lock_guard<std::mutex> lock(clients_mutex);
                client->receive_thread = std::thread(&VoiceServer::handleClient, this, client.get());
                clients.push_back(std::move(client));

                std::cout << "Client connected: " << name_buffer << std::endl;
            } else {
                CLOSE_SOCKET(client_socket);
            }
        }
    }

    void handleClient(Client* client) {
        std::vector<float> audio_buffer(FRAMES_PER_BUFFER * CHANNELS);

        while (is_running) {
            int bytes_received = recv(client->socket,
                                      reinterpret_cast<char*>(audio_buffer.data()),
                                      audio_buffer.size() * sizeof(float),
                                      0);

            if (bytes_received <= 0) {
                break;
            }

            broadcastAudio(audio_buffer, client->socket);
        }

        removeClient(client);
    }

    void broadcastAudio(const std::vector<float>& audio_data, SocketType sender_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);

        for (auto& client : clients) {
            if (client->socket != sender_socket && client->socket != INVALID_SOCKET_HANDLE) {
                send(client->socket,
                     reinterpret_cast<const char*>(audio_data.data()),
                     audio_data.size() * sizeof(float),
                     0);
            }
        }
    }

    void removeClient(Client* client_to_remove) {
        std::lock_guard<std::mutex> lock(clients_mutex);

        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->get() == client_to_remove) {
                std::cout << "Client disconnected: " << client_to_remove->name << std::endl;

                if (client_to_remove->receive_thread.joinable()) {
                    client_to_remove->receive_thread.join();
                }

                if (client_to_remove->socket != INVALID_SOCKET_HANDLE) {
                    CLOSE_SOCKET(client_to_remove->socket);
                }

                clients.erase(it);
                break;
            }
        }
    }
};

int main() {
    VoiceServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    server.start();

    std::cout << "Voice server running. Press Enter to stop..." << std::endl;
    std::cin.get();

    server.stop();
    std::cout << "Server stopped" << std::endl;

    return 0;
}