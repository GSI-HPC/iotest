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
#include <unistd.h>


#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if GCC_VERSION < 40900
    #error "GCC version should be greater than 4.9.0 for regex support!"
#elif GCC_VERSION < 60300
    # warning "GCC version less than 6.3.0 is not tested but might work..."
#endif


using namespace std::chrono;


typedef std::unique_ptr<char[]> uptr_char_array;


enum class Mode: char
{
    none = 'x',
    read ='r',
    write ='w'
};


struct Args 
{
    const std::size_t block_size;
    const std::size_t total_size;
    const std::string file_path;
    const Mode mode;

    Args(const std::size_t block_size, 
         const std::size_t total_size, 
         const std::string& file_path, 
         const Mode mode) 
            : block_size(block_size), 
              total_size(total_size), 
              file_path(file_path), 
              mode(mode) 
    {}
};


class IOTestResult
{
    public:

        IOTestResult(const high_resolution_clock::time_point& start,
                     const high_resolution_clock::time_point& stop,
                     const std::size_t elapsed_time, 
                     const std::size_t throughput)
            : start_(start), 
              stop_(stop), 
              elapsed_time_(elapsed_time), 
              throughput_(throughput)
        {}

        high_resolution_clock::time_point getStart() const {
            return start_;
        }
        
        high_resolution_clock::time_point getStop() const {
            return stop_;
        }

        std::size_t getElapsedTime() const {
            return elapsed_time_;
        }

        std::size_t getThroughput() const {
            return throughput_;
        }

    private:

        high_resolution_clock::time_point start_;
        high_resolution_clock::time_point stop_;
        std::size_t elapsed_time_;
        std::size_t throughput_;
};


// Template T should be an object from type: std::chrono::duration
template <class T> class Timer
{
    public:

        Timer() {}

        void start() {
            start_ = high_resolution_clock::now();
        }
        
        void stop() {
            stop_ = high_resolution_clock::now();
        }

        std::size_t elapsedTime()
        {
            //TODO C++17, which version exactly???: 
            // std::chrono::round(duration)
            // std::chrono::round<std::chrono::seconds>(stop_ - start_);
            // T elapsed_time = duration_cast<T>(dur);

            // With C++11 we round the duration count manually...
            // Otherwise appropriate accurancy will be lost!
            duration<double> dur = stop_ - start_;
            double dur_count = dur.count();
            size_t elapsed_time = 0;
 
            std::cout << dur_count << " \n";

            if(std::is_same<T, std::chrono::milliseconds>::value)
                elapsed_time = std::round(dur_count * 1000);
            else if(std::is_same<T, std::chrono::seconds>::value)
                elapsed_time = std::round(dur_count);
            else
                throw std::runtime_error("Not supported chrono duration!");

            return elapsed_time;
        }

        high_resolution_clock::time_point getStart() {
            return start_;
        }

        high_resolution_clock::time_point getStop() {
            return stop_;
        }

        ~Timer() {}

    private:

        high_resolution_clock::time_point start_;
        high_resolution_clock::time_point stop_;
};


std::string to_datetime_str(const high_resolution_clock::time_point tp)
{
    std::stringstream ss;
    std::time_t t = system_clock::to_time_t(tp);
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %T");
    return ss.str();
}


bool file_exists(const std::string& file_path)
{
    FILE *file = fopen(file_path.c_str(), "r");

    if(file) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}


std::size_t size_to_bytes(const std::string& size)
{
    std::smatch sm;
    const std::string regex_pattern = "^([0-9]{1,4})([KMG]{1})$";
    std::regex regex(regex_pattern);

    if(not std::regex_search(size, sm, regex)) 
        throw std::invalid_argument("Validation of size failed: " + size);

    const std::size_t value = stoi(sm.str(1));
    const char unit = sm.str(2).c_str()[0];

    std::size_t bytes = 0;

    if(unit == 'K')
        bytes = value * 1024;
    else if(unit == 'M')
        bytes = value * 1048576;
    else if(unit == 'G')
        bytes = value * 1073741824;

    return bytes;
}


