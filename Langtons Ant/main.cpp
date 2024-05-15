#include <windows.h>
#include <iostream>
#include <random>
#include <thread>

#define START_PATTERN 0 // pattern to start from (inclusive)
#define END_PATTERN 100 // pattern to end at (inclusive)
#define REVERSE_PATTERN false // reverse and flip the binary representation of the pattern, turning "101011" into "001010"
#define MAX_ITERATIONS 100000000 // some patterns develop extremely slowly so we need a cutoff
#define NUM_THREADS -1 // number of threads to use, -1 for auto

#define GRID_SIZE 1000 // size of the grid and resulting image
#define GRID_SIZE_HALF 500 // GRID_SIZE / 2
#define GRID_SQUARED 1000000 // GRID_SIZE * GRID_SIZE
#define GRID_INDEX 500500 // (GRID_SIZE * GRID_SIZE_HALF) + GRID_SIZE_HALF
#define FILE_SIZE 1000310 // GRID_SQUARED + 310

static const uint8_t bmp_header[54] = {
    0x42, 0x4D, // bmp signature
    FILE_SIZE & 0xFF, (FILE_SIZE >> 8) & 0xFF, (FILE_SIZE >> 16) & 0xFF, (FILE_SIZE >> 24) & 0xFF, // file size
    0x00, 0x00, 0x00, 0x00, // reserved
    0x36, 0x01, 0x00, 0x00, // offset to pixel data
    0x28, 0x00, 0x00, 0x00, // DIB header size
    GRID_SIZE & 0xFF, (GRID_SIZE >> 8) & 0xFF, 0x00, 0x00, GRID_SIZE & 0xFF, (GRID_SIZE >> 8) & 0xFF, 0x00, 0x00, // image width/height
    0x01, 0x00, // color planes
    0x8, 0x00, // bits per pixel
    0x00, 0x00, 0x00, 0x00, // compression method
    GRID_SQUARED & 0xFF, (GRID_SQUARED >> 8) & 0xFF, (GRID_SQUARED >> 16) & 0xFF, (GRID_SQUARED >> 24) & 0xFF, // image size
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // PPM resolution
    0x40, 0x00, 0x00, 0x00, // color palette size
    0x00, 0x00, 0x00, 0x00 // important colors size
};

void save_bmp(const uint8_t* grid, const uint8_t* palette, const char* filename) {
    HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(fileHandle, bmp_header, sizeof(bmp_header), &written, NULL);
    WriteFile(fileHandle, palette, 256, &written, NULL);
    uint8_t* buffer = new uint8_t[GRID_SQUARED];
    for (uint64_t i = 0; i < GRID_SQUARED; i++) {
        buffer[i] = grid[i];
    }
    WriteFile(fileHandle, buffer, GRID_SQUARED, &written, NULL);
    CloseHandle(fileHandle);
    delete[] buffer;
}

uint8_t highest_set_bit_position(uint64_t value) {
    for (int8_t i = 63; i >= 0; --i) {
        if (value & (1ULL << i)) return i + 1;
    }
    return 0;
}

uint8_t* create_binary_array(uint64_t value, uint8_t size) {
    uint8_t* binary_array = new uint8_t[size];
    for (uint8_t i = 0; i < size; i++) {
        binary_array[REVERSE_PATTERN ? i : size - 1 - i] = ((value >> i) & 1) ^ REVERSE_PATTERN;
    }
    return binary_array;
}

uint8_t* create_palette(uint8_t size) {
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(0, 255);
    uint8_t* colors = new uint8_t[256];
    for (uint16_t i = 0; i < 256; i++) {
        colors[i] = i < size ? distr(eng) : 0;
    }
    return colors;
}

bool is_valid_pattern(uint64_t value) {
    return (value + 1) & value;
}

void ant_thread(uint64_t start, uint64_t end) {
    for (uint64_t i = start; i < end; i++) {
        if (!is_valid_pattern(i)) continue;
        const uint8_t size = highest_set_bit_position(i);
        const uint8_t size_minus_one = size - 1;
        const uint8_t* pattern = create_binary_array(i, size);
        const uint8_t* palette = create_palette(size * 4);
        uint8_t* grid = new uint8_t[GRID_SQUARED]();
        uint32_t index = GRID_INDEX;
        uint16_t ant_position_x = GRID_SIZE_HALF;
        uint16_t ant_position_y = GRID_SIZE_HALF;
        int8_t ant_direction_x = 0;
        int8_t ant_direction_y = 1;
        for (uint64_t j = 0; j < MAX_ITERATIONS; j += 2) {
            uint8_t _grid = grid[index];
            uint8_t _pattern = pattern[_grid];
            int8_t temp_x = ant_direction_x;
            ant_direction_x = _pattern ? -ant_direction_y : ant_direction_y;
            ant_direction_y = _pattern ? temp_x : -temp_x;
            grid[index] = _grid < size_minus_one ? _grid + 1 : 0;
            ant_position_x = ant_position_x + ant_direction_x;
            index += ant_direction_x;
            if (ant_position_x >= GRID_SIZE) break;
            _grid = grid[index];
            _pattern = pattern[_grid];
            temp_x = ant_direction_x;
            ant_direction_x = _pattern ? -ant_direction_y : ant_direction_y;
            ant_direction_y = _pattern ? temp_x : -temp_x;
            grid[index] = _grid < size_minus_one ? _grid + 1 : 0;
            ant_position_y = ant_position_y + ant_direction_y;
            index += ant_direction_y * GRID_SIZE;
            if (ant_position_y >= GRID_SIZE) break;
        }
        save_bmp((uint8_t*)grid, palette, (std::to_string(i) + ".bmp").c_str());
        delete[] pattern;
        delete[] palette;
        delete[] grid;
    }
}

int main() {
    const uint8_t num_threads = NUM_THREADS < 0 ? std::thread::hardware_concurrency() : NUM_THREADS;
    const uint64_t patterns = END_PATTERN - START_PATTERN + 1;
    const uint64_t patterns_per_thread = patterns / num_threads;
    const uint64_t extra = patterns % num_threads;
    std::thread* threads = new std::thread[num_threads];
    for (uint8_t i = 0; i < num_threads; i++) {
        const uint64_t _start = START_PATTERN + i * patterns_per_thread + (i < extra ? i : extra);
        const uint64_t _end = _start + patterns_per_thread + (i < extra);
        threads[i] = std::thread(ant_thread, _start, _end);
    }
    for (uint8_t i = 0; i < num_threads; i++) {
        threads[i].join();
    }
    delete[] threads;
}