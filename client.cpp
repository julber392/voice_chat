#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
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
const std::string SERVER_IP = "127.0.0.1";
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

class VoiceClient {
private:
    SocketType socket;
    PaStream* input_stream;
    PaStream* output_stream;
    std::atomic<bool> is_running;
    std::thread receive_thread;

public:
    VoiceClient() : socket(INVALID_SOCKET_HANDLE), input_stream(nullptr), output_stream(nullptr), is_running(false) {}

    bool initialize(const std::string& username) {
#ifdef _WIN32
        WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                std::cerr << "WSAStartup failed" << std::endl;
                return false;
            }
#endif

        socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket == INVALID_SOCKET_HANDLE) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);

#ifdef _WIN32
        InetPton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr);
#else
        inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr);
#endif

        if (connect(socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_CODE) {
            std::cerr << "Connection failed" << std::endl;
            return false;
        }

        send(socket, username.c_str(), username.length(), 0);

        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        err = Pa_OpenDefaultStream(&input_stream,
                                   CHANNELS,
                                   0,
                                   paFloat32,
                                   SAMPLE_RATE,
                                   FRAMES_PER_BUFFER,
                                   audioInputCallback,
                                   this);

        if (err != paNoError) {
            std::cerr << "Input stream error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }


        err = Pa_OpenDefaultStream(&output_stream,
                                   0,           // input channels
                                   CHANNELS,    // output channels
                                   paFloat32,   // sample format
                                   SAMPLE_RATE,
                                   FRAMES_PER_BUFFER,
                                   audioOutputCallback,
                                   this);

        if (err != paNoError) {
            std::cerr << "Output stream error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        std::cout << "Connected to server as: " << username << std::endl;
        return true;
    }

    void start() {
        is_running = true;


        Pa_StartStream(input_stream);
        Pa_StartStream(output_stream);


        receive_thread = std::thread(&VoiceClient::receiveAudio, this);

        std::cout << "Voice chat started. Press Enter to stop..." << std::endl;
    }

    void stop() {
        is_running = false;


        if (input_stream) {
            Pa_StopStream(input_stream);
            Pa_CloseStream(input_stream);
        }
        if (output_stream) {
            Pa_StopStream(output_stream);
            Pa_CloseStream(output_stream);
        }

        // Закрытие сокета
        if (socket != INVALID_SOCKET_HANDLE) {
            CLOSE_SOCKET(socket);
        }

        if (receive_thread.joinable()) {
            receive_thread.join();
        }

        Pa_Terminate();

#ifdef _WIN32
        WSACleanup();
#endif

        std::cout << "Voice chat stopped" << std::endl;
    }

private:
    static int audioInputCallback(const void* inputBuffer, void* outputBuffer,
                                  unsigned long framesPerBuffer,
                                  const PaStreamCallbackTimeInfo* timeInfo,
                                  PaStreamCallbackFlags statusFlags,
                                  void* userData) {
        VoiceClient* client = static_cast<VoiceClient*>(userData);

        if (inputBuffer && client->is_running) {
            const float* samples = static_cast<const float*>(inputBuffer);

            send(client->socket,
                 reinterpret_cast<const char*>(samples),
                 framesPerBuffer * CHANNELS * sizeof(float),
                 0);
        }

        return paContinue;
    }

    static int audioOutputCallback(const void* inputBuffer, void* outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void* userData) {
        VoiceClient* client = static_cast<VoiceClient*>(userData);
        float* out = static_cast<float*>(outputBuffer);


        return paContinue;
    }

    void receiveAudio() {
        std::vector<float> audio_buffer(FRAMES_PER_BUFFER * CHANNELS);

        while (is_running) {
            int bytes_received = recv(socket,
                                      reinterpret_cast<char*>(audio_buffer.data()),
                                      audio_buffer.size() * sizeof(float),
                                      0);

            if (bytes_received > 0) {

                Pa_WriteStream(output_stream, audio_buffer.data(), FRAMES_PER_BUFFER);
            } else {
                break;
            }
        }
    }
};

int main() {
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    VoiceClient client;

    if (!client.initialize(username)) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }

    client.start();

    std::cout << "Press Enter to disconnect..." << std::endl;
    std::cin.get();

    client.stop();

    return 0;
}