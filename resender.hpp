#pragma once
#include <cstdint>
#include <qrencode.h>

void chunk_qr(QRcode*& qr, uint32_t count);
void init();


#pragma pack(push, 1)
struct ChunkHeader
{
    uint32_t chunk;
    uint32_t size;
    uint32_t offset;
    uint32_t crc;
};

struct MasterQRCode
{
    // qrcode info
    int32_t magic;
    int32_t protocol_version;       // version of qrcode-network
    int32_t version;
    int32_t correction;

    uint32_t flags;


    // data info
    uint32_t chunk_count;
    uint32_t chunk_size;
    uint64_t file_size;

    uint8_t sha256[32];
    uint16_t filename_size;
    char filename[256];
};
#pragma pack(pop)

SDL_Texture* QRToTexture(const QRcode* qr, int scale);
uint8_t* alloc_chunk(const char* filename, uint64_t offset, uint32_t chunk_size);
void delete_chunk(const uint8_t* buffer);