cmake_minimum_required(VERSION 3.5)
project(aicup2019)

# OS and compiler checks.
if(WIN32)
    add_definitions(-DWIN32)
    SET(PROJECT_LIBS Ws2_32.lib)
endif()

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,leak -fno-sanitize-recover=all -fsanitize-undefined-trap-on-error -g -O2 -fno-omit-frame-pointer -g")

file(GLOB HEADERS "*.hpp" "model/*.hpp" "csimplesocket/*.h")
SET_SOURCE_FILES_PROPERTIES(${HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
file(GLOB SRC "*.cpp" "model/*.cpp" "csimplesocket/*.cpp")
add_executable(aicup2019 ${HEADERS} ${SRC} Simulation.cpp Simulation.hpp Util.cpp Util.hpp StrategyGenerator.cpp StrategyGenerator.hpp)
TARGET_LINK_LIBRARIES(aicup2019 ${PROJECT_LIBS})