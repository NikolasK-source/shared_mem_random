# Shared Mem Random

This application fills a shared memory with random values.

## Build
```
git submodule init
git submodule update
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_BUILD_TYPE=Release -DCLANG_FORMAT=OFF -DCOMPILER_WARNINGS=OFF
cmake --build .
```

As an alternative to the ```git submodule``` commands, the ```--recursive``` option can be used with ```git clone```.

## Use
```
SHM_Random [OPTION...]

  -a, --alignment arg  use the given byte alignment to generate random values. (1,2,4,8) (default: 1)
  -m, --mask arg       optional bitmask (as hex value) that is applied to the generated random values
  -n, --name arg       mandatory name of the shared memory object
  -i, --interval arg   random value generation interval in milliseconds (default: 1000)
  -l, --limit arg      random interval limit. Use 0 for no limit (--> run until SIGINT / SIGTERM). (default: 0)
  -h, --help           print usage
```

## Libraries
This application uses the following libraries:
- cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)
