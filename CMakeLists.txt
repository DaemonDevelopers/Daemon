# Daemon BSD Source Code
# Copyright (c) 2013-2016, Daemon Developers
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of the <organization> nor the
#    names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required (VERSION 3.12)

# Choose Python versions by location instead of getting whatever is in PYTHON_EXECUTABLE
# Can be removed once we require CMake 3.15
if (POLICY CMP0094)
    cmake_policy(SET CMP0094 NEW)
endif()

if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
get_directory_property(DAEMON_PARENT_SCOPE_DIR PARENT_DIRECTORY)
if (DAEMON_PARENT_SCOPE_DIR)
    # Append Daemon module path to parent project's one only when Daemon is subproject.
    # See https://stackoverflow.com/questions/25199677/how-to-detect-if-current-scope-has-a-parent-in-cmake
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake PARENT_SCOPE)
endif()

include(CMakeDependentOption)

# This line has to appear before 'PROJECT' in order to be able to disable incremental linking
set(MSVC_INCREMENTAL_DEFAULT ON)
# Enable the creation of project folders for Visual Studio projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Default to Release builds. To use system CFLAGS only (for distro builds), set CMAKE_BUILD_TYPE to None
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if (Daemon_OUT)
    set(CMAKE_CURRENT_BINARY_DIR ${Daemon_OUT})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Daemon_OUT})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Daemon_OUT})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Daemon_OUT})
endif()

project(Daemon C CXX)

################################################################################
# Configuration options
################################################################################

if(NOT DAEMON_EXTERNAL_APP)
    option(BUILD_CLIENT "Engine client executable, required to play" ON)
    option(BUILD_SERVER "Engine server executable, required to host servers" ON)

    ## About the different ways to host/play games:
    ## Native DLL: no sandboxing, no cleaning up but debugger support. Use for dev.
    ## NaCl exe: sandboxing, no leaks, slightly slower, hard to debug. Use for regular players.
    ## Native exe: no sandboxing, no leaks, hard to debug. Might be used by server owners for perf.
    ## see VirtualMachine.h for code

    # can be loaded by daemon with vm.[sc]game.type 0 or 1
    # TODO (maybe): those could probably be merged as a single option "BUILD_DISTRIBUTED_FILES"
    option(BUILD_GAME_NACL "Build the \"pexe\" files, required to build the \"nexe\" files which are required to host servers." OFF)
    option(BUILD_GAME_NACL_NEXE "Build the \"nexe\" files from the \"pexe\" files, required to host servers." OFF)

    # can be loaded by daemon with vm.[sc]game.type 3
    option(BUILD_GAME_NATIVE_DLL "Build the shared library files, mostly useful for debugging changes locally." ON)

    # can be loaded by daemon with vm.[sc]game.type 2
    option(BUILD_GAME_NATIVE_EXE "Build native executable, which might be used for better performances by server owners" OFF)

    if (CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        set(BUILD_GAME_NACL ON)
        set(BUILD_GAME_NACL_NEXE ON)
    endif()
    option(BUILD_TTY_CLIENT "Engine client with no graphical display" ON)
    option(BUILD_DUMMY_APP "Stripped-down engine executable, mostly used to ease incremental porting and debugging" OFF)

    set(NACL_RUNTIME_PATH "" CACHE STRING "Directory containing the NaCl binaries")

    # Not supported on mac because the included version is too old
    if(APPLE)
        set(USE_CURSES OFF)
    elseif(WIN32)
        set(USE_CURSES ON)
    else()
        option(USE_CURSES "Enable fancy colors in terminal's output" ON)
    endif()
    cmake_dependent_option(USE_SMP "Compile with support for running the renderer in a separate thread" 1 BUILD_CLIENT 0)
    cmake_dependent_option(USE_GEOIP "Allows server to show from which coutry a player logs from" 1 "BUILD_SERVER OR BUILD_CLIENT OR BUILD_TTY_CLIENT" 0)
    option(USE_BREAKPAD "Generate Daemon crash dumps (which require Breakpad tools to read)" OFF)
endif()

option(USE_LTO "Use link-time optimization for release builds" OFF)
cmake_dependent_option(USE_SLIM_LTO "Generate slim LTO objects, improves build times" 1 "USE_LTO AND \"${CMAKE_CXX_COMPILER_ID}\" STREQUAL GNU" 0)
option(USE_HARDENING "Use stack protection and other hardening flags" OFF)
option(USE_WERROR "Tell the compiler to make the build fail when warnings are present" OFF)
option(USE_PEDANTIC "Tell the compiler to be pedantic" OFF)
option(USE_DEBUG_OPTIMIZE "Try to optimize the debug build" ON)

if (APPLE)
    # Turn off by default because it is reportedly broken
    option(USE_PRECOMPILED_HEADER "Improve build times by using a precompiled header" OFF)
else()
    option(USE_PRECOMPILED_HEADER "Improve build times by using a precompiled header" ON)
endif()
option(USE_ADDRESS_SANITIZER "Try to use the address sanitizer" OFF)
option(BE_VERBOSE "Tell the compiler to report all warnings" OFF)
option(USE_STATIC_LIBS "Tries to use static libs where possible. Only works for Linux" OFF)

# Game VM modules are built with a recursive invocation of CMake, by which all the configuration
# options are lost, except ones we explicitly choose to pass.
set(DEFAULT_NACL_VM_INHERITED_OPTIONS
    BE_VERBOSE
    BUILD_CGAME
    BUILD_SGAME
    CMAKE_BUILD_TYPE
    DAEMON_CBSE_PYTHON_PATH
    USE_PEDANTIC
    USE_PRECOMPILED_HEADER
    USE_WERROR
)
set(NACL_VM_INHERITED_OPTIONS "${DEFAULT_NACL_VM_INHERITED_OPTIONS}" CACHE STRING
    "Semicolon-separated list of options for which NaCl game VMs should use the same value as the other binaries")
mark_as_advanced(NACL_VM_INHERITED_OPTIONS)

if (BE_VERBOSE)
    set(WARNMODE "no-error=")
else()
    set(WARNMODE "no-")
endif()
if (DAEMON_PARENT_SCOPE_DIR)
    set(WARNMODE ${WARNMODE} PARENT_SCOPE)
endif()

include(DaemonPlatform)

################################################################################
# Directories
################################################################################

# Dependencies version, this must match the number in external_deps/build.sh
set(DEPS_VERSION 5)

set(MOUNT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
if (DAEMON_PARENT_SCOPE_DIR)
    set(MOUNT_DIR ${MOUNT_DIR} PARENT_SCOPE)
endif()
include_directories(${MOUNT_DIR})
set(OBJ_DIR ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY})
if (DAEMON_PARENT_SCOPE_DIR)
    set(OBJ_DIR ${OBJ_DIR} PARENT_SCOPE)
endif()
set(ENGINE_DIR ${MOUNT_DIR}/engine)
if (DAEMON_PARENT_SCOPE_DIR)
    set(ENGINE_DIR ${ENGINE_DIR} PARENT_SCOPE)
endif()
set(COMMON_DIR ${MOUNT_DIR}/common)
if (DAEMON_PARENT_SCOPE_DIR)
    set(COMMON_DIR ${COMMON_DIR} PARENT_SCOPE)
endif()
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
if (DAEMON_PARENT_SCOPE_DIR)
    set(LIB_DIR ${LIB_DIR} PARENT_SCOPE)
endif()
include_directories(${LIB_DIR})
if (CMAKE_CFG_INTDIR STREQUAL ".")
    set(FULL_OUTPUT_DIR ${CMAKE_BINARY_DIR})
else()
    set(FULL_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR})
