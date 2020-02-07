#include <cstdlib>
#include <iostream>
#include <fstream>
#include <ctime>
#include <array>
#include <regex>

#include <stdio.h>
#include <unistd.h>

#include <typeinfo>

# TODO: const return value
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


int procress_args(int argc, char *argv[])
{
    int opt;

    std::size_t block_size = 0;
    std::size_t total_size = 0;
    std::string file_path;
    char mode = 0;

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
                if(mode)
                    throw std::invalid_argument("Mode is already set!");
                else
                    mode = 'w';
                break;
            case 'w':
                if(mode)
                    throw std::invalid_argument("Mode is already set!");
                else
                    mode = 'r';
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

    return 0;
}


void doIo()
{
    const unsigned int BLOCK_SIZE = 1000;

    std::array<char, BLOCK_SIZE> BLOCK_DATA;

    std::srand(1);

    for (unsigned int i = 0; i < BLOCK_SIZE; ++i) {
        BLOCK_DATA[i] = char(std::rand() % 256);
    }

    std::ofstream outfile("myfile.txt", std::ofstream::binary);
    outfile.write(BLOCK_DATA.data(), BLOCK_SIZE);
    outfile.flush();
    outfile.close();
}



int main(int argc, char *argv[])
{
    procress_args(argc, argv);

    return 0;
}
