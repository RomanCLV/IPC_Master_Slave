#pragma once
#include <stdint.h>

#ifndef EXPECTED_SHARED_DATA_SIZE
#define EXPECTED_SHARED_DATA_SIZE 548
#endif // !EXPECTED_SHARED_DATA_SIZE

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
    char resultFilePath[256];       // 256 bytes    Offset: 280
    int32_t codeResult;             // 4 bytes      Offset: 536
    int32_t sumResult;              // 4 bytes      Offset: 540

    uint32_t flags;                 // 4 bytes      Offset: 544

    // TOTAL                         548 bytes
};
#pragma pack(pop)
