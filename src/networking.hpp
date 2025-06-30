// cross-platform networking lib (based on unix socket interface)
#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include <cstdint>
#include <string>
#include <iostream>

#if __unix__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#elif _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define NOGDI     // prevents wingdi.h from being included (Rectangle conflict)
#define NOUSER    // prevents winuser.h from being included (DrawText, CloseWindow conflicts)
#define NOMCX     // prevents mmsystem.h from being included (PlaySound conflict)
#include <winsock2.h>
#include <ws2tcpip.h> 
#pragma comment(lib, "ws2_32.lib")
#endif

#if _WIN32
#define GET_LAST_ERROR WSAGetLastError()
#define SOCKET_ERROR_VAL SOCKET_ERROR

// windows-specific error reporting function
inline void print_socket_error(const char* message) {
    int error_code = WSAGetLastError();
    std::cerr << message << ": WSA Error " << error_code;
    
=    switch(error_code) {
        case WSAECONNREFUSED:
            std::cerr << " (Connection refused)";
            break;
        case WSAENETUNREACH:
            std::cerr << " (Network unreachable)";
            break;
        case WSAETIMEDOUT:
            std::cerr << " (Connection timed out)";
            break;
        case WSAEHOSTUNREACH:
            std::cerr << " (Host unreachable)";
            break;
        case WSAEADDRINUSE:
            std::cerr << " (Address already in use)";
            break;
        case WSAEADDRNOTAVAIL:
            std::cerr << " (Address not available)";
            break;
        default:
            std::cerr << "Reached WSA error code " << error_code << " (unknown error)";
            break;
    }
    std::cerr << std::endl;
}
#else
#define GET_LAST_ERROR errno
#define SOCKET_ERROR_VAL -1

inline void print_socket_error(const char* message) {
    perror(message);
}
#endif

