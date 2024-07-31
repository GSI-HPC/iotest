// -*- coding: utf-8 -*-
//
// © Copyright 2023 GSI Helmholtzzentrum für Schwerionenforschung
//
// This software is distributed under
// the terms of the GNU General Public Licence version 3 (GPL Version 3),
// copied verbatim in the file "LICENCE".

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <array>
#include <regex>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if GCC_VERSION < 40900
    #error "GCC version should be greater than 4.9.0 for regex support!"
#endif

using namespace std::chrono;

typedef std::unique_ptr<char[]> uptr_char_array;

enum class Mode: char
{
    none             = 'x',
    sequential_read  = 'r',
    sequential_write = 'w',
    random_read      = 'R',
    random_write     = 'W'
};

struct Args
{
    const std::string block_size;
    const std::string total_size;
    const std::string filepath;
    const unsigned seed;
    const Mode mode;
    const bool sync_write;

    Args(const std::string& block_size,
         const std::string& total_size,
         const std::string& filepath,
         const int seed,
         const Mode& mode,
         const bool sync_write)
            : block_size(block_size),
              total_size(total_size),
              filepath(filepath),
              seed(seed),
              mode(mode),
              sync_write(sync_write)
    {}
};

class IOTestResult
{
    public:

        IOTestResult(const high_resolution_clock::time_point& start_time,
                     const high_resolution_clock::time_point& stop_time,
                     const std::size_t elapsed_time,
                     const std::size_t throughput,
                     const std::string& description,
                     const std::string& filepath)
            : start_time_(start_time),
              stop_time_(stop_time),
              elapsed_time_(elapsed_time),
              throughput_(throughput),
              description_(description),
              filepath_(filepath)
        {}

        high_resolution_clock::time_point start_time() const {
            return start_time_;
        }

        high_resolution_clock::time_point stop_time() const {
            return stop_time_;
        }

        std::size_t elapsed_time() const {
            return elapsed_time_;
        }

        std::size_t throughput() const {
            return throughput_;
        }

        std::string description() const {
            return description_;
        }

        std::string filepath() const {
            return filepath_;
        }

    private:

        high_resolution_clock::time_point start_time_;
        high_resolution_clock::time_point stop_time_;
        std::size_t elapsed_time_;
        std::size_t throughput_;
        std::string description_;
        std::string filepath_;
};

// Optional, since program creates just one file and closes the file on exit, but good practice to close file.
// TODO Write FileHandler class if used for more test cases.
class CloseFileHandler
{
    public:

        CloseFileHandler(const int fd)
            : fd(fd)
        {}

        ~CloseFileHandler()
        {
            if(fd > 2)
                close(fd);
        }

    private:
        const int fd;
};

// Template T should be an object from type: std::chrono::duration
template <class T> class Timer
{
    public:

        Timer() {}

        void Start() {
            start_time_ = high_resolution_clock::now();
        }

        void Stop() {
            stop_time_ = high_resolution_clock::now();
        }

        std::size_t ElapsedTime()
        {
            //TODO C++17, which version exactly???:
            // std::chrono::round(duration)
            // std::chrono::round<std::chrono::seconds>(stop_ - start_);
            // T elapsed_time = duration_cast<T>(dur);

            // With C++11 we round the duration count manually...
            // Otherwise appropriate accurancy will be lost!
            duration<double> dur = stop_time_ - start_time_;
            double dur_count = dur.count();
            size_t elapsed_time = 0;

            if(std::is_same<T, std::chrono::milliseconds>::value)
                elapsed_time = std::round(dur_count * 1000);
            else if(std::is_same<T, std::chrono::seconds>::value)
                elapsed_time = std::round(dur_count);
            else
                throw std::runtime_error("Not supported chrono duration!");

            return elapsed_time;
        }

        high_resolution_clock::time_point start_time() {
            return start_time_;
        }

        high_resolution_clock::time_point stop_time() {
            return stop_time_;
        }

        ~Timer() {}

    private:

        high_resolution_clock::time_point start_time_;
        high_resolution_clock::time_point stop_time_;
};

std::string to_datetime_str(const high_resolution_clock::time_point tp)
{
    std::stringstream ss;
    std::time_t t = system_clock::to_time_t(tp);
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %T");
    return ss.str();
}

bool file_exists(const std::string& filepath)
{
    FILE *file = fopen(filepath.c_str(), "r");

    if(file) {
        fclose(file);
        return true;
    } else
        return false;
}

std::size_t size_to_bytes(const std::string& size)
{
    std::smatch sm;
    const std::string regex_pattern = "^([1-9]{1}[0-9]{0,2})([BKMGT]{1})$";
    std::regex regex(regex_pattern);

    if(not std::regex_search(size, sm, regex))
        throw std::invalid_argument("Validation of size failed: " + size);

    const std::size_t value = stoi(sm.str(1));
    const char unit = sm.str(2).c_str()[0];

    std::size_t bytes = 0;

    if(unit == 'B')
        bytes = value;
    else if(unit == 'K')
        bytes = value * 1024;
    else if(unit == 'M')
        bytes = value * 1048576;
    else if(unit == 'G')
        bytes = value * 1073741824;
    else if(unit == 'T')
        bytes = value * 1099511627776;

    return bytes;
}

