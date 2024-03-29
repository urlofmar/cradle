cmake_minimum_required(VERSION 3.14)
project(cradle)

# Define the options for using the preprocessor.
# TODO: Add a Docker option.
set(PREPROCESSOR_USAGE_OPTIONS Native External)
set(PREPROCESSOR_USAGE Native CACHE STRING
    "How the preprocessor should be invoked")
set_property(CACHE PREPROCESSOR_USAGE PROPERTY
    STRINGS ${PREPROCESSOR_USAGE_OPTIONS})
set(EXTERNALLY_PREPROCESSED_DIRECTORY "" CACHE STRING
    "If PREPROCESSOR_USAGE is External, this is the directory that contains the externally preprocessed files.")

# Define the option for local Docker testing.
set(LOCAL_DOCKER_TESTING ON CACHE BOOL
    "Whether or not to enable testing that requires a local Docker instance")
if(LOCAL_DOCKER_TESTING)
    add_compile_options(-DLOCAL_DOCKER_TESTING)
endif()

# Detect the compiler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(IS_CLANG true)
else()
    set(IS_CLANG false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(IS_GCC true)
else()
    set(IS_GCC false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(IS_MSVC true)
else()
    set(IS_MSVC false)
endif()

# CRADLE requires C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include(cmake/version.cmake)

# Download/include CMake/Conan integration.
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.16.1/conan.cmake"
                  "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake"
                  TLS_VERIFY ON)
endif()
include(${CMAKE_CURRENT_BINARY_DIR}/conan.cmake)

# Invoke Conan.
conan_cmake_configure(
    REQUIRES
        boost/1.77.0
        catch2/2.13.4
        nlohmann_json/3.9.1
        msgpack/3.3.0
        libcurl/7.75.0
        openssl/1.1.1j
        sqlite3/3.35.2
        fakeit/2.0.7
        websocketpp/0.8.2
        zlib/1.2.11
        bzip2/1.0.8
        yaml-cpp/0.6.3
        spdlog/1.8.2
        simdjson/0.9.1
        lz4/1.9.3
        fmt/7.1.3
    OPTIONS
        boost:without_chrono=True
        boost:without_container=True
        boost:without_context=True
        boost:without_contract=True
        boost:without_coroutine=True
        boost:without_fiber=True
        boost:without_filesystem=True
        boost:without_graph=True
        boost:without_graph_parallel=True
        boost:without_iostreams=True
        boost:without_json=True
        boost:without_locale=True
        boost:without_log=True
        boost:without_mpi=True
        boost:without_nowide=True
        boost:without_serialization=True
        boost:without_stacktrace=True
        boost:without_test=True
        boost:without_thread=True
        boost:without_timer=True
        boost:without_type_erasure=True
        boost:without_wave=True
        fakeit:integration=catch
        *:shared=False
    GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(
    PATH_OR_REFERENCE .
    BUILD missing
    REMOTE conancenter
    SETTINGS ${settings})

include(FetchContent)

# The cppcoro detection logic seems broken for GCC 10.x (at least in CI), so
# override it.
if (IS_GCC)
    add_library(std::coroutines INTERFACE IMPORTED)
    set(CXX_COROUTINES_HAVE_COROUTINES TRUE CACHE BOOL "TRUE if we have the C++ coroutines feature" FORCE)
    set(CXX_COROUTINES_HEADER coroutine CACHE STRING "The header that should be included to obtain the coroutines APIs" FORCE)
    set(CXX_COROUTINES_NAMESPACE std CACHE STRING "The C++ namespace that contains the coroutines APIs" FORCE)
    set(Coroutines_FOUND TRUE CACHE BOOL "TRUE if we can compile and link a program using std::coroutines" FORCE)
    add_compile_options(-fcoroutines)
endif()

FetchContent_Declare(fetched_cppcoro
    GIT_REPOSITORY https://github.com/mghro/cppcoro
    GIT_TAG master)
FetchContent_MakeAvailable(fetched_cppcoro)

FetchContent_Declare(fetched_thread_pool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool
    GIT_TAG v2.0.0)
FetchContent_MakeAvailable(fetched_thread_pool)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")

find_package(Boost REQUIRED)
find_package(Catch2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(msgpack REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fakeit REQUIRED)
find_package(websocketpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(spdlog REQUIRED)
find_package(simdjson REQUIRED)
find_package(lz4 REQUIRED)

# Add the given linker options on anything that gets linked.
macro(add_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Add the given linker options for executables.
macro(add_exe_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Enable a high level of compiler warnings and treat them as errors.
if(IS_GCC OR IS_CLANG)
    add_compile_options(-Wall -Werror)
    # Disable warnings that are too strict.
    # unused function parameters
    add_compile_options(-Wno-unused-parameter)
    # unused functions
    add_compile_options(-Wno-unused-function)
elseif(IS_MSVC)
    # First strip out the old warning level.
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    add_compile_options(/W4 /WX)
    # Disable warnings that are too strict.
    # "unreferenced formal parameter"
    add_compile_options(/wd4100)
    # "declaration hides previous local declaration"
    add_compile_options(/wd4456)
    # "unreferenced local function has been removed"
    add_compile_options(/wd4505)
    # warnings about functions that are potentially insecure
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
endif()

# Use absolute paths for error messages.
if (IS_CLANG)
    add_compile_options(-fdiagnostics-absolute-paths)
elseif(IS_MSVC)
    add_compile_options(/FC)
endif()

# Enable big objects on MSVC.
if(IS_MSVC)
    add_compile_options(/bigobj)
endif()

# Enable the address sanitizer (for now).
# if(IS_MSVC)
#     add_compile_options(/fsanitize=address /Zi)
# endif()

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(IS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

# Set build options for instrumenting test coverage.
enable_testing()
if(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling gcov support")
    add_compile_options(-DLLVM_USE_LINKER=gold -fprofile-instr-generate
                        -fcoverage-mapping)
    string(APPEND CMAKE_EXE_LINKER_FLAGS
           " -fprofile-instr-generate -fcoverage-mapping")
endif()

# Preprocess the API headers.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/*.hpp")
if("${PREPROCESSOR_USAGE}" STREQUAL "Native")
    message(STATUS "Using native preprocessor")
    add_subdirectory(preprocessor)
    set(preprocessed_files ${headers})
    preprocess_header_files(
        generated_srcs generated_headers
        ACCOUNT_ID mgh
        # TODO: Make this its own actual app...
        TYPE_APP_ID dosimetry
        FUNCTION_APP_ID dosimetry
        NAMESPACE cradle
        INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/api_index.hpp"
        INPUT_FILES ${preprocessed_files})
    add_custom_target(preprocessing
        DEPENDS ${generated_srcs} ${generated_headers})
    set(generated_include_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
elseif("${PREPROCESSOR_USAGE}" STREQUAL "External")
    message(STATUS "Using externally preprocessed files in:")
    message(STATUS "  ${EXTERNALLY_PREPROCESSED_DIRECTORY}")
    get_filename_component(
        resolved_preprocessed_directory
        "${EXTERNALLY_PREPROCESSED_DIRECTORY}"
        ABSOLUTE)
    message(STATUS "  (${resolved_preprocessed_directory})")
    file(GLOB_RECURSE generated_srcs
        "${resolved_preprocessed_directory}/*.cpp")
    file(GLOB_RECURSE generated_headers
        "${resolved_preprocessed_directory}/*.hpp")
    set(generated_include_dir ${resolved_preprocessed_directory})
else()
    message(FATAL_ERROR "Invalid setting for PREPROCESSOR_USAGE")
endif()

# Add the main library.
file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(cradle STATIC ${srcs} ${headers})
# There doesn't seem to be a better way to disable the warnings that are
# generated by WebSocket++.
if(IS_MSVC)
    set_property(
        SOURCE src/cradle/websocket/server.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
    set_property(
        SOURCE src/cradle/websocket/client.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
endif()
target_sources(cradle PRIVATE ${generated_headers} ${generated_srcs}
    "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/version_info.hpp")

target_link_libraries(cradle PUBLIC
    Boost::headers
    Catch2::Catch2
    nlohmann_json::nlohmann_json
    msgpack::msgpack
    CURL::libcurl
    OpenSSL::OpenSSL
    SQLite::SQLite3
    fakeit::fakeit
    websocketpp::websocketpp
    ZLIB::ZLIB
    BZip2::BZip2
    yaml-cpp::yaml-cpp
    spdlog::spdlog
    simdjson::simdjson
    lz4::lz4
    cppcoro)
target_compile_definitions(
    cradle PUBLIC ${CONAN_DEFINES} ${CONAN_COMPILE_DEFINITIONS})
target_include_directories(cradle PUBLIC
    ${generated_include_dir}/src
    src
    src/third-party)

# Set up the deployment directory.
set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}/deploy")
file(MAKE_DIRECTORY ${deploy_dir})

# Add the server.
add_executable(server src/server.cpp)
target_link_libraries(server cradle)
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:server> ${deploy_dir})

# Add the unit test runner.
file(GLOB_RECURSE UNIT_TEST_FILES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(unit_test_runner ${UNIT_TEST_FILES})
target_link_libraries(unit_test_runner cradle)
target_include_directories(unit_test_runner
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests)
# Don't use precompiled headers for the test runner since it needs to include
# Catch specially.
set_source_files_properties(
    tests/runner.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON)

# Retrieve the token info if it exists
if (DEFINED ENV{CRADLE_THINKNODE_API_TOKEN})
    set(API_TOKEN $ENV{CRADLE_THINKNODE_API_TOKEN})
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.token")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.token" API_TOKEN)
else()
    message(WARNING "Couldn't find API token.")
    set(API_TOKEN "(missing)")
endif()
string(REGEX REPLACE "\n" "" API_TOKEN "${API_TOKEN}")

# Retrieve the Docker auth info if it exists
if (DEFINED ENV{CRADLE_THINKNODE_DOCKER_AUTH})
    set(DOCKER_AUTH $ENV{CRADLE_THINKNODE_DOCKER_AUTH})
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.docker-auth")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.docker-auth" DOCKER_AUTH)
else()
    message(WARNING "Couldn't find Docker auth string.")
    set(DOCKER_AUTH "(missing)")
endif()
string(REGEX REPLACE "\n" "" DOCKER_AUTH "${DOCKER_AUTH}")

# Add the unit testing target.
add_custom_target(
    unit_tests
    # Create a fresh 'unit-testing' directory within the build dir and run the
    # tests within that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E make_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E chdir unit-testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} $<TARGET_FILE:unit_test_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS unit_test_runner)

# Add the unit test coverage target.
if(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-profdata LLVM_PROFDATA ${CMAKE_CXX_COMPILER})
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-cov LLVM_COV ${CMAKE_CXX_COMPILER})
    add_custom_target(
        unit_test_coverage
        COMMAND ${CMAKE_COMMAND} --build . --target unit_tests
        COMMAND ${LLVM_PROFDATA} merge -sparse unit-testing/default.profraw
                -o default.profdata
        COMMAND ${LLVM_COV} show -instr-profile=default.profdata
                $<TARGET_FILE:unit_test_runner>
                >${PROJECT_SOURCE_DIR}/coverage.txt
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    add_test(
        NAME unit_test_coverage
        COMMAND ${CMAKE_COMMAND} --build . --target unit_test_coverage
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
else()
    add_test(
        NAME unit_tests
        COMMAND ${CMAKE_COMMAND}
            --build ${CMAKE_BINARY_DIR}
            --target unit_tests)
endif()

# Add the integration testing target.
add_custom_target(
    integration_tests
    COMMAND ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} pytest
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
    DEPENDS server)
add_test(
    NAME integration_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target integration_tests)
