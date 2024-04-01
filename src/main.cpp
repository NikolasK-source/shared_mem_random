/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "license.hpp"

#include "cxxitimer.hpp"
#include "cxxsemaphore.hpp"
#include "cxxshm.hpp"
#include <algorithm>
#include <csignal>
#include <cxxopts.hpp>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <sysexits.h>

static constexpr std::size_t MAX_SEM_ERROR     = 1000;
static constexpr std::size_t SEM_ERROR_INC     = 100;
static std::size_t           sem_error_counter = 0;  // NOLINT

static volatile bool terminate = false;  // NOLINT

static void sig_term_handler(int) {
    terminate = true;
}

static std::random_device         rd;        // NOLINT
static std::default_random_engine re(rd());  // NOLINT


/*! \brief fill memory area with random data
 *
 * @tparam T element type
 * @param data pointer to memory area
 * @param elements number of elements in the data area
 * @param bitmask bitmask that is applied to the generated random values
 */
template <typename T>
inline void random_data(void                                     *data,
                        std::size_t                               elements,  // NOLINT
                        std::size_t                               bitmask,   // NOLINT
                        std::unique_ptr<cxxsemaphore::Semaphore> &semaphore,
                        const timespec                            semaphore_max_time) {
    static_assert(sizeof(T) <= sizeof(bitmask), "must be compiled as 64 bit application.");
    static std::uniform_int_distribution<T> dist(0, std::numeric_limits<T>::max());

    if (semaphore) {
        if (semaphore_max_time.tv_sec == 0 && semaphore_max_time.tv_nsec == 0) {
            semaphore->wait();
        } else {
            if (!semaphore->wait(semaphore_max_time)) {
                std::cerr << " WARNING: Failed to acquire semaphore '" << semaphore->get_name()
                          << "' within a half intervall" << '\n';
                sem_error_counter += SEM_ERROR_INC;
                if (sem_error_counter >= MAX_SEM_ERROR) {
                    std::cerr << "ERROR: acquiring semaphore failed to often. Terminating...";
                    terminate = true;
                }
            } else {
                if (sem_error_counter) --sem_error_counter;
            }
        }
    }

    for (std::size_t i = 0; i < elements; ++i) {
        reinterpret_cast<T *>(data)[i] = dist(re) & static_cast<T>(bitmask);  // NOLINT
    }

    if (semaphore && semaphore->is_acquired()) semaphore->post();
}

