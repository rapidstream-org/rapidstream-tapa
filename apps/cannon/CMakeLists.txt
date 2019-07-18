add_executable(cannon)
target_sources(cannon
               PRIVATE
               cannon-host.cpp
               cannon.cpp)
target_link_libraries(cannon PRIVATE tlp)
add_test(NAME cannon COMMAND cannon)
