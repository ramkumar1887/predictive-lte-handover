cmake_minimum_required(VERSION 3.16)
project(predictive_lte_handover LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

add_library(predictive_ho_core STATIC
    src/stock_a3_engine.cpp
    src/predictive_a3_engine.cpp
    src/channel_model.cpp
    src/metrics.cpp
)

target_include_directories(predictive_ho_core PUBLIC include)

add_executable(lte_handover_sim
    src/main.cpp
)

target_link_libraries(lte_handover_sim PRIVATE predictive_ho_core)

# Compiler flags
if(MSVC)
    target_compile_options(predictive_ho_core PRIVATE /W4 /permissive-)
    target_compile_options(lte_handover_sim PRIVATE /W4 /permissive-)
else()
    target_compile_options(predictive_ho_core PRIVATE -Wall -Wextra -O3)
    target_compile_options(lte_handover_sim PRIVATE -Wall -Wextra -O3)
endif()
