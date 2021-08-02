# Shared Mem Random

This application fills a shared memory with random values.

## Build
```
git submodule init
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=$(which clang++)
cmake -build . 
```

## Use
```
SHM_Random [OPTION...]
  
  -a, --alignment arg  use the given byte alignment to generate random 
                       values. (1,2,4,8) (default: 1)
  -m, --mask arg       optional bitmask (as hex value) that is applied to 
                       the generated random values
  -n, --name arg       mandatory name of the shared memory object
  -i, --interval arg   random value generation interval in milliseconds 
                       (default: 1000)
  -l, --limit arg      random interval limit. Runs until termination signal 
                       if not specified. (default: 0)
  -h, --help           print usage

```

## Libraries
This application uses the following libraries:
- cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)