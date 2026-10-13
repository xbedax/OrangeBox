/**
 * @file libssh2_esp.c
 * @brief libssh2 ESP32 wrapper implementation with framework abstraction
 *
 * @author playmiel
 * @license BSD-3-Clause
 */

#include "libssh2_esp.h"
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

// Global initialization flag
static bool libssh2_esp_initialized = false;

int libssh2_esp_init(void) {
    if (libssh2_esp_initialized) {
        LIBSSH2_ESP_LOG("libssh2_esp already initialized");
        return 0;
    }
    
    LIBSSH2_ESP_LOG("Initializing libssh2_esp v%s on %s framework", 
                    LIBSSH2_ESP_VERSION, LIBSSH2_ESP_FRAMEWORK);
    
    // Initialize libssh2
    int rc = libssh2_init(0);
    if (rc != 0) {
        LIBSSH2_ESP_ERROR("Failed to initialize libssh2: %d", rc);
        return rc;
    }
    
    libssh2_esp_initialized = true;
    LIBSSH2_ESP_LOG("libssh2_esp initialized successfully");
    return 0;
}

void libssh2_esp_cleanup(void) {
    if (!libssh2_esp_initialized) {
        return;
    }
    
    LIBSSH2_ESP_LOG("Cleaning up libssh2_esp");
    libssh2_exit();
    libssh2_esp_initialized = false;
}

const char* libssh2_esp_get_framework(void) {
    return LIBSSH2_ESP_FRAMEWORK;
}

int libssh2_esp_socket_connect(const char* hostname, int port) {
    if (!hostname || port <= 0) {
        LIBSSH2_ESP_ERROR("Invalid parameters: hostname=%p, port=%d", hostname, port);
        return -1;
    }
    
    LIBSSH2_ESP_LOG("Connecting to %s:%d", hostname, port);
    
    int sock = -1;
    struct sockaddr_in sin;
    struct hostent *he;
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LIBSSH2_ESP_ERROR("Failed to create socket: %d", errno);
        return -1;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = LIBSSH2_ESP_CONNECT_TIMEOUT_MS / 1000;
    timeout.tv_usec = (LIBSSH2_ESP_CONNECT_TIMEOUT_MS % 1000) * 1000;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LIBSSH2_ESP_ERROR("Failed to set receive timeout");
    }
    
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LIBSSH2_ESP_ERROR("Failed to set send timeout");
    }
    
    // Resolve hostname
    he = gethostbyname(hostname);
    if (!he) {
        LIBSSH2_ESP_ERROR("Failed to resolve hostname: %s", hostname);
        close(sock);
        return -1;
    }
    
    // Setup address structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (inet_pton(AF_INET, hostname, &sin.sin_addr) != 1) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *result = NULL;
        int rc = getaddrinfo(hostname, NULL, &hints, &result);
        if (rc != 0 || result == NULL || result->ai_addr == NULL) {
            LIBSSH2_ESP_ERROR("Failed to resolve hostname: %s", hostname);
            if (result != NULL) {
                freeaddrinfo(result);
            }
            close(sock);
            return -1;
        }

        sin.sin_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr;
        freeaddrinfo(result);
    }
    
    // Connect
    if (connect(sock, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
        LIBSSH2_ESP_ERROR("Failed to connect to %s:%d - %d", hostname, port, errno);
        close(sock);
        return -1;
    }
    
    LIBSSH2_ESP_LOG("Successfully connected to %s:%d (socket %d)", hostname, port, sock);
    return sock;
}

void libssh2_esp_socket_close(int sock) {
    if (sock >= 0) {
        LIBSSH2_ESP_LOG("Closing socket %d", sock);
        close(sock);
    }
}
