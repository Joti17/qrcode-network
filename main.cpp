#include <cstring>
#include <filesystem>

#include "main.hpp"
#include "stdinclude.hpp"
#include "frames.hpp"

/*
Error Codes:
- 1: payload error
- 2: file error
- 3: SDL error




*/

constexpr int32_t FLAGS = 0;
constexpr int32_t VERSION = 40;
constexpr int32_t SCALE = 4;
constexpr int32_t QRCODE_WIDTH = 177;
constexpr int32_t SCALED_WIDTH = SCALE * QRCODE_WIDTH;
constexpr int32_t BORDER = 4;


static uint64_t file_size {};
static std::string filepath {};
static MasterQRCode master_qr {};

SDL_Renderer* renderer = nullptr;
SDL_Window* window = nullptr;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: qrcode-network <filename>" << std::endl;
        std::cin.ignore();
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << "Error initializing SDL" << std::endl;
        return 3;
    }

    window = SDL_CreateWindow(
        "file_transfer",
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

    // tmp
    filepath = argv[1];
    init();
    uint32_t current_chunk = 0;

    QRcode* qr = QRcode_encodeData(static_cast<int32_t>(sizeof(MasterQRCode)), reinterpret_cast<uint8_t*>(&master_qr), VERSION, QR_ECLEVEL_L);

    if (qr == nullptr)
    {
        std::cout << "Error creating QR code" << std::endl;
        SDL_Quit();
        return 1;
    }

    auto* texture = QRToTexture(qr, SCALE);

    bool running = true;
    bool start = false;
    while (running && current_chunk < master_qr.chunk_count)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE) running = false;
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
            next(qr);
            current_chunk++;

            if (texture)
            {
                SDL_DestroyTexture(texture);
            }

            texture = QRToTexture(qr, SCALE);

            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        }

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

    start_resender(filepath);

    std::cin.ignore();

    return 0;
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

/*
QRcode_List* generate_chunk(ChunkHeader head, const uint8_t* payload, uint32_t payload_size)
{
    if (payload_size > MAX_PAYLOAD)
    {
        std::cout << "Payload too large" << std::endl;
        exit(1);
    }

    QRcode_encodeDataStructured(payload_size, payload, 40, QR_ECLEVEL_L);
}
*/

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


void next(QRcode*& qr)
{
    static uint32_t count {};
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
    QRcode* new_qr = QRcode_encodeData(sizeof(head) + size, chunk.data(), VERSION, QR_ECLEVEL_L);
    QRcode_free(qr);
    qr = new_qr;

    delete_chunk(payload);


    count++;
}

SDL_Texture* QRToTexture(const QRcode* qr, int scale)
{
    int size = (qr->width + BORDER * 2) * scale;

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
                            ( (y + BORDER) * scale + yy) * size +
                            ((x + BORDER) * scale + xx)
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

    uint8_t sha256[32] {};
    std::cout << "starting sha256" << std::endl;
    time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    // picosha2::hash256(file, sha256, sha256 + 32);
    try {
        CryptoPP::SHA256 hash;
        CryptoPP::FileSource(
            file,
            true,
            new CryptoPP::HashFilter(
                hash,
                new CryptoPP::ArraySink(sha256, 32)
            )
        );
    }
    catch (const std::exception& e) {
        std::cerr << "Failed hashing: " << e.what() << std::endl;
    }
    time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "finished sha256 in " << end_time - current_time << "Seconds" << std::endl;
    uint32_t chunk_size = MAX_PAYLOAD;
    uint32_t chunk_count = static_cast<uint32_t>(std::ceil(static_cast<double>(file_size) / static_cast<double>(chunk_size)));
    master_qr =  {MAGIC,
        0,
        VERSION,
        static_cast<int32_t>(QR_ECLEVEL_L),
        FLAGS,
        chunk_count,
        chunk_size,
        file_size
    };
    std::memcpy(master_qr.sha256, sha256, sizeof(master_qr.sha256));
    std::string name = std::filesystem::path{filepath}.filename().string();
    // if filename too long only give first 255 chars of the name(255 chars + 1 null term)
    master_qr.filename_size = static_cast<uint16_t>(std::min(name.size(), sizeof(master_qr.filename) - 1));
    std::memcpy(master_qr.filename, name.c_str(), master_qr.filename_size);
    master_qr.filename[master_qr.filename_size] = '\0';
}




