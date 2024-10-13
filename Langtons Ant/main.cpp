#define NOMINMAX
#include <windows.h>
#include <string>
#include <thread>
#include <random>

#define START_PATTERN 0 // pattern to start from (inclusive)
#define END_PATTERN 99 // pattern to end at (inclusive)
#define REVERSE_PATTERN false // reverse and flip the binary representation of the pattern, for example turning "101011" into "001010"
#define MAX_ITERATIONS 100000000 // some patterns develop extremely slowly so we need a cutoff
#define NUM_THREADS -1 // number of threads to use, -1 for auto

#define GRID_SIZE 1024 // size of the grid and resulting image
#define GRID_SIZE_HALF 512 // GRID_SIZE / 2
#define GRID_SQUARED 1048576 // GRID_SIZE * GRID_SIZE
#define GRID_INDEX 524800 // (GRID_SIZE * GRID_SIZE_HALF) + GRID_SIZE_HALF
#define FILE_SIZE 1048886 // GRID_SQUARED + 310

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

void save_bmp(const uint8_t* grid, const uint8_t* palette, const std::string& filename) {
    HANDLE fileHandle = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(fileHandle, bmp_header, sizeof(bmp_header), &written, NULL);
    WriteFile(fileHandle, palette, 256, &written, NULL);
    uint8_t* buffer = new uint8_t[GRID_SQUARED];
    for (uint64_t i = 0; i < GRID_SQUARED; ++i) {
        buffer[i] = grid[i];
    }
    WriteFile(fileHandle, buffer, GRID_SQUARED, &written, NULL);
    CloseHandle(fileHandle);
    delete[] buffer;
}

uint8_t highest_set_bit_position(const uint64_t value) {
    for (int8_t i = 63; i >= 0; --i) {
        if (value & (1ULL << i)) return i + 1;
    }
    return 0;
}

uint8_t* create_binary_array(const uint64_t value, const uint8_t size) {
    uint8_t* binary_array = new uint8_t[size];
    for (uint8_t i = 0; i < size; ++i) {
        binary_array[REVERSE_PATTERN ? i : size - 1 - i] = ((value >> i) & 1) ^ REVERSE_PATTERN;
    }
    return binary_array;
}

uint8_t* create_palette(const uint8_t size) {
    std::mt19937 engine = std::mt19937{ std::random_device{}() };
    std::uniform_int_distribution<> distribution(0, 255);
    uint8_t* colors = new uint8_t[256];
    for (uint16_t i = 0; i < 256; ++i) {
        colors[i] = i < size ? distribution(engine) : 0;
    }
    return colors;
}

bool is_valid_pattern(const uint64_t value) {
    return (value + 1) & value;
}

void ant_thread(const uint64_t start, const uint64_t end) {
    for (uint64_t i = start; i < end; ++i) {
        if (!is_valid_pattern(i)) continue;
        const uint8_t size = highest_set_bit_position(i);
        const uint8_t size_minus_one = size - 1;
        const uint8_t* pattern = create_binary_array(i, size);
        const uint8_t* palette = create_palette(size * 4);
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
    const uint64_t num_patterns = END_PATTERN - START_PATTERN + 1;
    const uint64_t num_threads = std::min((uint64_t)std::max(NUM_THREADS < 0 ? std::thread::hardware_concurrency() : NUM_THREADS, 1U), num_patterns);
    const uint64_t patterns_per_thread = num_patterns / num_threads;
    const uint64_t extra = num_patterns % num_threads;
    std::vector<std::thread> threads;
    for (uint64_t i = 0; i < num_threads; ++i) {
        const uint64_t start = START_PATTERN + (i * patterns_per_thread) + std::min(i, extra);
        const uint64_t end = start + patterns_per_thread + (i < extra);
        threads.emplace_back(ant_thread, start, end);
    }
    for (std::thread& thread : threads){
        thread.join();
    }
}