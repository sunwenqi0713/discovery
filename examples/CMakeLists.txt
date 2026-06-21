############################################################
# discovery examples
############################################################

# CLI peer discovery tool
add_executable(discovery_tools discovery_tools.cpp)
target_link_libraries(discovery_tools PRIVATE discovery::discovery)