endif()
set(EXTERNAL_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external_deps" CACHE STRING "Directory in which to store the downloaded dependencies.")
if (MSVC)
    set(DEPS_EXT ".zip")
    if (ARCH STREQUAL "x86_64")
        set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/msvc64-${DEPS_VERSION})
    else()
        set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/msvc32-${DEPS_VERSION})
    endif()
elseif (WIN32)
    set(DEPS_EXT ".zip")
    if (ARCH STREQUAL "x86_64")
        set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/mingw64-${DEPS_VERSION})
    else()
        set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/mingw32-${DEPS_VERSION})
    endif()
elseif (APPLE)
    set(DEPS_EXT ".tar.bz2")
    set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/macosx64-${DEPS_VERSION})
elseif (LINUX)
    set(DEPS_EXT ".tar.bz2")
    # Our minimal NaCl .debs put the files in /usr/lib/nacl, so check that first
    if (EXISTS "/usr/lib/nacl/nacl_loader")
        set(DEPS_DIR "/usr/lib/nacl")
    else()
        set(DEPS_DIR ${EXTERNAL_DEPS_DIR}/linux64-${DEPS_VERSION})
    endif()
endif()
if (DAEMON_PARENT_SCOPE_DIR)
    set(DEPS_DIR ${DEPS_DIR} PARENT_SCOPE)
endif()

################################################################################
# Compile and link flags
################################################################################

include(DaemonFlags)

# Warning options
# keep
try_flag(WARNINGS           "-Wno-pragmas")
try_flag(WARNINGS           "-Wno-unknown-pragmas")
try_flag(WARNINGS           "-W${WARNMODE}old-style-cast")
try_flag(WARNINGS           "-Woverloaded-virtual")
try_flag(WARNINGS           "-Wstrict-null-sentinel")
try_flag(WARNINGS           "-Wno-error=sign-promo")
try_flag(WARNINGS           "-Wno-error=extern-c-compat")
# remove
try_flag(WARNINGS           "-Wno-error=maybe-uninitialized")
try_flag(WARNINGS           "-W${WARNMODE}missing-field-initializers")
try_flag(WARNINGS           "-W${WARNMODE}sign-compare")
try_flag(WARNINGS           "-Wno-error=switch")
try_flag(WARNINGS           "-Wno-error=unused-but-set-parameter")
try_flag(WARNINGS           "-Wno-error=unused-but-set-variable")
try_flag(WARNINGS           "-Wno-error=unused-function")
try_flag(WARNINGS           "-Wno-error=unused-parameter")
try_flag(WARNINGS           "-Wno-error=unused-variable")
try_flag(WARNINGS           "-Wno-error=format-truncation")
try_flag(WARNINGS           "-Wno-error=deprecated-copy") # TODO: Fix these errors.