Args process_args(int argc, char *argv[])
{
    int opt = 0;

    std::string block_size;
    std::string total_size;
    std::string filepath;
    Mode mode = Mode::none;
    unsigned seed = 1;
    bool sync_write = false;

    std::string help_message = "USAGE:\n"
                               "-b BLOCK SIZE 1-999{KM}/1G\n"
                               "-t TOTAL SIZE 1-999{KMGT}\n"
                               "IO MODES:\n"
                               "   * -r SEQUENTIAL READ\n"
                               "   * -w SEQUENTIAL WRITE\n"
                               "   * -R RANDOM READ\n"
                               "   * -W RANDOM WRITE\n"
                               "-f FILEPATH\n"
                               "-s SEED NUMBER\n"
                               "-y SYNC ON WRITE\n";

    if(argc == 1) {
        std::cout << help_message << "\n";
        exit(0);
    }

    // A colon ':' in getopt() indicates that an argument has a parameter and is not a switch.
    while((opt = getopt(argc, argv, "b:t:hrwRWf:s:y")) != -1) {

        switch(opt) {
            case 'b':
                block_size = optarg;
                break;
            case 't':
                total_size = optarg;
                break;
            case 'r':
                if(mode == Mode::none)
                    mode = Mode::sequential_read;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'w':
                if(mode == Mode::none)
                    mode = Mode::sequential_write;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'R':
                if(mode == Mode::none)
                    mode = Mode::random_read;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'W':
                if(mode == Mode::none)
                    mode = Mode::random_write;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'f':
                filepath = optarg;
                break;
            case 's':
                seed = unsigned(std::atoi(optarg));
                if(seed == 0)
                    throw std::runtime_error("Bad seed: " + std::to_string(seed));
                break;
            case 'y':
                sync_write = true;
                break;
            case 'h':
                std::cout << help_message << "\n";
                exit(0);
                break;
            case '?':
                throw std::invalid_argument("Unknown option: " + std::to_string(optopt));
                break;
        }
    }

    for(; optind < argc; optind++)
        printf("Extra arguments: %s\n", argv[optind]);

    if(sync_write && (mode == Mode::sequential_read || mode == Mode::random_read))
        throw std::invalid_argument("Sync flag is only allowed with a write mode enabled.");

    return Args(block_size, total_size, filepath, seed, mode, sync_write);
}

uptr_char_array create_random_block(const std::size_t block_size)
{
    uptr_char_array block_data_ptr(new char[block_size]);

    for(std::size_t i = 0; i < block_size; ++i)
        block_data_ptr[i] = char(std::rand() % 256);

    return block_data_ptr;
}

void read_file(const std::size_t block_size,
               const std::size_t total_size,
               const std::string& filepath,
               const bool enable_random)
{
    int fd = open(filepath.c_str(), O_RDONLY);
    CloseFileHandler close_file(fd);

    if(fd < 0)
        throw std::runtime_error("Error opening file: " + std::string(strerror(errno)));

    uptr_char_array block_data(new char[block_size]);

    const std::size_t count = total_size / block_size;
    const std::size_t limit = total_size - block_size;

    const std::size_t rest_size = total_size % block_size;

    std::size_t seek_pos = -1;

    for(std::size_t i = 0; i < count; i++) {

        if(enable_random) {

            seek_pos = std::rand() % (limit+1);
            off_t offset = lseek(fd, seek_pos, SEEK_SET);

            if(offset == -1)
                throw std::runtime_error("Failed to seek: " + std::string(strerror(errno)));
        }
        ssize_t file_in = read(fd, block_data.get(), block_size);

        if(file_in != 0) {

            if(file_in == block_size) {
                continue;
            }
            else if(file_in < block_size) {
                throw std::runtime_error("Incomplete reading of data block: " + std::string(strerror(errno)));
            }
            else if(file_in < 0) {
                throw std::runtime_error("Failed reading data block: " + std::string(strerror(errno)));
            }
        }
        else
            std::cout << "File reading complete!!!";
            return;
    }

    if(rest_size) {

        if(enable_random) {

            seek_pos = std::rand() % (limit+1);
            off_t offset = lseek(fd, seek_pos, SEEK_SET);

            if(offset == -1)
                throw std::runtime_error("Failed to seek: " + std::string(strerror(errno)));
        }

        ssize_t file_in = read(fd, block_data.get(), rest_size);

        if(file_in != 0) {

            if(file_in == rest_size) {
                std::cout << " File reading complete!!!";
                return;
            }
            else if(file_in < rest_size) {
                throw std::runtime_error("Incomplete reading of data block: " + std::string(strerror(errno)));
            }
            else if(file_in < 0) {
                throw std::runtime_error("Failed reading data block: " + std::string(strerror(errno)));
            }
        }
        else
            std::cout << "File reading complete!!!";
            return;

    }

}

