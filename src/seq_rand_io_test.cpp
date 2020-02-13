#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
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


using namespace std::chrono;

// Template T should be an object from type: std::chrono::duration
template <class T> class Timer
{
    public:

        Timer()
        {
            start_time = high_resolution_clock::now();
        }

        std::size_t elapsed_time()
        {
            time_point<high_resolution_clock> stop_time = 
                high_resolution_clock::now();

            //TODO C++17: std::chrono::round(duration)
            //T elapsed_time = duration_cast<T>(dur);

            // With C++11 we round the duration count manually...
            // Otherwise appropriate accurancy will be lost!
            duration<double> dur = stop_time - start_time;
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

        ~Timer() {}

    private:
        time_point<high_resolution_clock> start_time;
};


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

    std::cout << &block_data_ptr << std::endl;

    // TODO: Check what is returned!
    return block_data_ptr;
    //return std::move(block_data_ptr);
}


std::size_t read_file(const std::size_t block_size, 
                      const std::size_t count, 
                      const std::string& file_path)
{
    using namespace std::chrono;

    uptr_char_array block_data_ptr(new char[block_size]);

    Timer <seconds> timer;

    std::ifstream infile(file_path, std::ifstream::binary);

    for (std::size_t i = 0; i < count; i++) {
        infile.read(block_data_ptr.get(), block_size);
    }

    infile.close();

    return timer.elapsed_time();
}


std::size_t write_random_file(const std::size_t block_size, 
                              const std::size_t count, 
                              const std::string& file_path)
{
    using namespace std::chrono;

    uptr_char_array block_data_ptr = create_random_block(block_size);

    std::cout << &block_data_ptr << std::endl;

    Timer <seconds> timer;

    std::ofstream outfile(file_path, std::ofstream::binary);

    for (std::size_t i = 0; i < count; i++) {
        outfile.write(block_data_ptr.get(), block_size);
    }

    outfile.flush();
    outfile.close();

    return timer.elapsed_time();
}


std::size_t run_io_test(const size_t block_size, 
                        const size_t total_size, 
                        const std::string& file_path, 
                        const Mode mode)
{

    if(total_size % block_size)
        throw std::runtime_error("Block size must be multiple of total size!");

    const std::size_t count = total_size / block_size;

    if(mode == Mode::read)
    {
        if(file_exists(file_path) == false)
            throw std::runtime_error("File not found: " + file_path);

        return read_file(block_size, count, file_path);

    }
    
    else if(mode == Mode::write)
    {
        if(file_exists(file_path))
            throw std::runtime_error("File already exists: " + file_path);

        return write_random_file(block_size, count, file_path);
    }
    else {
        throw std::runtime_error("No valid mode detected!");
    }
}


int main(int argc, char *argv[])
{
    Args args = procress_args(argc, argv);

    std::size_t elapsed_time = 
        run_io_test(args.block_size, 
                    args.total_size, 
                    args.file_path, 
                    args.mode);

    std::cout << "Elapsed time: " << elapsed_time << std::endl;

    return 0;
}