################################################################################
# Git revision info
################################################################################

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_package(Git)
    if (GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --long --match v* --dirty=+dirty
            OUTPUT_VARIABLE GIT_DESCRIBE_REPORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        message(STATUS "git reported ${GIT_DESCRIBE_REPORT}")
        # this may fail with annotated non-release tags
        if (GIT_DESCRIBE_REPORT MATCHES "-0-g.......$")
            set(GIT_DESCRIBE_REPORT)
        endif()
    endif()
endif()

if (GIT_DESCRIBE_REPORT)
    set(DESIRED_REVISION_H_CONTENTS "#define REVISION \"${GIT_DESCRIBE_REPORT}\"\n")
endif()

if (EXISTS "${OBJ_DIR}/revision.h")
    file(READ "${OBJ_DIR}/revision.h" ACTUAL_REVISION_H_CONTENTS)
    if (NOT "${ACTUAL_REVISION_H_CONTENTS}" STREQUAL "${DESIRED_REVISION_H_CONTENTS}")
        file(WRITE "${OBJ_DIR}/revision.h" "${DESIRED_REVISION_H_CONTENTS}")
    endif()
else()
    file(WRITE "${OBJ_DIR}/revision.h" "${DESIRED_REVISION_H_CONTENTS}")
endif()

include_directories("${OBJ_DIR}")

################################################################################
# Group the sources by folder to have folder show in Visual Studio
################################################################################

macro(group_dir_sources dir)
    file(GLOB_RECURSE files ${dir}/*)
    string(LENGTH ${dir}/ dir_length)
    foreach(file ${files})
        if (NOT IS_DIRECTORY ${dir}/${file})
            get_filename_component(group_name ${file} DIRECTORY)
            string(SUBSTRING ${group_name} ${dir_length} -1 group_name)
            string(REPLACE "/" "\\" group_name ${group_name})
            source_group(${group_name} FILES ${file})
        endif()
    endforeach()
endmacro()

if (MSVC)
    group_dir_sources(${MOUNT_DIR})
endif()

################################################################################
# Support for precompiled headers
################################################################################

# MSVC requires that an extra file be added to a project
if (USE_PRECOMPILED_HEADER AND MSVC)
    file(WRITE ${OBJ_DIR}/PrecompiledHeader.cpp "")
    set(PCH_FILE ${OBJ_DIR}/PrecompiledHeader.cpp)
    if (DAEMON_PARENT_SCOPE_DIR)
        set(PCH_FILE ${PCH_FILE} PARENT_SCOPE)
    endif()
endif()

function(ADD_PRECOMPILED_HEADER Target)
    if (NOT USE_PRECOMPILED_HEADER OR CMAKE_VERSION VERSION_LESS 2.8.10)
        return()
    endif()

    # Get the common compile flags
    set(Flags ${CMAKE_CXX_FLAGS} ${WARNINGS})
    get_target_property(Type ${Target} TYPE)
    if (Type STREQUAL MODULE_LIBRARY)
        set(Flags ${Flags} ${CMAKE_SHARED_MODULE_CXX_FLAGS})
    endif()
    separate_arguments(Flags)

    # Get the per-configuration compile flags
    foreach(Config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${Config} CONFIG)
        set(ConfigFlags ${CMAKE_CXX_FLAGS_${CONFIG}})
        separate_arguments(ConfigFlags)
        foreach(Flag ${ConfigFlags})
            set(Flags ${Flags} $<$<CONFIG:${Config}>:${Flag}>)
        endforeach()
    endforeach()

    # Get preprocessor options for the target and directory (global)
    get_directory_property(DirCompileDefs COMPILE_DEFINITIONS)
    get_directory_property(DirIncludeDirs INCLUDE_DIRECTORIES)
    get_target_property(TargetCompileDefs ${Target} COMPILE_DEFINITIONS)
    get_target_property(TargetIncludeDirs ${Target} INCLUDE_DIRECTORIES)
    set(Defs)
    foreach(Def ${TargetCompileDefs})
        set(Defs ${Defs} -D${Def})
    endforeach()
    foreach(Def ${DirCompileDefs})
        set(Defs ${Defs} -D${Def})
    endforeach()
    foreach(Def ${DirIncludeDirs})
        set(Defs ${Defs} -I${Def})
    endforeach()
    foreach(Def ${TargetIncludeDirs})
        set(Defs ${Defs} -I${Def})
    endforeach()

    # Specify minimum OSX version
    if (APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
        set(Flags ${Flags} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()

    set(Header ${COMMON_DIR}/Common.h)

    # Compiler-specific PCH support
    if (CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR NACL)
        add_custom_command(OUTPUT "${OBJ_DIR}/${Target}.h.gch"
            COMMAND ${PNACLPYTHON_PREFIX2} ${CMAKE_CXX_COMPILER} ${Defs} ${Flags} -x c++-header ${Header} -o "${OBJ_DIR}/${Target}.h.gch"
            DEPENDS ${Header}
            IMPLICIT_DEPENDS CXX ${Header}
        )

        add_custom_target(${Target}-pch DEPENDS "${OBJ_DIR}/${Target}.h.gch")
        add_dependencies(${Target} ${Target}-pch)

        # PNaCl clang doesn't support -include-pch properly
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT NACL)
            set_property(TARGET ${Target} APPEND PROPERTY COMPILE_OPTIONS "-include-pch;${OBJ_DIR}/${Target}.h.gch")
        else()
            set_property(TARGET ${Target} APPEND PROPERTY COMPILE_OPTIONS "-include;${OBJ_DIR}/${Target}.h;-Winvalid-pch")
        endif()
    elseif (MSVC)
        # /Fp sets the PCH path used by either of the /Yc and /Yu options.
        # /Yc overrides /Yu.
        set_source_files_properties(${PCH_FILE} PROPERTIES COMPILE_FLAGS "/Yc${Header}")
        target_compile_options(${Target} PRIVATE "/Yu${Header}" "/Fp${OBJ_DIR}/${Target}_$<CONFIG>.pch" "/FI${Header}")
    endif()
endfunction()

################################################################################
# File lists
################################################################################

include(srclibs.cmake)
include(src.cmake)

################################################################################
# Libraries
################################################################################

# Import external dependencies
if (DEPS_DIR)
    # Download them if they not available
    if (NOT EXISTS ${DEPS_DIR})
        file(MAKE_DIRECTORY ${EXTERNAL_DEPS_DIR})
        get_filename_component(BASENAME ${DEPS_DIR} NAME)
        set(REMOTE "https://dl.unvanquished.net/deps/${BASENAME}${DEPS_EXT}")
        message(STATUS "Downloading dependencies from '${REMOTE}'")
        file(DOWNLOAD ${REMOTE} ${OBJ_DIR}/${BASENAME}${DEPS_EXT}
            SHOW_PROGRESS
            STATUS DOWNLOAD_RESULT
            LOG DOWNLOAD_LOG
        )
        list(GET DOWNLOAD_RESULT 0 DOWNLOAD_STATUS)
        list(GET DOWNLOAD_RESULT 1 DOWNLOAD_STRING)
        if (NOT DOWNLOAD_STATUS EQUAL 0)
            message(FATAL_ERROR "Error downloading '${REMOTE}':
                Status code: ${DOWNLOAD_STATUS}
                Error string: ${DOWNLOAD_STRING}
                Download log: ${DOWNLOAD_LOG}"
            )
        endif()
        message(STATUS "Download completed successfully")

        # Extract the downloaded archive
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf ${OBJ_DIR}/${BASENAME}${DEPS_EXT}
            WORKING_DIRECTORY ${EXTERNAL_DEPS_DIR}
            RESULT_VARIABLE EXTRACT_RESULT
        )
        if (NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Could not extract ${BASENAME}${DEPS_EXT}")
        endif()
    endif()

    # Add to paths
    set(CMAKE_FIND_ROOT_PATH ${DEPS_DIR} ${CMAKE_FIND_ROOT_PATH})
    set(CMAKE_INCLUDE_PATH ${DEPS_DIR} ${DEPS_DIR}/include ${CMAKE_INCLUDE_PATH})
    set(CMAKE_FRAMEWORK_PATH ${DEPS_DIR} ${CMAKE_FRAMEWORK_PATH})
    set(CMAKE_LIBRARY_PATH ${DEPS_DIR}/lib ${CMAKE_LIBRARY_PATH})
    if (DAEMON_PARENT_SCOPE_DIR)
        # Also set parent scope so the top level CMakeLists can find precompiled deps
        set(CMAKE_FIND_ROOT_PATH ${DEPS_DIR} ${CMAKE_FIND_ROOT_PATH} PARENT_SCOPE)
        set(CMAKE_INCLUDE_PATH ${DEPS_DIR} ${DEPS_DIR}/include ${CMAKE_INCLUDE_PATH} PARENT_SCOPE)
        set(CMAKE_FRAMEWORK_PATH ${DEPS_DIR} ${CMAKE_FRAMEWORK_PATH} PARENT_SCOPE)
        set(CMAKE_LIBRARY_PATH ${DEPS_DIR}/lib ${CMAKE_LIBRARY_PATH} PARENT_SCOPE)
    endif()

    # Force OpenAL Soft on Mac, otherwise the system framework will be selected
    if (APPLE)
        set(OPENAL_INCLUDE_DIR ${DEPS_DIR}/include/AL)
        set(OPENAL_LIBRARY ${DEPS_DIR}/lib/libopenal.dylib)
    endif()
endif()

# These variables are defined:
# LIBS_BASE, LIBS_ENGINE_BASE LIBS_ENGINE, LIBS_BASECLIENT, LIBS_CLIENT

# Native client
include(DaemonNacl)
if (NACL)
    add_library(srclibs-nacl-module EXCLUDE_FROM_ALL ${NACLLIST_MODULE})
    set_target_properties(srclibs-nacl-module PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
    set(LIBS_BASE ${LIBS_BASE} srclibs-nacl-module)
else()
    add_library(srclibs-nacl-native EXCLUDE_FROM_ALL ${NACLLIST_NATIVE})
    set_target_properties(srclibs-nacl-native PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
    if (APPLE)
        # Do not error for OSAtomic* deprecation notices
        target_compile_options(srclibs-nacl-native PRIVATE "-Wno-error=deprecated-declarations")
    endif()
    set(LIBS_BASE ${LIBS_BASE} srclibs-nacl-native)
endif()

# Base OS libs
if (WIN32)
    set(LIBS_BASE ${LIBS_BASE} winmm ws2_32)
elseif (NACL)
    find_library(NACL_EXCEPTION nacl_exception)
    find_library(NACL_MINIDUMP minidump_generator)
    set(LIBS_BASE ${LIBS_BASE} ${NACL_MINIDUMP} ${NACL_EXCEPTION} )
else()
    find_library(LIBM m)
    if (LIBM)
        set(LIBS_BASE ${LIBS_BASE} ${LIBM})
    endif()
    find_library(LIBRT rt)
    if (LIBRT)
        set(LIBS_BASE ${LIBS_BASE} ${LIBRT})
    endif()
    find_library(LIBICONV iconv)
    if (LIBICONV)
        set(LIBS_ENGINE ${LIBS_ENGINE} ${LIBICONV})
    endif()
    mark_as_advanced(LIBM LIBRT LIBICONV)
    set(LIBS_BASE ${LIBS_BASE} ${CMAKE_DL_LIBS})
    find_package(Threads REQUIRED)
    set(LIBS_BASE ${LIBS_BASE} ${CMAKE_THREAD_LIBS_INIT})
    if (APPLE)
        set(LIBS_CLIENT ${LIBS_CLIENT} "-framework Carbon" "-framework IOKit" "-framework Cocoa")
    endif()
endif()

# Minizip
add_library(srclibs-minizip EXCLUDE_FROM_ALL ${MINIZIPLIST})
set_target_properties(srclibs-minizip PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
set(LIBS_BASE ${LIBS_BASE} srclibs-minizip)

# Look for OpenGL here before we potentially switch to looking for static libs.
if (BUILD_CLIENT)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    set(LIBS_CLIENT ${LIBS_CLIENT} OpenGL::GL)
endif()

# Prefer static libs
if ( USE_STATIC_LIBS )
    if ( LINUX )
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    endif()
endif()


# zlib
if (NOT NACL)
    find_package(ZLIB REQUIRED)
else()
    add_library(srclibs-zlib EXCLUDE_FROM_ALL ${ZLIBLIST})
    set_target_properties(srclibs-zlib PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
    set(ZLIB_INCLUDE_DIRS ${LIB_DIR}/zlib)
    set(ZLIB_LIBRARIES srclibs-zlib)
endif()
set(LIBS_BASE ${LIBS_BASE} ${ZLIB_LIBRARIES})
include_directories(${ZLIB_INCLUDE_DIRS})

if (DAEMON_PARENT_SCOPE_DIR)
    set(LIBS_BASE ${LIBS_BASE} PARENT_SCOPE)
endif()

# Curses, pdcurses on Windows and ncursesw on Unix
if (USE_CURSES AND NOT NACL)
    if (WIN32)
        set(LIBS_ENGINE_BASE ${LIBS_ENGINE_BASE} gdi32 comdlg32)

        add_definitions(-DPDC_WIDE -DPDC_FORCE_UTF8 -DUSE_CURSES)
        add_library(srclibs-pdcurses EXCLUDE_FROM_ALL ${PDCURSESLIST})
        set_target_properties(srclibs-pdcurses PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
        set(LIBS_ENGINE_BASE ${LIBS_ENGINE_BASE} srclibs-pdcurses)
        include_directories(${LIB_DIR}/pdcurses)
    else ()
        add_definitions(-DUSE_CURSES)
        set(CURSES_NEED_NCURSES 1) # Tells FindCurses that ncurses is required
        find_package(CursesW REQUIRED)
        set(LIBS_ENGINE_BASE ${LIBS_ENGINE_BASE} ${CURSESW_LIBRARIES})
        include_directories(${CURSESW_INCLUDE_DIR})
    endif()
endif()

if (BUILD_CLIENT OR BUILD_TTY_CLIENT OR BUILD_SERVER)
    if (NACL_RUNTIME_PATH)
        add_definitions("-DNACL_RUNTIME_PATH=${NACL_RUNTIME_PATH}")
    endif()

    # Tinygettext
    add_library(srclibs-tinygettext EXCLUDE_FROM_ALL ${TINYGETTEXTLIST})
    set_target_properties(srclibs-tinygettext PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
    set(LIBS_ENGINE ${LIBS_ENGINE} srclibs-tinygettext)

    # GeoIP
    if (USE_GEOIP)
        find_package(GeoIP REQUIRED)
        add_definitions(-DHAVE_GEOIP)
        set(LIBS_ENGINE ${LIBS_ENGINE} ${GeoIP_LIBRARIES})
        include_directories(${GeoIP_INCLUDE_DIRS})
    endif()

    # Nettle
    find_package(Nettle REQUIRED)
    include_directories(${NETTLE_INCLUDE_DIR})
    set(LIBS_ENGINE ${LIBS_ENGINE} ${NETTLE_LIBRARIES})

    # GMP
    find_package(GMP REQUIRED)
    include_directories(${GMP_INCLUDE_DIR})
    set(LIBS_ENGINE ${LIBS_ENGINE} ${GMP_LIBRARIES})
endif()

if (BUILD_CLIENT OR BUILD_TTY_CLIENT)
    # CURL
    find_package(CURL REQUIRED)
    include_directories(${CURL_INCLUDE_DIRS})
    set(LIBS_CLIENTBASE ${LIBS_CLIENTBASE} ${CURL_LIBRARIES})

    # Mumble link
    add_library(srclibs-mumblelink EXCLUDE_FROM_ALL ${MUMBLELINKLIST})
    set_target_properties(srclibs-mumblelink PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
    set(LIBS_CLIENTBASE ${LIBS_CLIENTBASE} srclibs-mumblelink)
endif()

# SDL, required for all targets on win32 because of iconv and SDL_SetHint(SDL_TIMER_RESOLUTION, 0)
if (BUILD_CLIENT OR WIN32)
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIR})
    if (WIN32)
        set(LIBS_ENGINE_BASE ${LIBS_ENGINE_BASE} ${SDL2_LIBRARY})
    else()
        set(LIBS_CLIENT ${LIBS_CLIENT} ${SDL2_LIBRARY})
    endif()

    # Used by tinygettext to use SDL_iconv instead of base iconv
    if (WIN32)
        add_definitions(-DHAVE_SDL)
    endif()
    mark_as_advanced(SDL2MAIN_LIBRARY SDL2_LIBRARY SDL2_INCLUDE_DIR)
endif()

# Breakpad
if (USE_BREAKPAD)
    add_definitions(-DUSE_BREAKPAD)
    include_directories(${LIB_DIR}/breakpad/src)

    if (WIN32)
        add_library(srclibs-breakpad-common EXCLUDE_FROM_ALL ${BREAKPAD_COMMON_LIST})
        add_library(srclibs-breakpad-crash_generation_server EXCLUDE_FROM_ALL ${BREAKPAD_CRASHGENERATIONSERVER_LIST})
        add_library(srclibs-breakpad-exception_handler EXCLUDE_FROM_ALL ${BREAKPAD_EXCEPTIONHANDLER_LIST})
        add_library(srclibs-breakpad-crash_generation_client EXCLUDE_FROM_ALL ${BREAKPAD_CRASHGENERATIONCLIENT_LIST})

        set(BREAKPAD_LIBRARIES
            srclibs-breakpad-common
            srclibs-breakpad-crash_generation_server
            srclibs-breakpad-exception_handler
            srclibs-breakpad-crash_generation_client
        )
        foreach(breaklib ${BREAKPAD_LIBRARIES})
            set_target_properties(${breaklib} PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")

            # Breakpad library assumes wide char versions of Windows API functions
            target_compile_definitions(${breaklib} PRIVATE UNICODE)

            target_compile_definitions(${breaklib} PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
        endforeach(breaklib)

        # WIN32 option prevents a console from being created for the process
        add_executable(crash_server WIN32 src/engine/crash_server/crash_server_windows.cpp)
        target_link_libraries(crash_server
            srclibs-breakpad-crash_generation_server srclibs-breakpad-common)
        target_compile_definitions(crash_server PRIVATE UNICODE)

        set(LIBS_ENGINE_BASE ${LIBS_ENGINE_BASE} srclibs-breakpad-exception_handler
            srclibs-breakpad-crash_generation_client srclibs-breakpad-common)

    elseif (LINUX)
        add_library(srclibs-breakpad EXCLUDE_FROM_ALL ${BREAKPAD_LIST})
        set_target_properties(srclibs-breakpad PROPERTIES POSITION_INDEPENDENT_CODE 1 FOLDER "libs")
        set(LIBS_ENGINE ${LIBS_ENGINE} srclibs-breakpad)

        add_executable(crash_server src/engine/crash_server/crash_server_linux.cpp)
        target_link_libraries(crash_server srclibs-breakpad)
    endif()
endif()

if (BUILD_CLIENT)
    find_package(Ogg REQUIRED)
    include_directories(${OGG_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${OGG_LIBRARIES})

    find_package(Vorbis REQUIRED)
    include_directories(${VORBIS_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${VORBIS_LIBRARIES})

    find_package(Opus REQUIRED)
    include_directories(${OPUS_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${OPUS_LIBRARIES})

    find_package(WebP REQUIRED)
    include_directories(${WEBP_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${WEBP_LIBRARIES})

    find_package(JPEG REQUIRED)
    include_directories(${JPEG_INCLUDE_DIR})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${JPEG_LIBRARIES})

    find_package(PNG REQUIRED)
    include_directories(${PNG_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${PNG_LIBRARIES})

    find_package(Freetype REQUIRED)
    include_directories(${FREETYPE_INCLUDE_DIRS})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${FREETYPE_LIBRARIES})

    find_package(GLEW REQUIRED)
    include_directories(${GLEW_INCLUDE_DIR})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${GLEW_LIBRARIES})

    find_package(OpenAL REQUIRED)
    include_directories(${OPENAL_INCLUDE_DIR})
    set(LIBS_CLIENT ${LIBS_CLIENT} ${OPENAL_LIBRARY})
endif()

################################################################################
# Engine
################################################################################
function(AddApplication)
    set(oneValueArgs Target ExecutableName)
    set(multiValueArgs Definitions Flags Files Libs)
    cmake_parse_arguments(A "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${A_Target} ${A_Files} ${PCH_FILE})
    target_link_libraries(${A_Target} engine-lib ${A_Libs})

    set_property(TARGET ${A_Target} APPEND PROPERTY COMPILE_OPTIONS ${A_Flags})
    set_property(TARGET ${A_Target} APPEND PROPERTY INCLUDE_DIRECTORIES ${ENGINE_DIR} ${MOUNT_DIR} ${LIB_DIR})
    set_property(TARGET ${A_Target} APPEND PROPERTY COMPILE_DEFINITIONS ${A_Definitions})
    set_target_properties(${A_Target} PROPERTIES OUTPUT_NAME "${A_ExecutableName}" PREFIX "" FOLDER "engine")

    # Append Windows specific manifests.
    if (WIN32)
        set(WINDOWS_MANIFESTS_DIR ${ENGINE_DIR}/sys/manifests)
        target_sources(${A_Target} PRIVATE
            # App is DPI aware, so Windows doesn't try to scale UI.
            ${WINDOWS_MANIFESTS_DIR}/dpi-aware.manifest
            # App supports Windows Vista+, no quirks required.
            ${WINDOWS_MANIFESTS_DIR}/supported-os.manifest
        )
    endif()

    message(STATUS ${A_Target})
    ADD_PRECOMPILED_HEADER(${A_Target})
endfunction()

if (NOT NACL)
    add_library(engine-lib EXCLUDE_FROM_ALL ${PCH_FILE} ${COMMONLIST} ${ENGINELIST})
    target_link_libraries(engine-lib ${LIBS_BASE} ${LIBS_ENGINE_BASE})
    set_property(TARGET engine-lib APPEND PROPERTY COMPILE_DEFINITIONS BUILD_ENGINE)
    set_property(TARGET engine-lib APPEND PROPERTY INCLUDE_DIRECTORIES ${ENGINE_DIR} ${MOUNT_DIR} ${LIB_DIR})
    set_property(TARGET engine-lib APPEND PROPERTY COMPILE_OPTIONS ${WARNINGS})
    ADD_PRECOMPILED_HEADER(engine-lib)

    if (BUILD_DUMMY_APP)
        AddApplication(
            Target dummyapp
            ExecutableName dummyapp
            Definitions USELESS_DEFINITION_TO_AVOID_PCH_ISSUE
            Flags ${WARNINGS}
            Files ${ENGINE_DIR}/null/NullApplication.cpp
        )
    endif()
endif()

if (BUILD_CLIENT)
    set(Definitions
        BUILD_ENGINE BUILD_GRAPHICAL_CLIENT
    )
    if (USE_SMP)
        list(APPEND Definitions USE_SMP)
    endif()
    AddApplication(
        Target client
        ExecutableName daemon
        Definitions ${Definitions}
        Flags ${WARNINGS}
        Files WIN32 ${WIN_RC} ${QCOMMONLIST} ${SERVERLIST} ${CLIENTBASELIST} ${CLIENTLIST}
        Libs ${LIBS_CLIENT} ${LIBS_CLIENTBASE} ${LIBS_ENGINE}
    )

    # generate glsl include files
    set(GLSL_SOURCE_DIR ${ENGINE_DIR}/renderer/glsl_source)
    set(EMBED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/embed_data)
    file(MAKE_DIRECTORY ${EMBED_INCLUDE_DIR})
    set_property(TARGET client APPEND PROPERTY INCLUDE_DIRECTORIES ${EMBED_INCLUDE_DIR})

    foreach(res ${GLSLSOURCELIST})
        get_filename_component(filename_no_ext ${res} NAME_WE)
        set(outpath ${EMBED_INCLUDE_DIR}/${filename_no_ext}.glsl.h)
        add_custom_command(
            OUTPUT ${outpath}
            COMMAND ${CMAKE_COMMAND} "-DINPUT_FILE=${res}" "-DOUTPUT_FILE=${outpath}"
                "-DVARIABLE_NAME=${filename_no_ext}_glsl" -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedText.cmake
            MAIN_DEPENDENCY ${res}
        )
        set_property(TARGET client APPEND PROPERTY SOURCES ${outpath})
    endforeach()
endif()

if (BUILD_SERVER)
    AddApplication(
        Target server
        ExecutableName daemonded
        Definitions BUILD_ENGINE BUILD_SERVER
        Flags ${WARNINGS}
        Files WIN32 ${WIN_RC} ${QCOMMONLIST} ${SERVERLIST} ${DEDSERVERLIST}
        Libs ${LIBS_ENGINE}
    )
endif()

if (BUILD_TTY_CLIENT)
    AddApplication(
        Target ttyclient
        ExecutableName daemon-tty
        Definitions BUILD_ENGINE BUILD_TTY_CLIENT
        Flags ${WARNINGS}
        Files WIN32 ${WIN_RC} ${QCOMMONLIST} ${SERVERLIST} ${CLIENTBASELIST} ${TTYCLIENTLIST}
        Libs ${LIBS_CLIENTBASE} ${LIBS_ENGINE}
    )
endif()

################################################################################
# Runtime dependencies
################################################################################

if (BUILD_CLIENT OR BUILD_SERVER OR BUILD_TTY_CLIENT)
    add_custom_target(runtime_deps)
    set_target_properties(runtime_deps PROPERTIES FOLDER "CMakePlumbing")

    # The NaCl loader and IRT are required to load .nexe files
    add_custom_command(TARGET runtime_deps PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DEPS_DIR}/sel_ldr${CMAKE_EXECUTABLE_SUFFIX}
            ${FULL_OUTPUT_DIR}/nacl_loader${CMAKE_EXECUTABLE_SUFFIX}
    )
    add_custom_command(TARGET runtime_deps PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DEPS_DIR}/irt_core-${ARCH}.nexe
            ${FULL_OUTPUT_DIR}/irt_core-${ARCH}.nexe
    )

    # Linux uses a bootstrap program to reserve address space
    if (LINUX)
        add_custom_command(TARGET runtime_deps PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DEPS_DIR}/nacl_helper_bootstrap
                ${FULL_OUTPUT_DIR}/nacl_helper_bootstrap
        )
    endif()

    # Win32 requires nacl_loader64.exe in order to run on Win64
    if (WIN32 AND ARCH STREQUAL x86)
        add_custom_command(TARGET runtime_deps PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DEPS_DIR}/sel_ldr64${CMAKE_EXECUTABLE_SUFFIX}
                ${FULL_OUTPUT_DIR}/nacl_loader64${CMAKE_EXECUTABLE_SUFFIX}
        )
        add_custom_command(TARGET runtime_deps PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DEPS_DIR}/irt_core-x86_64.nexe
                ${FULL_OUTPUT_DIR}/irt_core-x86_64.nexe
        )
    endif()

    # Mac requires some libraries from external_deps
    if (APPLE)
        add_custom_command(TARGET runtime_deps PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${DEPS_DIR}/SDL2.framework
                ${FULL_OUTPUT_DIR}/SDL2.framework
        )
        file(GLOB RUNTIME_LIBS ${DEPS_DIR}/lib/*.dylib)
        foreach(RUNTIME_LIB ${RUNTIME_LIBS})
            add_custom_command(TARGET runtime_deps PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${RUNTIME_LIB}
                    ${FULL_OUTPUT_DIR}
            )
        endforeach()
    endif()

    # Windows requires some libraries from external_deps
    if (WIN32)
        file(GLOB RUNTIME_LIBS ${DEPS_DIR}/bin/*.dll)
        foreach(RUNTIME_LIB ${RUNTIME_LIBS})
            add_custom_command(TARGET runtime_deps PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${RUNTIME_LIB}
                    ${FULL_OUTPUT_DIR}
            )
        endforeach()
    endif()

    if (BUILD_CLIENT)
        add_dependencies(client runtime_deps)
    endif()
    if (BUILD_SERVER)
        add_dependencies(server runtime_deps)
    endif()
    if (BUILD_TTY_CLIENT)
        add_dependencies(ttyclient runtime_deps)
    endif()
endif()
