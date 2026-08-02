# QRCode Network

A file transfer system that uses QR codes as a communication medium.  
The sender encodes file data into QR codes and displays them frame-by-frame. The receiver captures these QR codes through a camera feed and reconstructs the original file.

## Features

- Transfers files using QR codes
- One QR code transmitted per frame
- Default transmission speed of 120 QR codes per second
- Chunk-based file reconstruction
- Hash verification for transferred data
- Works through OBS Virtual Camera

## Requirements

- C++20 compiler
- CMake
- Ninja
- vcpkg
- OBS Studio with Virtual Camera support
- A camera or virtual camera capable of the configured FPS

## Dependencies

Install dependencies using vcpkg:

```bash
vcpkg install
```

For the static MinGW triplet:

```bash
vcpkg install --triplet x64-mingw-static
```

## Building

Clone the repository:

```bash
git clone https://github.com/Joti17/qrcode-network.git
cd qrcode-network
```

Configure a Release build:

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static
```

Build:

```bash
cmake --build build --config Release
```

The compiled binaries will be located inside the `build` directory.

## Performance

The sender transmits exactly one QR code per frame.

Default configuration:

```cpp
FPS = 120
```

Each QR code contains approximately:

```
2937 bytes of payload data
```

The theoretical raw transfer speed is:

```
2937 bytes × 120 FPS = 352440 bytes/s
```

Approximately:

```
344 KiB/s
```

Actual transfer speed depends on:

- QR decoding speed
- Camera/virtual camera latency
- Dropped frames
- CPU performance
- Image quality
- Scaling/filtering applied by the camera pipeline

## Usage

The sender and receiver communicate through the OBS Virtual Camera.

### 1. Configure OBS Virtual Camera

Open OBS Studio:

1. Create a scene where the generated QR code is visible.
2. Start **OBS Virtual Camera**.
3. Set the FPS to match the `FPS` constant in the source code.

Default:

```cpp
FPS = 120
```

The sender outputs:

```
120 QR codes / second
```

If the `FPS` value is changed, OBS Virtual Camera must use the same FPS.

---

### 2. Start Sender

Run:

```bash
qrcode_network <filepath>
```

Example:

```bash
qrcode_network example.zip
```

The sender will load the file and wait for the start command.

---

### 3. Start Receiver

Launch the receiver application.

The receiver should connect to the OBS Virtual Camera and wait for incoming QR codes.

---

### 4. Start Transmission

After both sender and receiver are running, focus the sender window and press:

```
SPACE
```

The sender will begin displaying QR codes frame-by-frame.

The receiver will decode the QR codes and reconstruct the original file.

## Troubleshooting

### QR codes are not detected

Check:

- OBS Virtual Camera is running.
- OBS FPS matches the sender `FPS` value.
- The full QR code is visible.
- The camera resolution is high enough.
- No unwanted scaling or filtering is applied.

### Transfer speed is lower than expected

Possible causes:

- Receiver cannot decode every frame.
- Camera pipeline introduces latency.
- OBS is not running at the configured FPS.
- CPU usage is too high.

Try:

- Lowering FPS.
- Increasing camera resolution.
- Removing image scaling.
- Using a direct virtual camera feed.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
