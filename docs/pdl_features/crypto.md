# Crypto (MXCRYPTO) - Full-Featured Hardware Cryptography Engine for Embedded C Applications

## Overview

The **Crypto** driver provides a hardware-accelerated cryptographic engine (MXCRYPTO IP) offering symmetric encryption, asymmetric cryptography, hashing, message authentication, and random number generation through a unified PDL API. It offloads CPU-intensive security operations to dedicated hardware, enabling IoT, industrial, and connected-device applications to meet security standards such as FIPS 197, FIPS 180-4, and NIST SP 800-90A with minimal software overhead.

---

## Features

- **Symmetric encryption** — AES-128/192/256 in ECB, CBC, CFB, CTR, OFB, XTS modes; DES and Triple-DES (3DES/TDES)
- **Hash and MAC** — SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, HMAC, CMAC-AES, Poly1305
- **Asymmetric cryptography** — RSA (up to 4096-bit), ECC key generation, ECDSA sign/verify, EdDSA, EC25519 (X25519/Ed25519), ECDH
- **Random number generation** — True RNG (TRNG) based on ring oscillators (NIST SP 800-90A compliant) and Pseudo RNG (PRNG, LFSR-based)
- **Additional primitives** — CRC (configurable polynomial), ChaCha20 stream cipher, HKDF key derivation, memory utilities (XOR, CMP, SET)
- **IPC-based client/server architecture** — Crypto hardware shared across multiple CPU cores via inter-processor communication (IPC), with optional async completion callbacks

---

## When to Use

- Encrypt/decrypt firmware update packages or data-at-rest stored in external flash
- Authenticate TLS/DTLS sessions by computing SHA digests and verifying RSA or ECDSA signatures
- Derive unique device session keys using HKDF or ECDH key exchange
- Generate true random seeds for TLS nonces, UUIDs, or challenge-response protocols
- Implement CMAC or HMAC message integrity checks in industrial or automotive protocols
- Compute CRC checksums over data buffers for data integrity verification

---

## Prerequisites

### Hardware Requirements

- **Device:** Must have MXCRYPTO IP block (`CY_IP_MXCRYPTO`)
- **Clock:** The Crypto block operates from the high-frequency clock (HF); ensure it is enabled and configured
- **IPC Channels:** The driver uses two IPC channels (default: channel 9 for data, interrupt structures 1 and 2); ensure no conflicts with other IPC users

### Software Requirements

- PDL version ≥ 2.150 (included with ModusToolbox™ 3.x)
- `cy_pdl.h` or individual headers `cy_crypto.h`, `cy_crypto_core.h`
- The Crypto server must be started on the CM0+ core (or the core that owns the hardware); the client runs on any core

### Configure in the Tool

The Crypto driver does not have a Device Configurator personality — it is configured entirely in firmware. To enable the driver:

1. Include `cy_pdl.h` in your source file
2. Declare the `cy_stc_crypto_config_t` structure with your IPC channel and interrupt assignments
3. On the CM0+ (server) core call `Cy_Crypto_Server_Start()` before any crypto operations
4. On the application (client) core call `Cy_Crypto_Init()` with a matching configuration

> **Note:** If using direct LLD (Low-Level Driver) APIs (`Cy_Crypto_Core_*`), no IPC setup is required — call LLD functions directly on the core that owns the Crypto hardware.

---

## Quick Start

This quick start demonstrates AES-128-CBC encryption and SHA-256 hashing using the bare-metal LLD API on a single core.

**Step 1:** Include the PDL header.

**Step 2:** Declare required context structures.

**Step 3:** Initialize the Crypto core and perform an AES-CBC encrypt followed by SHA-256 hash.

