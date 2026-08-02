#include "stdinclude.hpp"
#include "networking.hpp"
#include <cstring>
#include <filesystem>
#include <SDL3/SDL.h>
#include <thread>
#include "resender.hpp"
#include "frames.hpp"



constexpr int32_t SCALE = 4;
constexpr int32_t QRCODE_WIDTH = 177;
constexpr int32_t SCALED_WIDTH = SCALE * QRCODE_WIDTH;
constexpr int32_t BORDER = 4;

std::string filepath;
uint64_t file_size;
MasterQRCode master_qr;

std::string shared_message;
std::mutex message_mutex;
bool has_new_message = false;

SDL_Renderer* renderer = nullptr;
SDL_Window* window = nullptr;

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << "Error initializing SDL" << std::endl;
        return 3;
    }

    window = SDL_CreateWindow(
        "resender",
        SCALED_WIDTH,
        SCALED_WIDTH,
        0
        );

    if (!window)
    {
        std::cout << SDL_GetError() << std::endl;
        return 3;
    }

    renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer)
    {
        std::cout << SDL_GetError() << std::endl;
        return 3;
    }



    if (argc != 2)
    {
        std::cout << "Usage: resender <filename>" << std::endl;
        return 1;
    }
    filepath = argv[1];

    std::thread server_thread(startServer, 0x7172, [](const std::string& msg) {
        std::lock_guard<std::mutex> lock(message_mutex);
        shared_message = msg;
        has_new_message = true;
    });
    server_thread.detach();

    init();
    QRcode* qr {};
    chunk_qr(qr, 0);

    if (qr == nullptr)
    {
        std::cout << "Error creating QR code" << std::endl;
        SDL_Quit();
        return 1;
    }


    auto* texture = QRToTexture(qr, SCALE);

    std::string current_message;

    bool quit = false;
    bool start = true;  // because it's started in sender


    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE) quit = true;
                if (event.key.key == SDLK_SPACE)
                {
                    std::cout << "start" << std::endl;
                    start = true;
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        if (!start)
        {
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        }
        else
        {
            if (has_new_message)
            {
                std::lock_guard<std::mutex> lock(message_mutex);
                current_message = shared_message;
                has_new_message = false;
                if (current_message.starts_with("RESEND"))
                {
                    std::cout << "resending" << std::endl;
                    uint32_t chunk = std::stoi(current_message.substr(7));
                    chunk_qr(qr, chunk);
                    if (texture)
                    {
                        SDL_DestroyTexture(texture);
                    }
                    texture = QRToTexture(qr, SCALE);
                }
            }
        }
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(1000 / FPS);
    }
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    QRcode_free(qr);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}

void init()
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Error opening file" << std::endl;
        exit(2);
    }
    file.seekg(0, std::ios::end);
    auto pos = file.tellg();
    if (pos != -1)
    {
        file_size = static_cast<uint64_t>(pos);
    }
    else
    {
        std::cout << "Error reading file" << std::endl;
        exit(2);
    }
    file.clear();
    file.seekg(0, std::ios::beg);

    uint32_t chunk_size = MAX_PAYLOAD;
    uint32_t chunk_count = static_cast<uint32_t>(std::ceil(static_cast<double>(file_size) / static_cast<double>(chunk_size)));
    master_qr =  {MAGIC,
        0,
        40,
        static_cast<int32_t>(QR_ECLEVEL_L),
        0,
        chunk_count,
        chunk_size,
        file_size
    };
}

std::vector<uint8_t> make_packet(const ChunkHeader& head, const uint8_t* payload)
{
    std::vector<uint8_t> packet(sizeof(ChunkHeader) + head.size);

    memcpy(packet.data(), &head, sizeof(ChunkHeader));

    memcpy(
        packet.data() + sizeof(ChunkHeader),
        payload,
        head.size
        );

    return packet;
}

void chunk_qr(QRcode*& qr, uint32_t count)
{
    if (count >= master_qr.chunk_count)
    {
        std::cout << "Invalid chunk request\n";
        return;
    }
    uint32_t size = MAX_PAYLOAD;
    if (MAX_PAYLOAD * (count+1) > file_size)
    {
        size = file_size - MAX_PAYLOAD * count;
    }
    uint32_t offset = count * MAX_PAYLOAD;

    uint8_t* payload = alloc_chunk(filepath.c_str(), offset, size);
    uint32_t crc = libdeflate_crc32(0, payload, size);

    ChunkHeader head {
        count,
            size,
            offset,
            crc
        };

    auto chunk = make_packet(head, payload);
    QRcode* new_qr = QRcode_encodeData(sizeof(head) + size, chunk.data(), 40, QR_ECLEVEL_L);
    QRcode_free(qr);
    qr = new_qr;

    delete_chunk(payload);
}

SDL_Texture* QRToTexture(const QRcode* qr, int scale)
{
    int size = (qr->width + 4 * 2) * scale;

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STATIC,
        size,
        size
    );

    if (!texture)
    {
        std::cout << SDL_GetError() << std::endl;
        return nullptr;
    }



    uint32_t white = SDL_MapRGBA(
        SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888),
        nullptr,
        255,255,255,255
    );

    uint32_t black = SDL_MapRGBA(
        SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888),
        nullptr,
        0,0,0,255
    );

    std::vector<uint32_t> pixels(size * size, white);

    for (int y = 0; y < qr->width; y++)
    {
        for (int x = 0; x < qr->width; x++)
        {
            if (qr->data[y * qr->width + x] & 1)
            {
                for (int yy = 0; yy < scale; yy++)
                {
                    for (int xx = 0; xx < scale; xx++)
                    {
                        pixels[
                            ( (y + 4) * scale + yy) * size +
                            ((x + 4) * scale + xx)
                        ] = black;
                    }
                }
            }
        }
    }


    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        size * sizeof(uint32_t)
    );

    return texture;
}

/*
 * @return Newly allocated buffer. Caller must free with delte_chunk
 */
uint8_t* alloc_chunk(const char* filename, uint64_t offset, uint32_t chunk_size)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open())
    {
        return nullptr;
    }

    file.seekg(offset, std::ios::beg);

    if (!file)
    {
        return nullptr;
    }

    uint8_t* buffer = new uint8_t[chunk_size];

    file.read(
        reinterpret_cast<char*>(buffer),
        chunk_size
    );

    if (!file && !file.eof())
    {
        delete[] buffer;
        return nullptr;
    }

    return buffer;
}

void delete_chunk(const uint8_t* buffer)
{
    delete[] buffer;
}