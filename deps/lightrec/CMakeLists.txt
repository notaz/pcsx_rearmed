cmake_minimum_required(VERSION 3.0)
project(lightrec LANGUAGES C VERSION 0.5)

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")
if (NOT BUILD_SHARED_LIBS)
	add_definitions(-DLIGHTREC_STATIC)
endif (NOT BUILD_SHARED_LIBS)

if (NOT LOG_LEVEL)
	set(LOG_LEVEL Info CACHE STRING "Log level" FORCE)
	set_property(CACHE LOG_LEVEL PROPERTY STRINGS NoLog Error Warning Info Debug)
endif()

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
		"Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
		FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS None Debug Release RelWithDebInfo MinSizeRel)
endif()

string(TOUPPER ${LOG_LEVEL} LIGHTREC_LOG_LEVEL)
add_definitions(-DLOG_LEVEL=${LIGHTREC_LOG_LEVEL}_L)

if (CMAKE_COMPILER_IS_GNUCC)
	add_compile_options(-fvisibility=hidden)
endif()

set(HAS_DEFAULT_ELM ${CMAKE_COMPILER_IS_GNUCC})

list(APPEND LIGHTREC_SOURCES
	blockcache.c
	emitter.c
	interpreter.c
	lightrec.c
	memmanager.c
	optimizer.c
	regcache.c
)
list(APPEND LIGHTREC_HEADERS
	blockcache.h
	debug.h
	disassembler.h
	emitter.h
	interpreter.h
	lightrec-private.h
	lightrec.h
	memmanager.h
	optimizer.h
	recompiler.h
	regcache.h
)

option(ENABLE_FIRST_PASS "Run the interpreter as first-pass optimization" ON)

option(ENABLE_THREADED_COMPILER "Enable threaded compiler" ON)
if (ENABLE_THREADED_COMPILER)
	list(APPEND LIGHTREC_SOURCES recompiler.c reaper.c)

	if (NOT ENABLE_FIRST_PASS)
		message(SEND_ERROR "Threaded compiler requires first-pass optimization")
	endif (NOT ENABLE_FIRST_PASS)
endif (ENABLE_THREADED_COMPILER)

option(OPT_REMOVE_DIV_BY_ZERO_SEQ "(optimization) Remove div-by-zero check sequence" ON)
option(OPT_REPLACE_MEMSET "(optimization) Detect and replace memset with host variant" ON)
option(OPT_DETECT_IMPOSSIBLE_BRANCHES "(optimization) Detect impossible branches" ON)
option(OPT_TRANSFORM_OPS "(optimization) Transform opcodes" ON)
option(OPT_LOCAL_BRANCHES "(optimization) Detect local branches" ON)
option(OPT_SWITCH_DELAY_SLOTS "(optimization) Switch delay slots" ON)
option(OPT_FLAG_STORES "(optimization) Flag stores that don't require invalidation" ON)
option(OPT_FLAG_IO "(optimization) Flag I/O opcodes whose target is known" ON)
option(OPT_FLAG_MULT_DIV "(optimization) Flag MULT/DIV that only use one of HI/LO" ON)
option(OPT_EARLY_UNLOAD "(optimization) Unload registers early" ON)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(${PROJECT_NAME} ${LIGHTREC_SOURCES} ${LIGHTREC_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	PUBLIC_HEADER lightrec.h
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	C_EXTENSIONS OFF
)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wno-parentheses)
endif()
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE -Wno-initializer-overrides)
endif()

option(ENABLE_TINYMM "Enable optional libtinymm dependency" OFF)
if (ENABLE_TINYMM)
	find_library(TINYMM_LIBRARIES tinymm REQUIRED)
	find_path(TINYMM_INCLUDE_DIR tinymm.h REQUIRED)

	include_directories(${TINYMM_INCLUDE_DIR})
	target_link_libraries(${PROJECT_NAME} PRIVATE ${TINYMM_LIBRARIES})
endif (ENABLE_TINYMM)

if (ENABLE_THREADED_COMPILER)
	find_library(PTHREAD_LIBRARIES pthread REQUIRED)
	find_path(PTHREAD_INCLUDE_DIR pthread.h REQUIRED)

	include_directories(${PTHREAD_INCLUDE_DIR})
	target_link_libraries(${PROJECT_NAME} PRIVATE ${PTHREAD_LIBRARIES})
endif (ENABLE_THREADED_COMPILER)

find_library(LIBLIGHTNING lightning REQUIRED)
find_path(LIBLIGHTNING_INCLUDE_DIR lightning.h REQUIRED)

include_directories(${LIBLIGHTNING_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBLIGHTNING})

if (LOG_LEVEL STREQUAL Debug)
	set(ENABLE_DISASSEMBLER ON)
	target_sources(${PROJECT_NAME} PRIVATE disassembler.c)
endif()

configure_file(lightrec-config.h.cmakein lightrec-config.h @ONLY)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
