
// CHAT GPT
#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    using SocketType = SOCKET;
    inline void closeSocket(SocketType s) { closesocket(s); }
    inline void cleanNetwork() { WSACleanup(); }
    inline bool initNetwork() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }

#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>

    using SocketType = int;
    inline void closeSocket(SocketType s) { close(s); }
    inline void cleanNetwork() {}
    inline bool initNetwork() { return true; }
#endif

/**
 * Sendet eine TCP-Nachricht an eine IP-Adresse und einen Port.
 * @return true bei Erfolg, false bei Fehlern.
 */
inline bool sendMessage(const std::string& ip, int port, const std::string& message) {
    if (!initNetwork()) {
        std::cerr << "Fatal error while initializing the network.\n";
        return false;
    }

    SocketType sock = socket(AF_INET, SOCK_STREAM, 0);
#if defined(_WIN32) || defined(_WIN64)
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        std::cerr << "Socket setup failed.\n";
        cleanNetwork();
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP-Address.\n";
        closeSocket(sock);
        cleanNetwork();
        return false;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to " << ip << ":" << port << "\n";
        closeSocket(sock);
        cleanNetwork();
        return false;
    }

    int bytesSent = send(sock, message.c_str(), static_cast<int>(message.length()), 0);
    if (bytesSent < 0) {
        std::cerr << "Send failed.\n";
        closeSocket(sock);
        cleanNetwork();
        return false;
    }

    closeSocket(sock);
    cleanNetwork();
    return true;
}

inline bool startServer(int port, std::function<void(const std::string&)> messageHandler) {
        if (!initNetwork()) return false;

        SocketType server_sock = socket(AF_INET, SOCK_STREAM, 0);
#if defined(_WIN32) || defined(_WIN64)
        if (server_sock == INVALID_SOCKET)
#else
        if (server_sock < 0)
#endif
        {
            cleanNetwork();
            return false;
        }

        int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Server: Bind failed on port " << port << "\n";
            closeSocket(server_sock); cleanNetwork(); return false;
        }

        if (listen(server_sock, 5) < 0) {
            std::cerr << "Server: Listen failed\n";
            closeSocket(server_sock); cleanNetwork(); return false;
        }

        std::cout << "Server successfully started on port " << port << ". Waiting for messages...\n\n";

        char buffer[1024];
        while (true) {
            SocketType client_sock = accept(server_sock, nullptr, nullptr);
#if defined(_WIN32) || defined(_WIN64)
            if (client_sock == INVALID_SOCKET) continue;
#else
            if (client_sock < 0) continue;
#endif

            std::memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

            if (bytesReceived > 0) {
                std::string receivedMsg(buffer, bytesReceived);

                // --- THIS IS WHERE YOU GET THE MESSAGE ---
                // It triggers the custom logic you define in your main file
                messageHandler(receivedMsg);
            }

            closeSocket(client_sock);
        }

        closeSocket(server_sock);
        cleanNetwork();
        return true;
    }

#endif
