from pathlib import Path

Import("env")


def replace_first(path, old_variants, new):
    if not path.exists():
        return

    text = path.read_text(encoding="utf-8")
    if new in text:
        return

    for old in old_variants:
        if old in text:
            path.write_text(text.replace(old, new, 1), encoding="utf-8")
            print(f"[patch_esp_reverse_tunnel] Patched {path}")
            return

    print(f"[patch_esp_reverse_tunnel] Pattern not found in {path}")


def ensure_include(path, include_line):
    if not path.exists():
        return

    text = path.read_text(encoding="utf-8")
    if include_line in text:
        return

    marker = "#include <netinet/in.h>\n"
    if marker not in text:
        print(f"[patch_esp_reverse_tunnel] Include marker not found in {path}")
        return

    path.write_text(text.replace(marker, marker + include_line + "\n", 1),
                    encoding="utf-8")
    print(f"[patch_esp_reverse_tunnel] Added include to {path}")


def ensure_include_after(path, marker, include_line):
    if not path.exists():
        return

    text = path.read_text(encoding="utf-8")
    if include_line in text:
        return

    if marker not in text:
        print(f"[patch_esp_reverse_tunnel] Include marker not found in {path}")
        return

    path.write_text(text.replace(marker, marker + include_line + "\n", 1),
                    encoding="utf-8")
    print(f"[patch_esp_reverse_tunnel] Added include to {path}")


libdeps_root = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
pioenv = env.subst("$PIOENV")
libdeps = libdeps_root / pioenv
if not libdeps.exists():
    libdeps = libdeps_root

ssh_session = libdeps / "ESP-Reverse_Tunneling_Libssh2" / "src" / "ssh_session.cpp"
replace_first(
    ssh_session,
    [
        """  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  struct hostent *he = gethostbyname(sshConfig.host.c_str());
  if (he == nullptr) {
    LOGF_E("SSH", "Invalid remote hostname: %s", sshConfig.host.c_str());
    close(socketfd_);
    socketfd_ = -1;
    return false;
  }
  memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);
  sin.sin_port = htons(sshConfig.port);
""",
        """  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  struct hostent *he = gethostbyname(sshConfig.host.c_str());
  if (he == nullptr) {
    LOGF_E("SSH", "Invalid remote hostname: %s", sshConfig.host.c_str());
    close(socketfd_);
    socketfd_ = -1;
    return false;
  }
  if (he->h_addr_list[0] == nullptr || he->h_length != sizeof(sin.sin_addr)) {
    LOGF_E("SSH", "Invalid IPv4 address length for %s: %d",
           sshConfig.host.c_str(), he->h_length);
    close(socketfd_);
    socketfd_ = -1;
    return false;
  }
  memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));
  sin.sin_port = htons(sshConfig.port);
""",
    ],
    """  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(sshConfig.port);

  if (inet_pton(AF_INET, sshConfig.host.c_str(), &sin.sin_addr) != 1) {
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    int rc = getaddrinfo(sshConfig.host.c_str(), nullptr, &hints, &result);
    if (rc != 0 || result == nullptr || result->ai_addr == nullptr) {
      LOGF_E("SSH", "Invalid remote hostname: %s", sshConfig.host.c_str());
      if (result != nullptr) {
        freeaddrinfo(result);
      }
      close(socketfd_);
      socketfd_ = -1;
      return false;
    }

    auto *resolved = reinterpret_cast<struct sockaddr_in *>(result->ai_addr);
    sin.sin_addr = resolved->sin_addr;
    freeaddrinfo(result);
  }
""",
)

libssh2_esp = libdeps / "libssh2_esp" / "src" / "libssh2_esp.c"
ensure_include(libssh2_esp, "#include <arpa/inet.h>")
replace_first(
    libssh2_esp,
    [
        """    // Setup address structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);
""",
        """    // Setup address structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (!he->h_addr_list[0] || he->h_length != sizeof(sin.sin_addr)) {
        LIBSSH2_ESP_ERROR("Invalid IPv4 address length for %s: %d", hostname, he->h_length);
        close(sock);
        return -1;
    }
    memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));
""",
    ],
    """    // Setup address structure
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
""",
)

mbedtls_backend = libdeps / "libssh2_esp" / "src" / "mbedtls.c"
ensure_include_after(mbedtls_backend, "#include <stdlib.h>\n", "#include <stdio.h>")
replace_first(
    mbedtls_backend,
    [
        """    memcpy(ntdata, data, data_len);

    if(_libssh2_mbedtls_parse_eckey(ctx, &pkey, session,
                                    ntdata, data_len + 1, pwd) == 0)
""",
    ],
    """    memcpy(ntdata, data, data_len);
    ntdata[data_len] = '\\0';

    if(_libssh2_mbedtls_parse_eckey(ctx, &pkey, session,
                                    ntdata, data_len + 1, pwd) == 0)
""",
)
replace_first(
    mbedtls_backend,
    [
        """    size_t r_len, s_len, tmp_sign_len = 0;
    unsigned char *sp, *tmp_sign = NULL;
    mbedtls_mpi pr, ps;
""",
    ],
    """    size_t r_len, s_len, tmp_sign_len = 0;
    unsigned char *sp, *tmp_sign = NULL;
    mbedtls_mpi pr, ps;
    int ret;
""",
)
replace_first(
    mbedtls_backend,
    [
        """    if(mbedtls_ecdsa_sign(&ctx->MBEDTLS_PRIVATE(grp), &pr, &ps,
                          &ctx->MBEDTLS_PRIVATE(d),
                          hash, hash_len,
                          mbedtls_ctr_drbg_random,
                          &_libssh2_mbedtls_ctr_drbg))
        goto cleanup;
""",
    ],
    """    ret = mbedtls_ecdsa_sign(&ctx->MBEDTLS_PRIVATE(grp), &pr, &ps,
                              &ctx->MBEDTLS_PRIVATE(d),
                              hash, hash_len,
                              mbedtls_ctr_drbg_random,
                              &_libssh2_mbedtls_ctr_drbg);
    if(ret) {
        printf("[libssh2_esp ERROR] mbedtls_ecdsa_sign failed: -0x%04x (%d)\\n",
               ret < 0 ? -ret : ret, ret);
        goto cleanup;
    }
""",
)
