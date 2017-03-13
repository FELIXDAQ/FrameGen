cmake_minimum_required(VERSION 2.8.11)

# Leave following 4 lines at the top of this file if you only want out-of-source builds.
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  file(REMOVE CMakeCache.txt)
  message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

project (libframegen)
set(VERSION "0.1.0")


## Check Dependencies ##
find_package( ZLIB REQUIRED 1.2.11 REQUIRED)
if ( NOT ZLIB_FOUND )
    message (FATAL_ERROR "Fatal error: ZLIB (version >= 1.2.11) required.\n")
endif( NOT ZLIB_FOUND )


## COMPILER SETUP ##
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -Wall -g")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
    message(INFO "${CMAKE_CXX_COMPILER_VERSION}")
    message(FATAL_ERROR "GCC version must be at least 4.8!")
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -opt-prefetch -unroll-aggressive -march=native -mtune=native")
endif()


## SOURCES AND TARGETS ##
include_directories("." ${CMAKE_BINARY_DIR} ${ZLIB_INCLUDE_DIRS})

file(GLOB FRAMEGEN_SOURCES FrameGen.cpp)

add_library(framegen SHARED ${FRAMEGEN_SOURCES})
target_link_libraries(framegen ${ZLIB_LIBRARIES})

add_executable(framegen-test main.cpp)
target_link_libraries(framegen-test framegen ${ZLIB_LIBRARIES})

## INSTALLATION ##
install(TARGETS framegen
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib)
install(FILES FrameGen.hpp DESTINATION include)
