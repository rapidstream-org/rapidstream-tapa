add_executable(jacobi)
target_sources(jacobi
               PRIVATE
               jacobi-host.cpp
               jacobi.cpp)
target_link_libraries(jacobi PRIVATE tlp)
add_test(NAME jacobi COMMAND jacobi)
