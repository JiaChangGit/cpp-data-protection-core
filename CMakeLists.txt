cmake_minimum_required(VERSION 3.16)

project(cpp-data-protection-core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DPC_BUILD_TESTS "Build tests" ON)
option(DPC_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DPC_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(DPC_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DPC_ENABLE_TSAN AND (DPC_ENABLE_ASAN OR DPC_ENABLE_UBSAN))
    message(FATAL_ERROR "DPC_ENABLE_TSAN cannot be combined with ASAN or UBSAN")
endif()

# Apply the selected sanitizer flags to one target. Unsupported compilers are
# left unchanged.
function(dpc_enable_sanitizers target_name)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    set(sanitizer_flags "")

    if(DPC_ENABLE_TSAN)
        list(APPEND sanitizer_flags -fsanitize=thread -fno-omit-frame-pointer)
    else()
        if(DPC_ENABLE_ASAN)
            list(APPEND sanitizer_flags -fsanitize=address -fno-omit-frame-pointer)
        endif()

        if(DPC_ENABLE_UBSAN)
            list(APPEND sanitizer_flags -fsanitize=undefined -fno-omit-frame-pointer)
        endif()
    endif()

    if(sanitizer_flags)
        target_compile_options(${target_name} PRIVATE ${sanitizer_flags})
        target_link_options(${target_name} PRIVATE ${sanitizer_flags})
    endif()
endfunction()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

find_library(ZSTD_LIBRARY
    NAMES zstd libzstd.so.1
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
)

if(NOT ZSTD_LIBRARY)
    message(FATAL_ERROR "libzstd shared library is required")
endif()

add_library(dpc_core
    src/core/BackupEngine.cpp
    src/core/RestoreEngine.cpp
    src/core/VerifyEngine.cpp
    src/core/FileScanner.cpp
    src/core/FixedChunker.cpp
    src/core/ContentDefinedChunker.cpp
    src/core/Compressor.cpp
    src/core/ObjectStore.cpp
    src/core/ManifestStore.cpp

    src/metadata/WalLog.cpp
    src/metadata/Checkpoint.cpp
    src/metadata/CommitMarker.cpp
    src/metadata/RecoveryManager.cpp
    src/metadata/MetadataCompactor.cpp
    src/metadata/FaultInjector.cpp

    src/network/BackupClient.cpp
    src/network/BackupServer.cpp
    src/network/PacketCodec.cpp
    src/network/TransferSession.cpp
    src/network/SessionIndex.cpp

    src/concurrency/ThreadPool.cpp
    src/concurrency/BoundedQueue.cpp
)

target_include_directories(dpc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(dpc_core PUBLIC
    OpenSSL::Crypto
    ${ZSTD_LIBRARY}
    Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dpc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

dpc_enable_sanitizers(dpc_core)

add_executable(backupctl src/cli/backupctl_main.cpp)
target_link_libraries(backupctl PRIVATE dpc_core)
dpc_enable_sanitizers(backupctl)

add_executable(backup-client src/cli/backup_client_main.cpp)
target_link_libraries(backup-client PRIVATE dpc_core)
dpc_enable_sanitizers(backup-client)

add_executable(backup-server src/cli/backup_server_main.cpp)
target_link_libraries(backup-server PRIVATE dpc_core)
dpc_enable_sanitizers(backup-server)

add_executable(backup-bench src/bench/backup_bench_main.cpp)
target_link_libraries(backup-bench PRIVATE dpc_core)
dpc_enable_sanitizers(backup-bench)

# WAL corruption helper. This target is not installed or included in release
# artifacts.
add_executable(corrupt-repo src/tools/corrupt_repo_main.cpp)
target_link_libraries(corrupt-repo PRIVATE dpc_core)
dpc_enable_sanitizers(corrupt-repo)

install(TARGETS
    backupctl
    backup-client
    backup-server
    backup-bench
    RUNTIME DESTINATION bin
)

if(DPC_BUILD_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(dpc_unit_tests
        tests/unit/test_fixed_chunker.cpp
        tests/unit/test_cdc_chunker.cpp
        tests/unit/test_hash.cpp
        tests/unit/test_compressor.cpp
        tests/unit/test_manifest.cpp
        tests/unit/test_wal.cpp
        tests/unit/test_packet_codec.cpp
        tests/unit/test_bounded_queue.cpp
    )

    target_link_libraries(dpc_unit_tests PRIVATE
        dpc_core
        GTest::gtest_main
    )

    dpc_enable_sanitizers(dpc_unit_tests)

    # Test discovery executes the test binary while building. TSan builds use
    # one aggregate CTest entry to avoid running the instrumented binary during
    # discovery.
    if(DPC_ENABLE_TSAN)
        add_test(NAME dpc_unit_tests COMMAND dpc_unit_tests)
    else()
        include(GoogleTest)
        gtest_discover_tests(dpc_unit_tests)
    endif()
endif()
