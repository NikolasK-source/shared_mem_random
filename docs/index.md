# Shared Memory Random

This application writes random values to a named shared memory.

## Use the Application
The tool must be started with the name of the shared memory to be written to as an argument (```-–name```). 
The shared memory with this name must already exist.

If no further arguments are given, the applications will write random numbers to the entire shared memory at an interval of one second until it is terminated by the user.
The argument ```--interval``` can be used to change the interval. 
It is specified in milliseconds.

The number of executions can be limited with the argument ```--limit```.

The format of the random values can be manipulated via the arguments ```--alignment ``` and ```--mask```.
```--alignment ``` specifies the memory size in which the random values are generated.
Possible are 1, 2, 4 and 8bit.
If a value different from 1 is selected, the size of the shared memory used must be a multiple of the selected size.
```--mask``` specifies a bit mask that is applied to the generated random values.
Respect the endianness of your system!


### Examples
All the following examples use the shared memory with the name mem.

#### Write random values every 10 seconds
```
shared-mem-random -n mem -i 10000
```

#### Write random values to the least significant bit of every second value
Respect the endianness of your system!
```
shared-mem-random -n mem -a 2 -m 0x1
```

#### Write random values once
```
shared-mem-random -n mem -l 1
```

## Install

### Using the Arch User Repository (recommended for Arch based Linux distributions)
The application is available as [shared-mem-random](https://aur.archlinux.org/packages/shared-mem-random) in the [Arch User Repository](https://aur.archlinux.org/).
See the [Arch Wiki](https://wiki.archlinux.org/title/Arch_User_Repository) for information about how to install AUR packages.


### Using the Modbus Collection Flatpak Package: Shared Memory Modbus
[SHM-Modbus](https://nikolask-source.github.io/SHM_Modbus/) is a collection of the shared memory modbus tools.
It is available as flatpak and published on flathub as ```network.koesling.shm-modbs```.

### Using the Standalone Flatpak package
The flatpak package can be installed via the .flatpak file.
This can be downloaded from the GitHub [projects release page](https://github.com/NikolasK-source/shared_mem_random/releases):

```
flatpak install --user shared-mem-random.flatpak
```

The application is then executed as follows:
```
flatpak run network.koesling.shared-mem-random
```

To enable calling with ```shared-mem-random``` [this script](https://gist.github.com/NikolasK-source/f0de7567d5e4f9e5c31b0a60b4ed4f7c) can be used.
In order to be executable everywhere, the path in which the script is placed must be in the ```PATH``` environment variable.


### Build from Source

The following packages are required for building the application:
- cmake
- clang or gcc

Use the following commands to build the application:
```
git clone https://github.com/NikolasK-source/shared_mem_random.git
cd shared_mem_random
mkdir build
cmake -B build . -DCMAKE_BUILD_TYPE=Release -DCLANG_FORMAT=OFF -DCOMPILER_WARNINGS=OFF
cmake --build build
```

The binary is located in the build directory.


## Links to related projects

### General Shared Memory Tools
- [Shared Memory Dump](https://nikolask-source.github.io/dump_shm/)
- [Shared Memory Write](https://nikolask-source.github.io/write_shm/)
- [Shared Memory Random](https://nikolask-source.github.io/shared_mem_random/)

### Modbus Clients
- [RTU](https://nikolask-source.github.io/modbus_rtu_client_shm/)
- [TCP](https://nikolask-source.github.io/modbus_tcp_client_shm/)

### Modbus Shared Memory Tools
- [STDIN to Modbus](https://nikolask-source.github.io/stdin_to_modbus_shm/)
- [Float converter](https://nikolask-source.github.io/modbus_conv_float/)


## License

MIT License

Copyright (c) 2021-2022 Nikolas Koesling

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
