#include "main.hpp"
#include "stdinclude.hpp"
#include "frames.hpp"

#include <ZXing/ReadBarcode.h>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/ImageView.h>
#include "networking.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

static MasterQRCode master;
constexpr size_t ONE_GB = 1024 * 1024 * 1024;

/*
void start_resender()
{
    std::string program_name = "resender";
    std::string location;

    std::cout << "Enter location of the file to transfer. ";
    std::cin >> location;

    std::cout << "Starting resender" << std::endl;

#ifdef _WIN32

    std::string command = program_name + ".exe \"" + location + "\"";

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::vector<char> cmd(command.begin(), command.end());
    cmd.push_back('\0');

    CreateProcessA(
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
    );

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
}*/

std::string sha256_to_hex(const uint8_t* sha256, size_t length = 32) {
    std::string hex_str;
    hex_str.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        hex_str += std::format("{:02x}", sha256[i]);
    }
    return hex_str;
}

int main()
{
    time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    cv::VideoCapture cap(0); // Virtual OBS camera
    if (!cap.isOpened()) return -1;

    ZXing::ReaderOptions options;
    options.setFormats(ZXing::BarcodeFormat::QRCode);
    options.setTryRotate(false);
    options.setTryHarder(false);


    cv::Mat frame, grayFrame;

    // hidden window for later keyboard input

    while (master.chunk_count == 0)
    {
        cap >> frame;
        if (frame.empty()) break;

        cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
        ZXing::ImageView zxingView(grayFrame.data, grayFrame.cols, grayFrame.rows, ZXing::ImageFormat::Lum);


        ZXing::Result res = ZXing::ReadBarcode(zxingView, options);

        if (res.isValid())
        {
            const ZXing::ByteArray& zxingBytes = res.bytes();

            uint8_t byte_array[MAX_SIZE];
            size_t size = (zxingBytes.size() < MAX_SIZE) ? zxingBytes.size() : MAX_SIZE;
            memcpy(byte_array, zxingBytes.data(), size);
            int32_t potential_magic = *reinterpret_cast<int32_t*>(byte_array);
            if (potential_magic == MAGIC)
            {
                // MasterQR Code
                std::memcpy(&master, byte_array, sizeof(MasterQRCode));
                if (master.version != 40)
                {
                    std::cout << "Unsupported version" << std::endl;
                    return 1;
                }
                std::cout << "Master qr code found" << std::endl;
                std::cout << "File size: " << master.file_size << std::endl;
                std::cout << "Chunk count: " << master.chunk_count << std::endl;
                std::cout << "Chunk size: " << master.chunk_size << std::endl;
                std::cout << "Filename: " << master.filename << std::endl << std::endl;
                break;
            }
            else
            {
                std::cout << "Wasn't the Master qr code. " << std::endl;
                continue;
            }
        }
    }

    int32_t prev_chunk{-1};
    uint32_t packages_per_gigabyte = static_cast<uint32_t>(ONE_GB / master.chunk_size);
    std::vector<uint32_t> skipped_chunks;
    std::vector<uint8_t> giga_package;
    giga_package.reserve(packages_per_gigabyte * master.chunk_size);

    std::vector<bool> received_chunks(master.chunk_count, false);

    uint64_t current_block_start_offset{0};
    uint32_t chunks_collected{0};
    std::ofstream out_file(master.filename, std::ios::binary | std::ios::out | std::ios::trunc);
    out_file.seekp(master.file_size - 1);
    out_file.write("", 1);

    bool resender_open = false;
    SDL_Event event;
    bool sender_finished = false;
    int invalid_counter = 0;
    bool saved_cursor = false;
    while (!sender_finished)
    {
        cap >> frame;
        if (frame.empty()) break;

        cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
        if (grayFrame.cols <= 0 || grayFrame.rows <= 0 || grayFrame.data == nullptr) continue;


        ZXing::Result res;

        try
        {
            ZXing::ImageView zxingView(grayFrame.data, grayFrame.cols, grayFrame.rows, ZXing::ImageFormat::Lum);
            res = ZXing::ReadBarcode(zxingView, options);
        }
        catch (const std::exception& e)
        {
            // Skip this frame if ZXing fails
            invalid_counter++;
            if (invalid_counter > 10)
            {
                // std::cout << "Skipped following chunks: " << skipped_chunks.size() << std::endl;
                for (auto chunk : skipped_chunks)
                {
                    std::cout << chunk << " ";
                }
            }
            continue;
        }

        if (res.isValid())
        {
            const ZXing::ByteArray& zxingBytes = res.bytes();

            if (zxingBytes.size() < sizeof(ChunkHeader))
                continue;

            uint32_t possible_magic;
            std::memcpy(&possible_magic, zxingBytes.data(), sizeof(possible_magic));
            // std::cout << "possible_magic=" << possible_magic << std::endl;
            if (possible_magic == MAGIC) continue;

            ChunkHeader head;
            std::memcpy(&head, zxingBytes.data(), sizeof(ChunkHeader));

            double progress = (double)chunks_collected / (double)master.chunk_count * 100;
            // chunks/s = FPS | *s / FPS
            // s = chunks/FPS
            uint64_t estimated_time = (master.chunk_count - chunks_collected) / FPS;
            uint32_t minutes = estimated_time / 60;
            uint32_t seconds = estimated_time % 60;
            if (chunks_collected % 50 == 0)
            {
                if (!saved_cursor) {
                    std::cout << "\033[s" << std::flush;       // save cursor
                    saved_cursor = true;
                }

                std::cout   << "\033[u"         // restore cursor
                            << "\033[2K"        // clear line
                            << "\rchunk=" << head.chunk
                            << " | offset=" << head.offset
                            << " | size=" << head.size
                            << " | remaining=" << master.chunk_count - chunks_collected
                            << " | progress=" << std::fixed << std::setprecision(2) << progress << "%"
                            << " | estimated time=" << minutes << ":" << std::setw(2) << std::setfill('0') << seconds << "s"
                            << " | skipped chunks=" << skipped_chunks.size();
            }

            if (zxingBytes.size() < sizeof(ChunkHeader) + head.size)
                continue;

            size_t payload_size = head.size;
            uint32_t crc = libdeflate_crc32(0, zxingBytes.data()+sizeof(ChunkHeader), payload_size);

            if (crc != head.crc)
            {
                std::cout << "CRC mismatch" << std::endl;
                std::cout << "Expected: " << std::hex << head.crc << std::endl;
                std::cout << "Got: " << std::hex << crc << std::endl;
                std::cout << "Try again" << std::endl;
                skipped_chunks.push_back(head.chunk);
                chunks_collected++;
                prev_chunk = head.chunk;
                continue;
            }



            // adjusted size for indecies
            if (head.chunk >= master.chunk_count) continue;

            if (received_chunks[head.chunk]) continue;
            if (head.chunk >= (master.chunk_count - 1)) sender_finished = true;

            if (head.chunk > static_cast<uint32_t>(prev_chunk) + 1)
            {
                // skipped chunk(s)
                for (uint32_t i = prev_chunk + 1; i < head.chunk; i++)
                {
                    if (!received_chunks[i])
                    {
                        skipped_chunks.push_back(i);
                        /*// for a later fixer program to be handled
                        received_chunks[i] = true;
                        chunks_collected++;
                        */
                    }
                }
            }
            prev_chunk = head.chunk;

            // 1GB vector offset
            uint64_t relative_offset = head.offset - current_block_start_offset;

            if (head.offset > current_block_start_offset && relative_offset + payload_size <= (packages_per_gigabyte *
                master.chunk_size))
            {
                if (giga_package.size() < relative_offset + payload_size)
                {
                    giga_package.resize(relative_offset + payload_size);
                }

                std::memcpy(giga_package.data() + relative_offset, zxingBytes.data() + sizeof(ChunkHeader),
                            payload_size);
                received_chunks[head.chunk] = true;
                chunks_collected++;
            }
            else
            {
                // chunk is part of the next gigabyte package
                out_file.seekp(current_block_start_offset);
                out_file.write(reinterpret_cast<const char*>(giga_package.data()), giga_package.size());
                giga_package.clear();
                current_block_start_offset = head.offset;

                uint64_t relative_offset_new = head.offset - current_block_start_offset;
                giga_package.resize(relative_offset_new + payload_size);


                memcpy(giga_package.data() + relative_offset_new, zxingBytes.data() + sizeof(ChunkHeader),
                       payload_size);
                received_chunks[head.chunk] = true;
                chunks_collected++;
            }

            if (std::all_of(received_chunks.begin(), received_chunks.end(),
                            [](bool received) { return received; }))
            {
                out_file.seekp(current_block_start_offset);
                out_file.write(reinterpret_cast<const char*>(giga_package.data()), giga_package.size());
                giga_package.clear();

                std::cout << "All chunks received successfully" << std::endl;
                break;
            }
        }
    }
    if (!giga_package.empty())
    {
        out_file.seekp(current_block_start_offset);
        out_file.write(reinterpret_cast<const char*>(giga_package.data()), giga_package.size());
        giga_package.clear();
    }

    if (!skipped_chunks.empty())
    {
        std::cout << "Please open the resender. " << std::endl;
        std::cout << "Once the resender is open, press ENTER to start resending. " << std::endl;

        std::string dummy;
        // std::getline(std::cin, dummy);
        std::string localhost = "127.0.0.1";

        for (auto chunk : skipped_chunks)
        {
            std::cout << "\n>>> Requesting missing chunk " << chunk << "..." << std::endl;
            sendMessage(localhost, 0x7172, "RESEND " + std::to_string(chunk));
            bool chunk_received = false;
            while (!chunk_received)
            {
                cap >> frame;
                if (frame.empty()) continue;

                cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
                ZXing::ImageView zxingView(grayFrame.data, grayFrame.cols, grayFrame.rows, ZXing::ImageFormat::Lum);

                ZXing::Result res = ZXing::ReadBarcode(zxingView, options);

                if (res.isValid())
                {
                    const ZXing::ByteArray& zxingBytes = res.bytes();

                    if (zxingBytes.size() < sizeof(ChunkHeader))
                        continue;

                    ChunkHeader head;
                    std::memcpy(&head, zxingBytes.data(), sizeof(ChunkHeader));

                    std::cout << "Currently seeing chunk: " << head.chunk << "      \r" << std::flush;

                    if (head.chunk == chunk && zxingBytes.size() >= sizeof(ChunkHeader) + head.size)
                    {
                        size_t payload_size = head.size;

                        out_file.seekp(head.offset);
                        out_file.write(reinterpret_cast<const char*>(zxingBytes.data() + sizeof(ChunkHeader)), payload_size);

                        std::cout << "Successfully recovered chunk " << chunk << '\n';
                        chunk_received = true;
                    }
                }
            }
        }
    }


    out_file.close();


    std::ifstream file(master.filename, std::ios::binary);
    uint8_t sha256[32]{};
    std::cout << "starting sha256" << std::endl;
    try
    {
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
    catch (const std::exception& e)
    {
        std::cerr << "Failed hashing: " << e.what() << std::endl;
    }
    if (memcmp(sha256, master.sha256, 32) != 0)
    {
        std::cout << "Hash mismatch" << std::endl;
        std::cout << "Expected: " << std::hex << sha256_to_hex(master.sha256) <<
            std::endl;
        std::cout << "Got: " << std::hex << sha256_to_hex(sha256) << std::endl;
        std::cout << "Try again" << std::endl;
        file.close();
        std::cin.ignore();
        return 1;
    }

    std::cout << "File received successfully" << std::endl;
    std::cout << "Transfer finished in "
              << std::to_string(static_cast<uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) - current_time)
              << "s"
              <<  std::endl;
    std::cout << "Press ENTER to exit" << std::endl;


    file.close();
    std::cin.ignore();
    return 0;
}
