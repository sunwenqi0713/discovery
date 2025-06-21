############################################################
# discovery examples
############################################################

# Echo server example
add_executable(discovery_tools main.cpp)
target_link_libraries(discovery_tools PRIVATE discovery::discovery)