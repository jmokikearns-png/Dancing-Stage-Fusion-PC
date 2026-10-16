# Linux -> Windows x86_64 cross toolchain for PSXRecomp games, using Ubuntu's
# posix-thread MinGW-w64 compilers (full C++11 thread support).
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/recomp-src/psxrecomp/cmake/toolchain-mingw-w64.cmake")
set(CMAKE_C_COMPILER   "x86_64-w64-mingw32-gcc-posix")
set(CMAKE_CXX_COMPILER "x86_64-w64-mingw32-g++-posix")