int main(int argc, char **argv) {  // NOLINT
    const std::string exe_name = std::filesystem::path(*argv).filename().string();
    cxxopts::Options  options(exe_name, "Write random values to a shared memory.");

    options.add_options()("a,alignment",
                          "use the given byte alignment to generate random values. (1,2,4,8)",
                          cxxopts::value<unsigned>()->default_value("1"));
    options.add_options()("m,mask",
                          "optional bitmask (as hex value) that is applied to the generated random values",
                          cxxopts::value<std::string>());
    options.add_options()("n,name", "mandatory name of the shared memory object", cxxopts::value<std::string>());
    options.add_options()("i,interval",
                          "random value generation interval in milliseconds",
                          cxxopts::value<std::size_t>()->default_value("1000"));
    options.add_options()("l,limit",
                          "random interval limit. Use 0 for no limit (--> run until SIGINT / SIGTERM).",
                          cxxopts::value<std::size_t>()->default_value("0"));
    options.add_options()("o,offset",
                          "skip the first arg bytes of the shared memory",
                          cxxopts::value<std::size_t>()->default_value("0"));
    options.add_options()("e,elements",
                          "maximum number of elements to work on (size depends on alignment)",
                          cxxopts::value<std::size_t>());
    options.add_options()("c,create", "create shared memory with given size in byte", cxxopts::value<std::size_t>());
    options.add_options()("force", "create shared memory even if it exists. (Only relevant if -c is used.)");
    options.add_options()("p,permissions",
                          "permission bits that are applied when creating a shared memory. "
                          "(Only relevant if -c is used.) Default: 0660",
                          cxxopts::value<std::string>()->default_value("0660"));
    options.add_options()("semaphore",
                          "protect the shared memory with a named semaphore against simultaneous access. "
                          "If -c is used, the semaphore is created, otherwise an existing semaphore is required.",
                          cxxopts::value<std::string>());
    options.add_options()("semaphore-force",
                          "Force the use of the semaphore even if it already exists. "
                          "Do not use this option per default! "
                          "It should only be used if the semaphore of an improperly terminated instance continues "
                          "to exist as an orphan and is no longer used. "
                          "(Only relevant if -c is used.)");
    options.add_options()("h,help", "print usage");
    options.add_options()("v,version", "print version information");
    options.add_options()("license", "show licenses");

    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    } catch (cxxopts::exceptions::parsing::exception &e) {
        std::cerr << "Failed to parse arguments: " << e.what() << '.' << '\n';
        return EX_USAGE;
    }

    if (args.count("help")) {
        options.set_width(120);  // NOLINT
        std::cout << options.help() << '\n';
        std::cout << '\n';
        std::cout << "Note: If specified, the offset should be an integer multiple of alignment." << '\n';
        std::cout << "      Incorrect alignment can significantly reduce performance." << '\n';
        std::cout << '\n';
        std::cout << "This application uses the following libraries:" << '\n';
        std::cout << "  - cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)" << '\n';
        std::cout << "  - cxxshm (https://github.com/NikolasK-source/cxxshm)" << '\n';
        std::cout << "  - cxxsemaphore (https://github.com/NikolasK-source/cxxsemaphore)" << '\n';
        std::cout << "  - cxxitimer (https://github.com/NikolasK-source/cxxitimer)" << '\n';
        return EX_OK;
    }

    // print version
    if (args.count("version")) {
        std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << " (compiled with " << COMPILER_INFO << " on "
                  << SYSTEM_INFO << ')' << '\n';
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
            std::cerr << "no shared memory specified." << '\n';
            std::cerr << "argument '--name' is mandatory." << '\n';
        } else {
            std::cerr << "multiple definitions of '--name' are not allowed." << '\n';
        }
        std::cerr << "Use '" << exe_name << " --help' for more information." << '\n';
        return EX_USAGE;
    }
    const std::string shm_name = args["name"].as<std::string>();


    enum alignment_t { BYTE = 1, WORD = 2, DWORD = 4, QWORD = 8 } alignment = BYTE;
    const auto alignment_count                                              = args.count("alignment");
    if (alignment_count > 1) {
        std::cerr << "multiple definitions of '--alignment' are not allowed." << '\n';
        std::cerr << "Use '" << exe_name << " --help' for more information." << '\n';
        return EX_USAGE;
    } else if (alignment_count == 1) {
        const auto tmp = args["alignment"].as<unsigned>();
        switch (tmp) {
            case 1: alignment = BYTE; break;   // NOLINT
            case 2: alignment = WORD; break;   // NOLINT
            case 4: alignment = DWORD; break;  // NOLINT
            case 8: alignment = QWORD; break;  // NOLINT
            default:
                std::cerr << tmp << " is not a valid value for '--alignment'" << '\n';
                std::cerr << "Use '" << exe_name << " --help' for more information." << '\n';
                return EX_USAGE;
        }
    }

    const auto  interval_counter   = args["limit"].as<std::size_t>();
    const auto  random_interval_ms = interval_counter != 1 ? args["interval"].as<std::size_t>() : 0;
    std::size_t bitmask            = ~static_cast<std::size_t>(0);  // no mask

    const auto mask_count = args.count("mask");
    if (mask_count > 1) {
        std::cerr << "multiple definitions of '--mask' are not allowed." << '\n';
        std::cerr << "Use '" << exe_name << " --help' for more information." << '\n';
        return EX_USAGE;
    } else if (mask_count == 1) {
        const auto &mask = args["mask"].as<std::string>();
        try {
            static_assert(sizeof(unsigned long long) == sizeof(std::size_t), "compile as 64 bit application!");
            bitmask = std::stoull(mask, nullptr, 16);  // NOLINT
        } catch (const std::invalid_argument &) {
            std::cerr << '\'' << mask << "' is not a valid value for '--mask'" << '\n';
            std::cerr << "Use '" << exe_name << " --help' for more information." << '\n';
            return EX_USAGE;
        }
    }

    if (signal(SIGINT, sig_term_handler) == SIG_ERR || signal(SIGTERM, sig_term_handler) == SIG_ERR) {
        perror("signal");
        return EX_OSERR;
    }

    std::unique_ptr<cxxshm::SharedMemory> shm;
    const bool                            ARG_CREATE = args.count("create");

    if (ARG_CREATE) {
        const auto shm_size      = args["create"].as<std::size_t>();
        const auto shm_exclusive = !args.count("force");
        const auto shm_mode_str  = args["permissions"].as<std::string>();

        mode_t      mode = 0;
        bool        fail = false;
        std::size_t idx  = 0;
        try {
            mode = static_cast<decltype(mode)>(std::stoul(shm_mode_str, &idx, 0));
        } catch (const std::exception &) { fail = true; }
        fail = fail || idx != shm_mode_str.size();

        if (fail) throw std::invalid_argument("Failed to parse permissions '" + shm_mode_str + '\'');

        try {
            shm = std::make_unique<cxxshm::SharedMemory>(shm_name, shm_size, false, shm_exclusive, mode);
        } catch (std::exception &e) {
            std::cerr << e.what() << '\n';
            return EX_OSERR;
        }
    } else {
        try {
            shm = std::make_unique<cxxshm::SharedMemory>(shm_name);
        } catch (std::exception &e) {
            std::cerr << e.what() << '\n';
            return EX_OSERR;
        }
    }

    const auto OFFSET = args["offset"].as<std::size_t>();
    const auto SIZE   = OFFSET > shm->get_size() ? 0 : (shm->get_size() - OFFSET);

    std::cerr << "Opened shared memory '" << shm_name << "'. Size: " << shm->get_size()
              << (shm->get_size() != 1 ? " bytes" : " byte") << '.';
    if (OFFSET) std::cerr << " (Effective size: " << SIZE << (SIZE != 1 ? " bytes" : " byte") << ")";
    std::cerr << '\n';

    if (OFFSET % args["alignment"].as<unsigned>() != 0)
        std::cerr << "WARNING: Invalid alignment detected. Performance issues possible." << '\n';

    std::size_t shm_elements = SIZE / static_cast<std::size_t>(alignment);
    if (args.count("elements")) { shm_elements = std::min(shm_elements, args["elements"].as<std::size_t>()); }

    if (shm_elements == 0) {
        std::cerr << "no elements to work on. (Either is the shared memory to small to create at least one element ";
        std::cerr << "with the specified allignment or the parameter elements is 0.)" << '\n';
        return EX_DATAERR;
    }

    std::unique_ptr<cxxsemaphore::Semaphore> semaphore;
    timespec                                 semaphore_max_time {0, 0};

    if (args.count("semaphore")) {
        const auto semaphore_name = args["semaphore"].as<std::string>();

        try {
            if (ARG_CREATE) {
                const bool force = args.count("semaphore-force");
                semaphore        = std::make_unique<cxxsemaphore::Semaphore>(semaphore_name, 1, force);
            } else {
                semaphore = std::make_unique<cxxsemaphore::Semaphore>(semaphore_name);
            }
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            return EX_SOFTWARE;
        }

        semaphore_max_time = {static_cast<__time_t>((random_interval_ms / 2) / 1000),                        // NOLINT
                              static_cast<__syscall_slong_t>(((random_interval_ms / 2) % 1000) * 1000000)};  // NOLINT
    }

    // interval timer
    const struct timeval interval_time {
        static_cast<__time_t>(random_interval_ms / 1000),                           // NOLINT
                static_cast<__syscall_slong_t>((random_interval_ms % 1000) * 1000)  // NOLINT
    };
    cxxitimer::ITimer_Real interval_timer(interval_time);
    if (random_interval_ms) interval_timer.start();

    sigset_t sleep_sigset;
    sigemptyset(&sleep_sigset);
    sigaddset(&sleep_sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sleep_sigset, nullptr);

    // MAIN loop
    std::size_t counter = 0;

    auto handle_counter = [&]() {
        if (interval_counter) {
            if (++counter >= interval_counter) return true;
        }
        return false;
    };

    auto handle_sleep = [&]() {
        if (random_interval_ms == 0) return;

        int  sig = 0;
        auto tmp = sigwait(&sleep_sigset, &sig);
        if (tmp == -1) {
            perror("sigwait");
            exit(EX_OSERR);
        }
    };

    switch (alignment) {
        case BYTE:
            while (!terminate) {
                random_data<uint8_t>(
                        shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask, semaphore, semaphore_max_time);
                handle_sleep();
                if (handle_counter()) break;
            }
            break;
        case WORD:
            while (!terminate) {
                random_data<uint16_t>(
                        shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask, semaphore, semaphore_max_time);
                handle_sleep();
                if (handle_counter()) break;
            }
            break;
        case DWORD:
            while (!terminate) {
                random_data<uint32_t>(
                        shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask, semaphore, semaphore_max_time);
                handle_sleep();
                if (handle_counter()) break;
            }
            break;
        case QWORD:
            while (!terminate) {
                random_data<uint64_t>(
                        shm->get_addr<uint8_t *>() + OFFSET, shm_elements, bitmask, semaphore, semaphore_max_time);
                handle_sleep();
                if (handle_counter()) break;
            }
            break;
    }

    std::cerr << "Terminating..." << '\n';
}
