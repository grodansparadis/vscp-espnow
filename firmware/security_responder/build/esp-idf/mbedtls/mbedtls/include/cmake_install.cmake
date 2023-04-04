# Install script for directory: /usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/akhe/.espressif/tools/xtensa-esp32-elf/esp-2022r1-11.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "/usr/local/src/esp/esp-idf-v5.0.1/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

