#pragma once
#include <stdint.h>

#ifndef EXPECTED_SHARED_DATA_SIZE
#define EXPECTED_SHARED_DATA_SIZE 548
#endif // !EXPECTED_SHARED_DATA_SIZE

namespace IPCFlags
{
    constexpr uint32_t IDLE = 0x0;           // État initial, au repos
    constexpr uint32_t MASTER_READY = 0x1;   // Master a écrit les inputs, slave peut commencer
    constexpr uint32_t SLAVE_STARTED = 0x2;  // Slave a lu les inputs et commence le traitement
    constexpr uint32_t SLAVE_FINISHED = 0x4; // Slave a terminé et écrit les outputs
}

// Codes d'erreur
namespace IPCErrorCode
{
    constexpr int32_t SUCCESS = 0;
    constexpr int32_t START_GREATER_THAN_END = 1;
    constexpr int32_t OVERFLOW_ERROR = 2;
    constexpr int32_t FILE_WRITE_ERROR = 3;
    constexpr int32_t INVALID_RESPONSE_COUNTER = 98;
    constexpr int32_t UNKNOWN_ERROR = 99;
}

#pragma pack(push, 1)
// supprime tout padding
// garantit structure identique entre compilateurs

struct SharedData
{
    // ### CHAMP ###   ### TAILLE ###   ### OFFSET ###

    // Pour vérifier que le mapping est correct, lire une mémoire corrompue
    uint32_t magic = 0xDEADBEEF;    // 4 bytes      Offset: 0
    uint32_t version = 1;           // 4 bytes      Offset: 4

    // Inputs
    char resultsFolderPath[256];    // 256 bytes    Offset: 8
    int32_t startNumber;            // 4 bytes      Offset: 264
    int32_t endNumber;              // 4 bytes      Offset: 268

    // Sécurité pour synchro req/res
    uint32_t requestCounter;        // 4 bytes      Offset: 272
    uint32_t responseCounter;       // 4 bytes      Offset: 276

    // Outputs
    char resultFileName[256];       // 256 bytes    Offset: 280
    int32_t codeResult;             // 4 bytes      Offset: 536
    int32_t sumResult;              // 4 bytes      Offset: 540

    uint32_t flags;                 // 4 bytes      Offset: 544

    // TOTAL                         548 bytes
};
#pragma pack(pop)

static_assert(sizeof(SharedData) == EXPECTED_SHARED_DATA_SIZE, "SharedData size mismatch");