void write_file(const std::size_t block_size,
                const std::size_t total_size,
                const std::string& filepath,
                const bool sync_write,
                const bool enable_random)
{
    int flags = O_WRONLY;

    if(enable_random == false)
        flags = flags | O_CREAT | O_TRUNC | O_APPEND;

    if(sync_write)
        flags = flags | O_SYNC;

    int fd = open(filepath.c_str(), flags, S_IRWXU);
    CloseFileHandler close_file(fd);

    if(fd < 0)
        throw std::runtime_error("Error opening file: " + std::string(strerror(errno)));

    uptr_char_array block_data = create_random_block(block_size);

    const std::size_t count = total_size / block_size;
    const std::size_t limit = total_size - block_size;

    const std::size_t rest_size = total_size % block_size;

    std::size_t seek_pos = -1;

    for(std::size_t i = 0; i < count; i++) {

        if(enable_random) {

            seek_pos = std::rand() % (limit+1);
            off_t offset = lseek(fd, seek_pos, SEEK_SET);

            if(offset == -1)
                throw std::runtime_error("Failed to seek: " + std::string(strerror(errno)));
        }

        ssize_t file_out = write(fd, block_data.get(), block_size);

        if(file_out < 0)
            throw std::runtime_error("Failed writing data block: " + std::string(strerror(errno)));
    }

    if(rest_size) {

        if(enable_random) {

            seek_pos = std::rand() % (limit+1);
            off_t offset = lseek(fd, seek_pos, SEEK_SET);

            if(offset == -1)
                throw std::runtime_error("Failed to seek: " + std::string(strerror(errno)));
        }

        ssize_t file_out = write(fd, block_data.get(), rest_size);

        if(file_out < 0)
            throw std::runtime_error("Failed writing data block: " + std::string(strerror(errno)));
    }
}

IOTestResult run_io_test(const std::string& block,
                         const std::string& total,
                         const std::string& filepath,
                         const unsigned seed,
                         const Mode mode,
                         const bool sync_write)
{
    std::string description;
    std::size_t block_size = size_to_bytes(block);
    std::size_t total_size = size_to_bytes(total);

    if(block_size > 1073741824)
        throw std::runtime_error("Max block size is 1GiB!");

    std::size_t elapsed_time = 0;
    std::size_t throughput_mb_per_sec = 0;

    Timer <std::chrono::seconds> timer;

    std::srand(seed);

    if(mode == Mode::sequential_read) {

        if(file_exists(filepath) == false)
            throw std::runtime_error("File not found: " + filepath);

        description = "sequential-read-" + block + "-" + total;

        timer.Start();
        read_file(block_size, total_size, filepath, false);
        timer.Stop();

    } else if(mode == Mode::sequential_write) {

        if(sync_write)
            description = "sequential-sync-write-" + block + "-" + total;
        else
            description = "sequential-write-" + block + "-" + total;

        timer.Start();
        write_file(block_size, total_size, filepath, sync_write, false);
        timer.Stop();

    } else if(mode == Mode::random_read) {

        if(file_exists(filepath) == false)
            throw std::runtime_error("File not found: " + filepath);

        description = "random-read-" + block + "-" + total;

        timer.Start();
        read_file(block_size, total_size, filepath, true);
        timer.Stop();

    } else if(mode == Mode::random_write) {

        if(file_exists(filepath) == false)
            throw std::runtime_error("File not found: " + filepath);

        if(sync_write)
            description = "random-sync-write-" + block + "-" + total;
        else
            description = "random-write-" + block + "-" + total;

        timer.Start();
        write_file(block_size, total_size, filepath, sync_write, true);
        timer.Stop();

    } else
        throw std::runtime_error("No valid mode detected!");

    elapsed_time = timer.ElapsedTime();

    if(elapsed_time != 0)
        throughput_mb_per_sec = (total_size / elapsed_time) / 1000000;
    else
        throughput_mb_per_sec = total_size / 1000000;

    return IOTestResult(timer.start_time(),
                        timer.stop_time(),
                        elapsed_time,
                        throughput_mb_per_sec,
                        description,
                        filepath);
}

int main(int argc, char *argv[])
{
    Args args = process_args(argc, argv);

    IOTestResult result = run_io_test(args.block_size,
                                      args.total_size,
                                      args.filepath,
                                      args.seed,
                                      args.mode,
                                      args.sync_write);

    std::cout << to_datetime_str(result.start_time()) << "|"
              << to_datetime_str(result.stop_time())  << "|"
              << result.elapsed_time()                << "|"
              << result.throughput()                  << "|"
              << result.description()                 << "|"
              << result.filepath()                    << "|"
              << "\n";

    return 0;
}
