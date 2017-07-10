cmake_minimum_required(VERSION 2.8.12)

enable_testing()

# Set up fips.
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")
set(FIPS_EXCEPTIONS ON)
set(FIPS_RTTI ON)
fips_setup()
fips_project(cradle)

# Run Conan to install external C++ libraries.
# Conan and fips disagree on which runtime we should use for MSVC, so
# we need to manually sort that out for MSVC builds.
set(conan_options)
if (FIPS_MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(conan_options
            -s compiler=Visual\ Studio -s build_type=Debug -s compiler.runtime=MTd)
    else()
        set(conan_options
            -s compiler=Visual\ Studio -s build_type=Release -s compiler.runtime=MT)
    endif()
elseif(FIPS_GCC)
    set(conan_options -o Boost:fPIC=True)
endif()
execute_process(
    COMMAND conan install ${CMAKE_SOURCE_DIR} ${conan_options} --build missing
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE conan_result)
if(NOT ${conan_result} EQUAL 0)
    message(FATAL_ERROR "Conan failed.")
endif()

# And now set up CMake to use those libraries.
# Note that Conan seems to insert flags that don't make sense and cause warnings.
set(original_cxx_flags "${CMAKE_CXX_FLAGS}")
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()
set(CMAKE_CXX_FLAGS "${original_cxx_flags}")

# Register Conan's include directories with fips.
fips_include_directories(${CONAN_INCLUDE_DIRS}
    "$<$<CONFIG:Release>:${CONAN_INCLUDE_DIRS_RELEASE}>"
    "$<$<CONFIG:Debug>:${CONAN_INCLUDE_DIRS_DEBUG}>")

# Include cotire.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CONAN_COTIRE_ROOT}/cmake/cotire")
include(cotire)

fips_include_directories(${CMAKE_SOURCE_DIR}/src)

# Enable advanced C++ features.
if(FIPS_GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

# Enable a high level of compiler warnings and treat them as errors.
if(FIPS_GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    # Disable warnings that are too strict.
    # Warnings about suggested parentheses occur naturally when using Catch.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-parentheses")
    # unused function parameters
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
elseif(FIPS_MSVC)
    # First strip out the old warning level.
    string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    # Disable warnings that are too strict.
    # "unreferenced formal parameter"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")
    # "declaration hides previous local declaration"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4456")
    # "unreferenced local function has been removed"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")
endif()

# Enable "big objects" for Visual C++.
if(FIPS_MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()

# Set some other compiler and linker flags to speed up builds.
if(FIPS_MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FASTLINK")
endif()

# Exclude as much of the Windows API as possible to avoid #define pollution and speed up builds.
# Note that this has to be done via command-line arguments due to precompilation.
if(FIPS_MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOUSER /DNONLS /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX")
endif()

# Set compiler flags for instrumenting test coverage.
if(FIPS_GCC)
    message(STATUS "Enabling gcov support")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fno-inline -fno-inline-small-functions -fno-default-inline")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
endif()

fips_include_directories(${CMAKE_BINARY_DIR}/generated)

fips_begin_app(test_runner cmdline)
    fips_src(src)
    fips_src(tests)
    fips_dir(.)
    fips_generate(
        FROM api.json
        TYPE cpp_api_generator
        OUT_OF_SOURCE
        SOURCE "generated/cradle/api_types.cpp"
        HEADER "generated/cradle/api_types.hpp"
        ARGS "{ app_name: cradle }")
fips_end_app()

target_link_libraries(test_runner ${CONAN_LIBS})

# Add tests.
# set(test_dir ${CMAKE_SOURCE_DIR}/tests)
# file (GLOB_RECURSE test_files ${CMAKE_SOURCE_DIR}/tests/*.[chi]pp)
# add_executable(test_runner ${source_files} ${test_files})
target_link_libraries(test_runner ${CONAN_LIBS})
# add_test(NAME test_runner COMMAND test_runner)

# Exclude from precompiled headers any local headers or overly specific libraries.
set_property(
    DIRECTORY
    PROPERTY
        COTIRE_PREFIX_HEADER_IGNORE_PATH
            ${CMAKE_SOURCE_DIR}
            #${CMAKE_BINARY_DIR}
            ${CONAN_MSGPACK-C_ROOT}
            ${CONAN_CATCH_ROOT}
            ${CONAN_JSON_ROOT})

# Exclude the test runner source file from Cotire because it acts as the "main" file for Catch
# and needs to include Catch specially.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/tests/runner.cpp
    PROPERTIES
        COTIRE_EXCLUDED ON)

set_property (SOURCE "api.json" PROPERTY COTIRE_DEPENDENCY "TRUE")

# Invoke cotire.
cotire(test_runner)

fips_finish()
