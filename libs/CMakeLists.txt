# ---------------------------------------- packages --------------------------------------------------------------------
# ======================================================================================================================

find_package(cxxshm)
find_package(cxxsemaphore)
find_package(cxxitimer)

# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE rt)
target_link_libraries(${Target} PRIVATE cxxopts)
target_link_libraries(${Target} PRIVATE cxxshm)
target_link_libraries(${Target} PRIVATE cxxsemaphore)
target_link_libraries(${Target} PRIVATE cxxitimer)
