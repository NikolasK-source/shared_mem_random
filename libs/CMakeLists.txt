
# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================



# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxopts)

add_subdirectory(cxxshm EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxshm)

target_link_libraries(${Target} PRIVATE rt)