// sock init
inline int create_socket(int domain, int type, int protocol) {
#if __unix__
    return socket(domain, type, protocol);
#elif _WIN32
    return (int)WSASocket(domain, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
    return -1; // what is bro running this on
#endif
}

// bind socket
inline int bind_socket(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
#if __unix__
    return bind(sockfd, addr, addrlen);
#elif _WIN32
    return bind((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// listen for connections
inline int listen_socket(int sockfd, int backlog) {
#if __unix__
    return listen(sockfd, backlog);
#elif _WIN32
    return listen((SOCKET)sockfd, backlog);
#else
    return -1;
#endif
}

// accept connection
inline int accept_connection(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
#if __unix__
    return accept(sockfd, addr, addrlen);
#elif _WIN32
    return (int)accept((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// connect to server
inline int connect_socket(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
#if __unix__
    return connect(sockfd, addr, addrlen);
#elif _WIN32
    return connect((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// send data
inline int send_data(int sockfd, const void *buf, size_t len, int flags) {
#if __unix__
    return send(sockfd, buf, len, flags);
#elif _WIN32
    return send((SOCKET)sockfd, (const char*)buf, (int)len, flags);
#else
    return -1;
#endif
}

// receive data
inline int recv_data(int sockfd, void *buf, size_t len, int flags) {
#if __unix__
    return recv(sockfd, buf, len, flags);
#elif _WIN32
    return recv((SOCKET)sockfd, (char*)buf, (int)len, flags);
#else
    return -1;
#endif
}

// shutdown socket
inline int shutdown_socket(int sockfd, int how) {
#if __unix__
    return shutdown(sockfd, how);
#elif _WIN32
    return shutdown((SOCKET)sockfd, how);
#else
    return -1;
#endif
}

// close socket
inline int close_socket(int sockfd) {
#if __unix__
    return close(sockfd);
#elif _WIN32
    return closesocket((SOCKET)sockfd);
#else
    return -1;
#endif
}

// set socket options
inline int set_socket_option(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
#if __unix__
    return setsockopt(sockfd, level, optname, optval, optlen);
#elif _WIN32
    return setsockopt((SOCKET)sockfd, level, optname, (const char*)optval, (int)optlen);
#else
    return -1;
#endif
}

// convert ip address
inline unsigned long inet_address(const char *cp) {
#if __unix__
    return inet_addr(cp);
#elif _WIN32
    return inet_addr(cp);
#else
    return INADDR_NONE;
#endif
}

// convert host to network short
inline uint16_t host_to_network_short(uint16_t hostshort) {
#if __unix__ || _WIN32
    return htons(hostshort);
#else
    return hostshort;
#endif
}

// convert network to host short
inline uint16_t network_to_host_short(uint16_t netshort) {
#if __unix__ || _WIN32
    return ntohs(netshort);
#else
    return netshort;
#endif
}

// convert host to network long
inline uint32_t host_to_network_long(uint32_t hostlong) {
#if __unix__ || _WIN32
    return htonl(hostlong);
#else
    return hostlong;
#endif
}

// convert network to host long
inline uint32_t network_to_host_long(uint32_t netlong) {
#if __unix__ || _WIN32
    return ntohl(netlong);
#else
    return netlong;
#endif
}

// convert ip string to binary
inline uint32_t ip_string_to_binary(const char* ipStr) {
#if defined(_WIN32)
    in_addr addr;
    // Windows Vista+ supports inet_pton
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        return addr.s_addr;
    } else {
        // Fallback to inet_addr (returns INADDR_NONE on failure)
        return inet_addr(ipStr);
    }
#else
    in_addr addr;
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        return addr.s_addr;
    } else {
        return INADDR_NONE;
    }
#endif
}

// convert ip binary to string
inline std::string ip_binary_to_string(uint32_t ip) {
    char buf[INET_ADDRSTRLEN];
    const char* result = inet_ntop(AF_INET, &ip, buf, INET_ADDRSTRLEN);
    return result ? std::string(result) : std::string();
}


// socket address structures
#if __unix__ || _WIN32
using socket_address_in = sockaddr_in;
using socket_address = sockaddr;
#endif

// constants
#if __unix__
constexpr int SOCKET_STREAM = SOCK_STREAM;
constexpr int SOCKET_DGRAM = SOCK_DGRAM;
constexpr int ADDRESS_FAMILY_INET = AF_INET;
constexpr int ADDRESS_FAMILY_INET6 = AF_INET6;
constexpr int SOCKET_LEVEL = SOL_SOCKET;
constexpr int SOCKET_REUSEADDR = SO_REUSEADDR;
constexpr int SOCKET_REUSEPORT = SO_REUSEPORT; 
constexpr int SHUTDOWN_READ = SHUT_RD;
constexpr int SHUTDOWN_WRITE = SHUT_WR;
constexpr int SHUTDOWN_BOTH = SHUT_RDWR;
constexpr uint32_t ADDRESS_ANY = INADDR_ANY;
constexpr uint32_t ADDRESS_NONE = INADDR_NONE;
#elif _WIN32
constexpr int SOCKET_STREAM = SOCK_STREAM;
constexpr int SOCKET_DGRAM = SOCK_DGRAM;
constexpr int ADDRESS_FAMILY_INET = AF_INET;
constexpr int ADDRESS_FAMILY_INET6 = AF_INET6;
constexpr int SOCKET_LEVEL = SOL_SOCKET;
constexpr int SOCKET_REUSEADDR = SO_REUSEADDR;
constexpr int SOCKET_REUSEPORT = 0; // winsock doesn't have SO_REUSEPORT on individual sockets
constexpr int SHUTDOWN_READ = SD_RECEIVE;
constexpr int SHUTDOWN_WRITE = SD_SEND;
constexpr int SHUTDOWN_BOTH = SD_BOTH;
constexpr uint32_t ADDRESS_ANY = INADDR_ANY;
constexpr uint32_t ADDRESS_NONE = INADDR_NONE;
#else
// generic fallbacks if neither unix nor windows is detected
constexpr int SOCKET_STREAM = 1;
constexpr int SOCKET_DGRAM = 2;
constexpr int ADDRESS_FAMILY_INET = 2;
constexpr int ADDRESS_FAMILY_INET6 = 10;
constexpr int SOCKET_LEVEL = 1;
constexpr int SOCKET_REUSEADDR = 2;
constexpr int SOCKET_REUSEPORT = 15;
constexpr int SHUTDOWN_READ = 0;
constexpr int SHUTDOWN_WRITE = 1;
constexpr int SHUTDOWN_BOTH = 2;
constexpr uint32_t ADDRESS_ANY = 0;
constexpr uint32_t ADDRESS_NONE = 0xFFFFFFFF;
#endif

#ifdef _WIN32
// winsock init and cleanup
inline int initialize_winsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

inline int cleanup_winsock() {
    return WSACleanup();
}
#endif

#endif // NETWORKING_HPP