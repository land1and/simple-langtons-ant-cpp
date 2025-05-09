#include <windows.h>
#include <string>
#include <thread>
#include <random>

#define START_PATTERN 0 // pattern to start from
#define END_PATTERN 99 // pattern to end at
#define INVERT_PATTERN false // reverse and flip the pattern, for example turning "101011" into "001010"
#define MAX_ITERATIONS 100000000 // max iterations
#define NUM_THREADS -1 // number of threads to use, -1 for auto

#define GRID_SIZE (1024 & ~3) // size of the grid and resulting image rounded down to the neaest multiple of 4
#define GRID_SIZE_HALF (GRID_SIZE / 2)
#define GRID_SQUARED (GRID_SIZE * GRID_SIZE)
#define GRID_INDEX (GRID_SIZE * GRID_SIZE_HALF) + GRID_SIZE_HALF
#define FILE_SIZE (GRID_SQUARED + 310)

static const uint8_t bmp_header[54] = {
    0x42, 0x4D, // signature
    FILE_SIZE & 0xFF, (FILE_SIZE >> 8) & 0xFF, (FILE_SIZE >> 16) & 0xFF, (FILE_SIZE >> 24) & 0xFF, // file size
    0x00, 0x00, 0x00, 0x00, // reserved
    0x36, 0x01, 0x00, 0x00, // offset
    0x28, 0x00, 0x00, 0x00, // header size
    GRID_SIZE & 0xFF, (GRID_SIZE >> 8) & 0xFF, (GRID_SIZE >> 16) & 0xFF, (GRID_SIZE >> 24) & 0xFF, // width
    GRID_SIZE & 0xFF, (GRID_SIZE >> 8) & 0xFF, (GRID_SIZE >> 16) & 0xFF, (GRID_SIZE >> 24) & 0xFF, // height
    0x01, 0x00, // color planes
    0x8, 0x00, // bits per pixel
    0x00, 0x00, 0x00, 0x00, // compression
    GRID_SQUARED & 0xFF, (GRID_SQUARED >> 8) & 0xFF, (GRID_SQUARED >> 16) & 0xFF, (GRID_SQUARED >> 24) & 0xFF, // image size
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ppm resolution
    0x40, 0x00, 0x00, 0x00, // number of colors
    0x00, 0x00, 0x00, 0x00 // important colors size
};

void save_bmp(const uint8_t* grid, const uint8_t* palette, const std::string& filename) {
    HANDLE handle = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(handle, bmp_header, 54, &written, NULL);
    WriteFile(handle, palette, 256, &written, NULL);
    WriteFile(handle, grid, GRID_SQUARED, &written, NULL);
    CloseHandle(handle);
}

void ant_thread(const uint64_t start, const uint64_t end) {
    std::mt19937 gen = std::mt19937{ std::random_device{}() };
    std::uniform_int_distribution<> dist(0, 255);
    for (uint64_t i = start; i < end; ++i) {
        if (!((i + 1) & i)) continue;
        uint8_t size = 0;
        for (int8_t j = 63; j >= 0; --j) {
            if (!(i & (1ULL << j))) continue;
            size = j + 1;
            break;
        }
        uint8_t size_minus_one = size - 1;
        uint8_t* pattern = new uint8_t[size];
        for (uint8_t j = 0; j < size; ++j) {
            pattern[INVERT_PATTERN ? j : size - 1 - j] = ((i >> j) & 1) ^ INVERT_PATTERN;
        }
        uint8_t* palette = new uint8_t[256]();
        for (uint8_t j = 0; j < ((size * 4) - 1); ++j) {
            palette[j] = dist(gen);
        }
        uint8_t* grid = new uint8_t[GRID_SQUARED]();
        uint32_t index = GRID_INDEX;
        uint16_t ant_position_x = GRID_SIZE_HALF;
        uint16_t ant_position_y = GRID_SIZE_HALF;
        int8_t ant_direction = 1;
        for (uint64_t j = 0; j < MAX_ITERATIONS; j += 2) {
            uint8_t state = grid[index];
            uint8_t next = state < size_minus_one ? state + 1 : 0;
            grid[index] = next;
            if (pattern[state]) ant_direction = -ant_direction;
            index += ant_direction;
            ant_position_x += ant_direction;
            if (ant_position_x >= GRID_SIZE) break;
            state = grid[index];
            next = state < size_minus_one ? state + 1 : 0;
            grid[index] = next;
            if (!pattern[state]) ant_direction = -ant_direction;
            index += ant_direction * GRID_SIZE;
            ant_position_y += ant_direction;
            if (ant_position_y >= GRID_SIZE) break;
        }
        save_bmp(grid, palette, std::to_string(i) + ".bmp");
        delete[] pattern;
        delete[] palette;
        delete[] grid;
    }
}

int main() {
    uint64_t num_patterns = END_PATTERN - START_PATTERN + 1;
    uint64_t num_threads = std::min<uint64_t>(std::max<uint64_t>(NUM_THREADS < 0 ? std::thread::hardware_concurrency() : NUM_THREADS, 1U), num_patterns);
    uint64_t patterns_per_thread = num_patterns / num_threads;
    uint64_t extra = num_patterns % num_threads;
    std::vector<std::thread> threads;
    for (uint64_t i = 0; i < num_threads; ++i) {
        uint64_t start = START_PATTERN + (i * patterns_per_thread) + std::min<uint64_t>(i, extra);
        uint64_t end = start + patterns_per_thread + (i < extra);
        threads.emplace_back(ant_thread, start, end);
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
}