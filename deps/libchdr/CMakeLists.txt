cmake_minimum_required(VERSION 3.9)

project(chdr C)

set(CHDR_VERSION_MAJOR 0)
set(CHDR_VERSION_MINOR 1)

if(CMAKE_PROJECT_NAME STREQUAL "chdr")
  option(BUILD_SHARED_LIBS "Build libchdr also as a shared library" ON)
endif()
option(INSTALL_STATIC_LIBS "Install static libraries" OFF)
option(WITH_SYSTEM_ZLIB "Use system provided zlib library" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fPIC -O3 -flto")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

include(FindPkgConfig)
include(GNUInstallDirs)

# Detect processor type.
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
  set(CPU_ARCH "x64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  # MSVC x86/x64
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CPU_ARCH "x64")
  else()
    set(CPU_ARCH "x86")
  endif()
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386" OR
       ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
  set(CPU_ARCH "x86")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(CPU_ARCH "aarch64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7-a")
  set(CPU_ARCH "arm")
else()
  message(FATAL_ERROR "Unknown system processor: " ${CMAKE_SYSTEM_PROCESSOR})
endif()

#--------------------------------------------------
# dependencies
#--------------------------------------------------


# lzma
add_subdirectory(deps/lzma-19.00 EXCLUDE_FROM_ALL)
  list(APPEND CHDR_LIBS lzma)
  list(APPEND CHDR_INCLUDES lzma)

# zlib
if (WITH_SYSTEM_ZLIB)
  pkg_check_modules(ZLIB REQUIRED zlib)
  list(APPEND PLATFORM_INCLUDES ${ZLIB_INCLUDE_DIRS})
  list(APPEND PLATFORM_LIBS ${ZLIB_LIBRARIES})
else()
  add_subdirectory(deps/zlib-1.2.11 EXCLUDE_FROM_ALL)
  list(APPEND CHDR_LIBS zlib)
  list(APPEND CHDR_INCLUDES zlib)
endif()

#--------------------------------------------------
# chdr
#--------------------------------------------------

set(CHDR_SOURCES
  src/libchdr_bitstream.c
  src/libchdr_cdrom.c
  src/libchdr_chd.c
  src/libchdr_flac.c
  src/libchdr_huffman.c
)

list(APPEND CHDR_INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/include)

add_library(chdr-static STATIC ${CHDR_SOURCES})
target_include_directories(chdr-static PRIVATE ${CHDR_INCLUDES} ${PLATFORM_INCLUDES} PUBLIC include)
target_compile_definitions(chdr-static PRIVATE ${CHDR_DEFS})
target_link_libraries(chdr-static PRIVATE ${CHDR_LIBS} ${PLATFORM_LIBS})

if (INSTALL_STATIC_LIBS)
  install(TARGETS chdr-static ${CHDR_LIBS}
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if (BUILD_SHARED_LIBS)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

  add_library(chdr SHARED ${CHDR_SOURCES})
  target_include_directories(chdr PRIVATE ${CHDR_INCLUDES} ${PLATFORM_INCLUDES} PUBLIC include)
  target_compile_definitions(chdr PRIVATE ${CHDR_DEFS})
  target_link_libraries(chdr PRIVATE ${CHDR_LIBS} ${PLATFORM_LIBS})

  if(MSVC)
    target_compile_definitions(chdr PUBLIC "CHD_DLL")
    target_compile_definitions(chdr PRIVATE "CHD_DLL_EXPORTS")
  elseif(APPLE)
    target_link_options(chdr PRIVATE -Wl,-dead_strip -Wl,-exported_symbol,_chd_*)
  else()
    target_link_options(chdr PRIVATE -Wl,--version-script ${CMAKE_CURRENT_SOURCE_DIR}/src/link.T -Wl,--no-undefined)
  endif()

  set_target_properties(chdr PROPERTIES PUBLIC_HEADER "include/libchdr/bitstream.h;include/libchdr/cdrom.h;include/libchdr/chd.h;include/libchdr/chdconfig.h;include/libchdr/coretypes.h;include/libchdr/flac.h;include/libchdr/huffman.h")
  set_target_properties(chdr PROPERTIES VERSION "${CHDR_VERSION_MAJOR}.${CHDR_VERSION_MINOR}")

  if (CMAKE_BUILD_TYPE MATCHES Release)
    #add_custom_command(TARGET chdr POST_BUILD COMMAND ${CMAKE_STRIP} libchdr.so)
  endif (CMAKE_BUILD_TYPE MATCHES Release)

  install(TARGETS chdr
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libchdr"
  )

  configure_file(pkg-config.pc.in ${CMAKE_BINARY_DIR}/libchdr.pc @ONLY)
  install(FILES ${CMAKE_BINARY_DIR}/libchdr.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

