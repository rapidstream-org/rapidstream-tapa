add_executable(vadd)
target_sources(vadd
               PRIVATE
               vadd-host.cpp
               vadd.cpp)
target_link_libraries(vadd PRIVATE tlp)
add_test(NAME vadd COMMAND vadd)
