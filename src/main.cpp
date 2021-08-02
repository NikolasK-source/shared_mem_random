#include <csignal>
#include <cxxopts.hpp>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

static volatile bool terminate = false;

static void sig_term_handler(int) { terminate = true; }

static std::random_device         rd;
static std::default_random_engine re(rd());

/*! \brief fill memory area with random data
 *
 * @tparam T element type
 * @param data pointer to memory area
 * @param elements number of elements in the data area
 * @param bitmask bitmask that is applied to the generated random values
 */
template <typename T>
inline void random_data(void *data, std::size_t elements, std::size_t bitmask = ~static_cast<T>(0)) {
    static_assert(sizeof(T) <= sizeof(bitmask), "must be compiled as 64 bit application.");
    static std::uniform_int_distribution<T> dist(0, std::numeric_limits<T>::max());
    for (std::size_t i = 0; i < elements; ++i) {
        reinterpret_cast<T *>(data)[i] = dist(re) & static_cast<T>(bitmask);
    }
}

int main(int argc, char **argv) {
    const std::string exe_name = std::filesystem::path(argv[0]).filename().string();
    cxxopts::Options  options(exe_name, "Write random values to a shared memory.");

    // clang-format off
    options.add_options()("a,alignment",
                          "use the given byte alignment to generate random values. (1,2,4,8)",
                          cxxopts::value<int>()->default_value("1"))
                         ("m,mask",
                          "optional bitmask (as hex value) that is applied to the generated random values",
                          cxxopts::value<std::string>())
                         ("n,name",
                          "mandatory name of the shared memory object",
                          cxxopts::value<std::string>())
                         ("i,interval",
                          "random value generation interval in milliseconds",
                          cxxopts::value<std::size_t>()->default_value("1000"))
                         ("l,limit",
                          "random interval limit. Runs until termination signal if not specified.",
                          cxxopts::value<std::size_t>()->default_value("0"))
                         ("h,help",
                          "print usage");
    // clang-format on

    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    } catch (cxxopts::OptionParseException &e) {
        std::cerr << "Failed to parse arguments: " << e.what() << '.' << std::endl;
        exit(EX_USAGE);
    }

    if (args.count("help")) {
        std::cout << options.help() << std::endl;
        exit(EX_OK);
    }

    const auto name_count = args.count("name");
    if (name_count != 1) {
        if (!name_count) {
            std::cerr << "no shared memory specified." << std::endl;
            std::cerr << "argument '--name' is mandatory." << std::endl;
        } else {
            std::cerr << "multiple definitions of '--name' are not allowed." << std::endl;
        }
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        exit(EX_USAGE);
    }
    const std::string shm_name = args["name"].as<std::string>();


    enum alignment_t { BYTE = 1, WORD = 2, DWORD = 4, QWORD = 8 } alignment = BYTE;
    const auto alignment_count                                              = args.count("alignment");
    if (alignment_count > 1) {
        std::cerr << "multiple definitions of '--alignment' are not allowed." << std::endl;
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        exit(EX_USAGE);
    } else if (alignment_count == 1) {
        const auto tmp = args["alignment"].as<int>();
        switch (tmp) {
            case 1: alignment = BYTE; break;
            case 2: alignment = WORD; break;
            case 4: alignment = DWORD; break;
            case 8: alignment = QWORD; break;
            default:
                std::cerr << tmp << " is not a valid value for '--alignment'" << std::endl;
                std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
                exit(EX_USAGE);
        }
    }

    const auto  random_interval_ms = args["interval"].as<std::size_t>();
    const auto  interval_counter   = args["limit"].as<std::size_t>();
    std::size_t bitmask            = ~static_cast<std::size_t>(0);  // no mask

    const auto mask_count = args.count("mask");
    if (mask_count > 1) {
        std::cerr << "multiple definitions of '--mask' are not allowed." << std::endl;
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        exit(EX_USAGE);
    } else if (mask_count == 1) {
        const auto &mask = args["mask"].as<std::string>();
        try {
            static_assert(sizeof(unsigned long long) == sizeof(std::size_t), "compile as 64 bit application!");
            bitmask = std::stoull(mask, nullptr, 16);
        } catch (const std::invalid_argument &) {
            std::cerr << '\'' << mask << "' is not a valid value for '--mask'" << std::endl;
            std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
            exit(EX_USAGE);
        }
    }

    if (signal(SIGINT, sig_term_handler) || signal(SIGTERM, sig_term_handler)) {
        perror("signal");
        exit(EX_OSERR);
    }

    // open shared memory
    int fd = shm_open(shm_name.c_str(), O_RDWR, 0660);
    if (fd < 0) {
        perror(("failed to open shared memory '" + shm_name + '\'').c_str());
        exit(EX_OSERR);
    }

    // determine size of shared memory
    struct stat shm_stats {};
    if (fstat(fd, &shm_stats)) {
        perror("fstat");
        close(fd);
        exit(EX_OSERR);
    }
    const auto SIZE = static_cast<std::size_t>(shm_stats.st_size);

    // map shared memory
    void *data = mmap(nullptr, SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED || data == nullptr) {
        perror("mmap");
        if (close(fd)) { perror("close"); }
        exit(EX_OSERR);
    }

    std::cerr << "Opened shared memory '" << shm_name << "'. Size: " << SIZE << (SIZE != 1 ? " bytes" : " byte") << '.'
              << std::endl;

    const struct timespec sleep_time {
        static_cast<__time_t>(random_interval_ms / 1000),
                static_cast<__syscall_slong_t>((random_interval_ms % 1000) * 1000000)
    };

    const std::size_t shm_elements = SIZE / static_cast<std::size_t>(alignment);

    // MAIN loop
    std::size_t counter = 0;
    switch (alignment) {
        case BYTE:
            while (!terminate) {
                random_data<uint8_t>(data, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case WORD:
            while (!terminate) {
                random_data<uint16_t>(data, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case DWORD:
            while (!terminate) {
                random_data<uint32_t>(data, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case QWORD:
            while (!terminate) {
                random_data<uint64_t>(data, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
    }

    // clean
    if (munmap(data, SIZE)) { perror("munmap"); }
    if (close(fd)) { perror("close"); }

    std::cerr << "Terminating..." << std::endl;
}
