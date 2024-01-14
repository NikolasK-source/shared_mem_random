/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "license.hpp"

#include "cxxshm.hpp"
#include <algorithm>
#include <csignal>
#include <cxxopts.hpp>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <sys/mman.h>
#include <sysexits.h>

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
                          "random interval limit. Use 0 for no limit (--> run until SIGINT / SIGTERM).",
                          cxxopts::value<std::size_t>()->default_value("0"))
                         ("o,offset",
                          "skip the first arg bytes of the shared memory",
                          cxxopts::value<std::size_t>()->default_value("0"))
                         ("e,elements",
                          "maximum number of elements to work on (size depends on alignment)",
                          cxxopts::value<std::size_t>())
                         ("c,create",
                          "create shared memory with given size in byte",
                          cxxopts::value<std::size_t>())
                         ("force",
                          "create shared memory even if it exists. (Only relevant if -c is used.)")
                         ("p,permissions",
                          "permission bits that are applied when creating a shared memory. "
                          "(Only relevant if -c is used.) Default: 0660",
                          cxxopts::value<std::string>()->default_value("0660"))
                         ("h,help",
                          "print usage")
                         ("v,version",
                          "print version information")
                         ("license",
                          "show licenses");
    // clang-format on

    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    } catch (cxxopts::OptionParseException &e) {
        std::cerr << "Failed to parse arguments: " << e.what() << '.' << std::endl;
        return EX_USAGE;
    }

    if (args.count("help")) {
        options.set_width(120);
        std::cout << options.help() << std::endl;
        std::cout << std::endl;
        std::cout << "Note: If specified, the offset should be an integer multiple of alignment." << std::endl;
        std::cout << "      Incorrect alignment can significantly reduce performance." << std::endl;
        std::cout << std::endl;
        std::cout << "This application uses the following libraries:" << std::endl;
        std::cout << "  - cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)" << std::endl;
        std::cout << "  - cxxshm (https://github.com/NikolasK-source/cxxshm)" << std::endl;
        return EX_OK;
    }

    // print version
    if (args.count("version")) {
        std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << " (compiled with " << COMPILER_INFO << " on "
                  << SYSTEM_INFO << ')' << std::endl;
        return EX_OK;
    }

    // print licenses
    if (args.count("license")) {
        print_licenses(std::cout);
        return EX_OK;
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
        return EX_USAGE;
    }
    const std::string shm_name = args["name"].as<std::string>();


    enum alignment_t { BYTE = 1, WORD = 2, DWORD = 4, QWORD = 8 } alignment = BYTE;
    const auto alignment_count                                              = args.count("alignment");
    if (alignment_count > 1) {
        std::cerr << "multiple definitions of '--alignment' are not allowed." << std::endl;
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        return EX_USAGE;
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
                return EX_USAGE;
        }
    }

    const auto  random_interval_ms = args["interval"].as<std::size_t>();
    const auto  interval_counter   = args["limit"].as<std::size_t>();
    std::size_t bitmask            = ~static_cast<std::size_t>(0);  // no mask

    const auto mask_count = args.count("mask");
    if (mask_count > 1) {
        std::cerr << "multiple definitions of '--mask' are not allowed." << std::endl;
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        return EX_USAGE;
    } else if (mask_count == 1) {
        const auto &mask = args["mask"].as<std::string>();
        try {
            static_assert(sizeof(unsigned long long) == sizeof(std::size_t), "compile as 64 bit application!");
            bitmask = std::stoull(mask, nullptr, 16);
        } catch (const std::invalid_argument &) {
            std::cerr << '\'' << mask << "' is not a valid value for '--mask'" << std::endl;
            std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
            return EX_USAGE;
        }
    }

    if (signal(SIGINT, sig_term_handler) == SIG_ERR || signal(SIGTERM, sig_term_handler) == SIG_ERR) {
        perror("signal");
        return EX_OSERR;
    }

    std::unique_ptr<cxxshm::SharedMemory> shm;

    if (args.count("create")) {
        const auto shm_size      = args["create"].as<std::size_t>();
        const auto shm_exclusive = !args.count("force");
        const auto shm_mode_str  = args["permissions"].as<std::string>();

        mode_t      mode;
        bool        fail = false;
        std::size_t idx  = 0;
        try {
            mode = std::stoul(shm_mode_str, &idx, 0);
        } catch (const std::exception &) { fail = true; }
        fail = fail || idx != shm_mode_str.size();

        if (fail) throw std::invalid_argument("Failed to parse permissions '" + shm_mode_str + '\'');

        try {
            shm = std::make_unique<cxxshm::SharedMemory>(shm_name, shm_size, false, shm_exclusive, mode);
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            return EX_OSERR;
        }
    } else {
        try {
            shm = std::make_unique<cxxshm::SharedMemory>(shm_name);
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            return EX_OSERR;
        }
    }

    const auto OFFSET = args["offset"].as<std::size_t>();
    const auto SIZE   = OFFSET > shm->get_size() ? 0 : (shm->get_size() - OFFSET);

    std::cerr << "Opened shared memory '" << shm_name << "'. Size: " << shm->get_size()
              << (shm->get_size() != 1 ? " bytes" : " byte") << '.';
    if (OFFSET) std::cerr << " (Effective size: " << SIZE << (SIZE != 1 ? " bytes" : " byte") << ")";
    std::cerr << std::endl;

    if (OFFSET % args["alignment"].as<int>() != 0)
        std::cerr << "WARNING: Invalid alignment detected. Performance issues possible." << std::endl;


    const struct timespec sleep_time {
        static_cast<__time_t>(random_interval_ms / 1000),
                static_cast<__syscall_slong_t>((random_interval_ms % 1000) * 1000000)
    };

    std::size_t shm_elements = SIZE / static_cast<std::size_t>(alignment);
    if (args.count("elements")) { shm_elements = std::min(shm_elements, args["elements"].as<std::size_t>()); }

    if (shm_elements == 0) {
        std::cerr << "no elements to work on. (Either is the shared memory to small to create at least one element ";
        std::cerr << "with the specified allignment or the parameter elements is 0.)" << std::endl;
        return EX_DATAERR;
    }

    // MAIN loop
    std::size_t counter = 0;
    switch (alignment) {
        case BYTE:
            while (!terminate) {
                random_data<uint8_t>(shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case WORD:
            while (!terminate) {
                random_data<uint16_t>(shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case DWORD:
            while (!terminate) {
                random_data<uint32_t>(shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
        case QWORD:
            while (!terminate) {
                random_data<uint64_t>(shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask);
                nanosleep(&sleep_time, nullptr);

                if (interval_counter) {
                    if (++counter > interval_counter) break;
                }
            }
            break;
    }

    std::cerr << "Terminating..." << std::endl;
}
