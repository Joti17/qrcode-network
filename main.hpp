#pragma once
#include "stdint.h"
#include <fstream>
#include <iostream>
#include <qrencode.h>
#include <vector>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif


void init();
void next(QRcode*& qr);

SDL_Texture* QRToTexture(const QRcode* qr, int scale);


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
    int32_t protocol_version; // version of qrcode-network
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

std::vector<uint8_t> make_packet(const ChunkHeader& head, const uint8_t* payload);
void delete_chunk(const uint8_t* buffer);
uint8_t* alloc_chunk(const char* filename, uint64_t offset, uint32_t chunk_size);

void start_resender(std::string location)
{
    std::string program_name = "resender";


    std::cout << "Starting resender" << std::endl;

#ifdef _WIN32

    std::string command = program_name + ".exe \"" + location + "\"";

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::vector<char> cmd(command.begin(), command.end());
    cmd.push_back('\0');

    if (!CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    ))
    {
        std::cerr << "Failed starting resender: "
            << GetLastError()
            << std::endl;
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

#else

    pid_t pid = fork();

    if (pid == 0)
    {
        execl(
            ("./" + program_name).c_str(),
            program_name.c_str(),
            location.c_str(),
            nullptr
        );

        _exit(1);
    }

#endif
}