Args procress_args(int argc, char *argv[])
{
    int opt = 0;

    std::size_t block_size = 0;
    std::size_t total_size = 0;
    std::string file_path;
    Mode mode = Mode::none;

    while((opt = getopt(argc, argv, "b:t:rwf:")) != -1)
    {
        switch(opt)
        {
            case 'b':
                block_size = size_to_bytes(optarg);
                break;
            case 't':
                total_size = size_to_bytes(optarg);
                break;
            case 'r':
                if(mode == Mode::none)
                    mode = Mode::read;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'w':
                if(mode == Mode::none)
                    mode = Mode::write;
                else
                    throw std::invalid_argument("Mode is already set!");
                break;
            case 'f':
                file_path = optarg;
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
        }
    }
    
    for(; optind < argc; optind++) {
        printf("extra arguments: %s\n", argv[optind]);
    }
    
    return Args(block_size, total_size, file_path, mode);
}


uptr_char_array create_random_block(const std::size_t block_size)
{
    std::srand(1);
    uptr_char_array block_data_ptr(new char[block_size]);

    for (std::size_t i = 0; i < block_size; ++i)
        block_data_ptr[i] = char(std::rand() % 256);

    return block_data_ptr;
}


void read_file(const uptr_char_array& block_data, 
               const std::size_t block_size, 
               const std::size_t count, 
               const std::string& file_path)
{
    std::ifstream infile(file_path, std::ifstream::binary);

    for (std::size_t i = 0; i < count; i++) {
        infile.read(block_data.get(), block_size);
    }

    infile.close();
}


void write_file(const uptr_char_array& block_data,
                const std::size_t block_size,
                const std::size_t count, 
                const std::string& file_path)
{
    std::ofstream outfile(file_path, std::ofstream::binary);

    for (std::size_t i = 0; i < count; i++) {
        outfile.write(block_data.get(), block_size);
    }

    outfile.flush();
    outfile.close();
}


IOTestResult run_seq_io_test(const size_t block_size, 
                             const size_t total_size, 
                             const std::string& file_path, 
                             const Mode mode)
{
    if(total_size % block_size)
        throw std::runtime_error("Block size must be multiple of total size!");

    const std::size_t count = total_size / block_size;

    std::size_t elapsed_time = 0;
    std::size_t throughput_mb_per_sec = 0;

    Timer <std::chrono::seconds> timer;

    if(mode == Mode::read)
    {
        if(file_exists(file_path) == false)
            throw std::runtime_error("File not found: " + file_path);

        uptr_char_array block_data(new char[block_size]);

        timer.start();
        read_file(block_data, block_size, count, file_path);
        timer.stop();
    }
    else if(mode == Mode::write)
    {
        if(file_exists(file_path))
            throw std::runtime_error("File already exists: " + file_path);

        uptr_char_array block_data = create_random_block(block_size);

        timer.start();
        write_file(block_data, block_size, count, file_path);
        timer.stop();
    }
    else {
        throw std::runtime_error("No valid mode detected!");
    }

    elapsed_time = timer.elapsedTime();

    if(elapsed_time != 0)
        throughput_mb_per_sec = (total_size / elapsed_time) / 1000000;
    else
        throughput_mb_per_sec = total_size / 1000000;


    return IOTestResult(timer.getStart(), 
                        timer.getStop(), 
                        elapsed_time, 
                        throughput_mb_per_sec);
}


int main(int argc, char *argv[])
{
    Args args = procress_args(argc, argv);

    IOTestResult result = 
        run_seq_io_test(args.block_size, 
                        args.total_size, 
                        args.file_path, 
                        args.mode);

    std::cout << to_datetime_str(result.getStart()) << "|" 
              << to_datetime_str(result.getStop())  << "|" 
              << result.getElapsedTime()            << "|" 
              << result.getThroughput()             << "\n";

    return 0;
}
