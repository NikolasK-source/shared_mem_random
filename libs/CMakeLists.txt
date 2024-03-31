# ---------------------------------------- packages --------------------------------------------------------------------
# ======================================================================================================================

find_package(cxxshm REQUIRED)
find_package(cxxsemaphore REQUIRED)
find_package(cxxitimer REQUIRED)
find_package(cxxopts REQUIRED)

# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE rt)
target_link_libraries(${Target} PRIVATE INTERFACE cxxopts)
target_link_libraries(${Target} PRIVATE cxxshm)
target_link_libraries(${Target} PRIVATE cxxsemaphore)
target_link_libraries(${Target} PRIVATE cxxitimer)
