find_package(Boost 1.59 COMPONENTS coroutine stacktrace_backtrace)
find_package(FRT REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/TAPATargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/TAPACCConfig.cmake")
