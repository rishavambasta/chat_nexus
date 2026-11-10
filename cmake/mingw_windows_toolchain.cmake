# Toolchain configuration for cross-compiling Qt 6 to Windows using MinGW-w64
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW Toolchain on host
set(TOOLCHAIN_PREFIX "/run/media/rishav/DS1/prog/chat/tools/gcc-mingw")
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}/bin/x86_64-w64-mingw32-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}/bin/x86_64-w64-mingw32-g++")
set(CMAKE_RC_COMPILER "${TOOLCHAIN_PREFIX}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}/bin/x86_64-w64-mingw32-ar")
set(CMAKE_RANLIB "${TOOLCHAIN_PREFIX}/bin/x86_64-w64-mingw32-ranlib")

# Qt 6.5.3 for Windows MinGW
set(CMAKE_PREFIX_PATH "/run/media/rishav/DS1/prog/chat/qt_win/6.5.3/mingw_64")
set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/wine")

# Wrappers for Qt code generators
get_filename_component(_CURRENT_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_PROJ_DIR "${_CURRENT_DIR}" PATH)
set(CMAKE_AUTOMOC_EXECUTABLE "${_PROJ_DIR}/tools/wrappers/moc")
set(CMAKE_AUTORCC_EXECUTABLE "${_PROJ_DIR}/tools/wrappers/rcc")
set(CMAKE_AUTOUIC_EXECUTABLE "${_PROJ_DIR}/tools/wrappers/uic")

# Prevent host Linux Vulkan /usr/include from leaking into Windows build
set(Vulkan_INCLUDE_DIR "" CACHE PATH "" FORCE)
set(Vulkan_FOUND FALSE CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders TRUE CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_VulkanHeaders TRUE CACHE BOOL "" FORCE)

set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PREFIX} ${CMAKE_PREFIX_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