**Step 4:** Add the following code to your `main.c` (see [Sample Code](#sample-code) for complete example).

**Expected Outcome:** The AES-CBC ciphertext is computed from the plaintext and stored in `cipherOut`. The SHA-256 digest of the message is stored in `digest`. No assertion fires if vectors match expected values.

### Sample Code

```c
#include "cy_pdl.h"
#include <string.h>

/* AES-128 key (16 bytes) */
CY_ALIGN(4) static const uint8_t aesKey[CY_CRYPTO_AES_128_KEY_SIZE] = {
    0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
};
/* AES-CBC initialization vector (16 bytes) */
CY_ALIGN(4) static const uint8_t aesIv[CY_CRYPTO_AES_BLOCK_SIZE] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};
/* Plaintext (32 bytes) */
CY_ALIGN(4) static const uint8_t plainText[32] = {
    0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
    0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
    0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,
    0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51
};

/* Output buffers */
CY_ALIGN(4) static uint8_t cipherOut[32];
CY_ALIGN(4) static uint8_t digest[CY_CRYPTO_SHA256_DIGEST_SIZE];

/* Crypto contexts */
static cy_stc_crypto_context_aes_t aesCtx;
static cy_stc_crypto_context_sha_t shaCtx;

int main(void)
{
    cy_en_crypto_status_t status;

    /* --- AES-128-CBC Encryption (LLD) --- */
    cy_stc_crypto_aes_state_t aesState;

    status = Cy_Crypto_Core_Aes_Init(CRYPTO, aesKey, CY_CRYPTO_KEY_AES_128, &aesState);
    CY_ASSERT(CY_CRYPTO_SUCCESS == status);

    status = Cy_Crypto_Core_Aes_Cbc(CRYPTO, CY_CRYPTO_ENCRYPT, 32U,
                                     (uint8_t *)aesIv, cipherOut, plainText, &aesState);
    CY_ASSERT(CY_CRYPTO_SUCCESS == status);

    Cy_Crypto_Core_Aes_Free(CRYPTO, &aesState);

    /* --- SHA-256 Hash (LLD) --- */
    status = Cy_Crypto_Core_Sha(CRYPTO, plainText, sizeof(plainText),
                                 digest, CY_CRYPTO_MODE_SHA256);
    CY_ASSERT(CY_CRYPTO_SUCCESS == status);

    for (;;) { /* done */ }
}
```

### Expected Outcome

- `cipherOut` contains the AES-128-CBC ciphertext of `plainText`
- `digest` contains the 32-byte SHA-256 hash of `plainText`
- `CY_ASSERT` does not trigger — both operations return `CY_CRYPTO_SUCCESS`

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `Cy_Crypto_Init()` returns error | IPC channel conflict or server not started | Verify `Cy_Crypto_Server_Start()` is called on the CM0+ core before `Cy_Crypto_Init()` on the CM4/CM7 |
| Hard fault on Crypto register access | Device does not have MXCRYPTO IP | Confirm the device has `CY_IP_MXCRYPTO` support; use Cryptolite driver if the device has MXCRYPTOLITE instead |
| `CY_CRYPTO_HW_BUSY` returned | Another core is using the Crypto block | Add retry logic or use the async callback mechanism |
| AES output does not match expected | Buffer not 4-byte aligned | Use `CY_ALIGN(4)` on all input/output buffers; for CM7 with D-cache use `CY_ALIGN(32)` |
| CM7 cache coherency issues | D-cache stale data for DMA/crypto buffers | Declare Crypto buffers with `CY_ALIGN(32)` and call `SCB_CleanDCache_by_Addr()` before Crypto operations |
| `Cy_Crypto_Core_Rsa_Verify()` fails | Public key not aligned or wrong key format | Ensure public key modulus and exponent are in the `cy_stc_crypto_rsa_pub_key_t` structure with correct `moduloLength` |

---

## Related Code Examples

- [PSOC™ Edge MCU: Crypto AES](https://github.com/Infineon/mtb-example-psoc-edge-crypto-aes)
- [PSOC™ Edge MCU: Crypto SHA](https://github.com/Infineon/mtb-example-psoc-edge-crypto-sha)
- [PSOC™ Edge MCU: Mbed TLS PSA Crypto](https://github.com/Infineon/mtb-example-psoc-edge-mbedtls-psa-crypto)

## Related Application Notes

- [NIST SP 800-90A Rev 1 — TRNG recommendations](https://csrc.nist.gov/publications/detail/sp/800-90a/rev-1/final)

---

## Configuration Parameters Reference

| Parameter / API | Type | Default | Description |
|----------------|------|---------|-------------|
| `ipcChannel` | `uint32_t` | 9 | IPC channel number used for Crypto server/client communication |
| `acquireNotifierChannel` | `uint32_t` | 1 | IPC interrupt structure index for server acquire notifications |
| `releaseNotifierChannel` | `uint32_t` | 2 | IPC interrupt structure index for client release notifications |
| `userCompleteCallback` | function ptr | NULL | Optional callback invoked when an async Crypto operation completes |
| `userGetDataHandler` | function ptr | NULL | Optional handler called when Crypto server receives a new request |
| `userErrorHandler` | function ptr | NULL | Optional handler called on hardware error detection |
| `CY_CRYPTO_KEY_AES_128` | enum | — | AES key length: 128-bit |
| `CY_CRYPTO_KEY_AES_192` | enum | — | AES key length: 192-bit |
| `CY_CRYPTO_KEY_AES_256` | enum | — | AES key length: 256-bit |
| `CY_CRYPTO_MODE_SHA256` | enum | — | SHA-256 (32-byte digest) hash mode |
| `CY_CRYPTO_MODE_SHA512` | enum | — | SHA-512 (64-byte digest) hash mode |
| `CY_CRYPTO_ENCRYPT` / `CY_CRYPTO_DECRYPT` | enum | — | Direction selector for symmetric cipher operations |
| `CY_CRYPTO_USER_CONFIG_FILE` | macro | (all enabled) | Define to a header filename to selectively enable/disable algorithm modules |

### Key API Summary

| API | Description |
|-----|-------------|
| `Cy_Crypto_Core_Aes_Init()` | Initialize AES context with key and key length |
| `Cy_Crypto_Core_Aes_Cbc()` | AES-CBC encrypt or decrypt |
| `Cy_Crypto_Core_Aes_Ecb()` | AES-ECB encrypt or decrypt (single block) |
| `Cy_Crypto_Core_Aes_Ctr()` | AES-CTR mode streaming cipher |
| `Cy_Crypto_Core_Aes_Free()` | Release AES hardware resources |
| `Cy_Crypto_Core_Sha()` | One-shot SHA hash |
| `Cy_Crypto_Core_Hmac()` | HMAC-SHA message authentication |
| `Cy_Crypto_Core_Cmac()` | CMAC-AES message authentication |
| `Cy_Crypto_Core_Trng()` | Generate true random number |
| `Cy_Crypto_Core_Prng_Init()` | Initialize PRNG with LFSR seeds |
| `Cy_Crypto_Core_Prng()` | Generate pseudo random number |
| `Cy_Crypto_Core_Des()` | DES encrypt/decrypt |
| `Cy_Crypto_Core_Tdes()` | 3DES (TDES) encrypt/decrypt |
| `Cy_Crypto_Core_Rsa_Verify()` | Verify RSA PKCS#1 signature |
| `Cy_Crypto_Core_ECC_MakeKeyPair()` | Generate ECC key pair |
| `Cy_Crypto_Core_ECC_SignHash()` | ECDSA sign a hash |
| `Cy_Crypto_Core_ECC_VerifyHash()` | ECDSA verify a signature |
| `Cy_Crypto_Core_Crc()` | Compute CRC with configurable polynomial |
| `Cy_Crypto_Core_Hkdf()` | HKDF key derivation |
| `Cy_Crypto_Server_Start()` | Start the Crypto IPC server (CM0+ core) |
| `Cy_Crypto_Init()` | Initialize Crypto IPC client (application core) |

---

## Advanced Usage

### Multi-Core IPC Mode

On dual-core devices (CM0+ + CM4/CM7), the Crypto hardware is arbitrated through IPC:

1. CM0+ calls `Cy_Crypto_Server_Start(&cryptoConfig, &serverContext)` — this configures the IPC interrupt and enters a waiting loop
2. CM4/CM7 calls `Cy_Crypto_Init(&cryptoConfig, &cryptoContext)` — establishes the client connection
3. All subsequent `Cy_Crypto_*` (non-Core) API calls are routed through IPC automatically
4. Pass a `userCompleteCallback` for non-blocking async operation

### D-Cache Alignment (CM7)

When the CM7 data cache is enabled, all Crypto input/output buffers must be:
- Aligned to the cache line boundary: `CY_ALIGN(32)` or `CY_ALIGN(__SCB_DCACHE_LINE_SIZE)`
- Cleaned/invalidated before/after Crypto operations to avoid stale cache data corrupting results

### Selective Feature Compilation

Define `CY_CRYPTO_USER_CONFIG_FILE` to point to a custom header that enables only the required algorithms, reducing code size:

```c
// crypto_user_config.h
#define CY_CRYPTO_CFG_AES_C     1
#define CY_CRYPTO_CFG_SHA_C     1
#define CY_CRYPTO_CFG_HMAC_C    1
#define CY_CRYPTO_CFG_TRNG_C    1
// leave all others undefined (disabled)
```

### ECC Curve Selection

Supported NIST curves: P-192, P-224, P-256, P-384, P-521. Curve parameters are selected via the `cy_en_crypto_ecc_curve_id_t` enum (e.g., `CY_CRYPTO_ECC_ECP_SECP256R1`). Curve domain parameters are stored in ROM — no user configuration needed.

---

## Industry Standards

| Standard | Description |
|----------|-------------|
| **FIPS 197** | Advanced Encryption Standard (AES) — AES-128/192/256 |
| **FIPS 46-3** | Data Encryption Standard / Triple-DES (DES / 3DES) |
| **FIPS 180-4** | Secure Hash Standard (SHA-1, SHA-224, SHA-256, SHA-384, SHA-512) |
| **FIPS 198-1** | HMAC — The Keyed-Hash Message Authentication Code |
| **NIST SP 800-38B** | CMAC — Recommendation for Block Cipher Modes of Operation |
| **NIST SP 800-90A Rev 1** | TRNG — Recommendation for Random Number Generation |
| **NIST SP 800-56A** | ECDH — Key Agreement using ECC |
| **RFC 5869** | HMAC-based Key Derivation Function (HKDF) |
| **PKCS#1 v2.1** | RSA signature schemes (RSASSA-PKCS1-v1_5, RSASSA-PSS) |

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
