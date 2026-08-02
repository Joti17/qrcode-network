#pragma once
#include <stddef.h>
#include <stdint.h>
#include <qrencode.h>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include <math.h>
#include <libdeflate.h>
#include <picosha2.h>
#include <chrono>
#include <thread>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/files.h>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ReaderOptions.h>
#include <ZXing/ImageView.h>
#include <ZXing/BarcodeFormat.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>


constexpr int32_t CHUNK_SIZE = 16;
constexpr int32_t MAX_SIZE = 2953;
constexpr int32_t MAGIC = 0x23456789;
constexpr int32_t MAX_PAYLOAD = MAX_SIZE - CHUNK_SIZE;
