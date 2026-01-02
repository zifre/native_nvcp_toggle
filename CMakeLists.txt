cmake_minimum_required(VERSION 3.10)
project(native_nvcp_toggle C)

set(CMAKE_C_STANDARD 11)

# NVAPI SDK path
set(NVAPI_DIR "${CMAKE_SOURCE_DIR}/nvapi")

# Source files
add_executable(native_nvcp_toggle native_nvcp_toggle.c)

# Include directories
target_include_directories(native_nvcp_toggle PRIVATE ${NVAPI_DIR})

# Link libraries
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64-bit
    target_link_libraries(native_nvcp_toggle
        "${NVAPI_DIR}/amd64/nvapi64.lib"
        user32
        gdi32
    )
else()
    # 32-bit
    target_link_libraries(native_nvcp_toggle
        "${NVAPI_DIR}/x86/nvapi.lib"
        user32
        gdi32
    )
endif()

# Copy config file to output directory
add_custom_command(TARGET native_nvcp_toggle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/native_nvcp_config.ini"
    "$<TARGET_FILE_DIR:native_nvcp_toggle>/native_nvcp_config.ini"
)
