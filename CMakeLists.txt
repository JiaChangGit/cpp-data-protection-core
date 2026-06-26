cmake_minimum_required(VERSION 3.16)

# 專案名稱與版本設定
# LANGUAGES CXX 表示此專案主要使用 C++。
project(cpp-data-protection-core VERSION 0.1.0 LANGUAGES CXX)

# 使用 C++17 標準
#
# CMAKE_CXX_STANDARD_REQUIRED ON：
#   要求編譯器一定要支援指定的 C++ 標準。
#
# CMAKE_CXX_EXTENSIONS OFF：
#   關閉 GNU extension，例如 gnu++17，改用標準 c++17。
#   這樣可以提高跨編譯器相容性。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 是否建置測試
option(DPC_BUILD_TESTS "Build tests" ON)

# Sanitizer 開關
#
# DPC_ENABLE_ASAN:
#   AddressSanitizer，用來偵測 memory bug，例如 heap overflow、use-after-free。
#
# DPC_ENABLE_UBSAN:
#   UndefinedBehaviorSanitizer，用來偵測 undefined behavior，例如 signed overflow。
#
# DPC_ENABLE_TSAN:
#   ThreadSanitizer，用來偵測 data race 與多執行緒同步問題。
option(DPC_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DPC_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(DPC_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# 統一輸出目錄
#
# Runtime binary：
#   build/bin/
#
# Static library：
#   build/lib/
#
# Shared library：
#   build/lib/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 產生 compile_commands.json
#
# clang-tidy、clangd、CodeQL、IDE 等工具可以用它取得實際編譯參數。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# TSan 不應該和 ASan / UBSan 混用
#
# 原因：
#   ThreadSanitizer runtime 和其他 sanitizer runtime 常常會衝突。
#   因此 CI 裡通常會分成：
#     1. ASan + UBSan
#     2. TSan
if(DPC_ENABLE_TSAN AND (DPC_ENABLE_ASAN OR DPC_ENABLE_UBSAN))
    message(FATAL_ERROR "DPC_ENABLE_TSAN cannot be combined with ASAN or UBSAN")
endif()

# 對指定 target 啟用 sanitizer
#
# 使用方式：
#   dpc_enable_sanitizers(target_name)
#
# 這個 function 會根據上面的 DPC_ENABLE_ASAN / UBSAN / TSAN 選項，
# 自動把 sanitizer flags 加到 compile options 與 link options。
function(dpc_enable_sanitizers target_name)
    # Sanitizer 主要支援 GCC / Clang。
    # 如果使用 MSVC 或其他編譯器，這裡直接略過。
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

    # 同時加到 compile 與 link。
    #
    # 注意：
    #   sanitizer 不只需要在編譯時打開，也需要在連結階段連到對應 runtime。
    if(sanitizer_flags)
        target_compile_options(${target_name} PRIVATE ${sanitizer_flags})
        target_link_options(${target_name} PRIVATE ${sanitizer_flags})
    endif()
endfunction()

# 尋找 OpenSSL
#
# 此專案使用 OpenSSL::Crypto。
find_package(OpenSSL REQUIRED)

# 尋找 pthread / threading library
#
# 使用 Threads::Threads 可以讓 CMake 自動處理不同平台的 thread link flags。
find_package(Threads REQUIRED)

# 尋找 Zstandard shared library
#
# Ubuntu 上通常是：
#   /usr/lib/x86_64-linux-gnu/libzstd.so
#
# 這裡使用 find_library，避免不同 Linux distribution 的 library path 不一致。
find_library(ZSTD_LIBRARY
    NAMES zstd libzstd.so.1
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
)

if(NOT ZSTD_LIBRARY)
    message(FATAL_ERROR "libzstd shared library is required")
endif()

# 核心函式庫
#
# dpc_core 包含整個 data protection core 的主要模組：
#   1. core：backup / restore / verify / chunking / compression / object store
#   2. metadata：WAL / checkpoint / commit marker / recovery / compaction / fault injection
#   3. network：client / server / packet codec / transfer session
#   4. concurrency：thread pool / bounded queue
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

# 對外公開 include/ 目錄
#
# PUBLIC 表示：
#   1. dpc_core 自己可以 include
#   2. link dpc_core 的 executable / test 也可以 include
target_include_directories(dpc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 連結 dpc_core 所需外部 library
#
# PUBLIC 表示依賴會傳遞給使用 dpc_core 的 target。
target_link_libraries(dpc_core PUBLIC
    OpenSSL::Crypto
    ${ZSTD_LIBRARY}
    Threads::Threads
)

# 啟用常見警告
#
# -Wall:
#   開啟常見 warning。
#
# -Wextra:
#   開啟額外 warning。
#
# -Wpedantic:
#   檢查是否符合標準 C++。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dpc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 對核心函式庫套用 sanitizer 設定
dpc_enable_sanitizers(dpc_core)

# backupctl CLI
#
# 主要命令列工具，通常用來觸發 local backup / restore / verify 等操作。
add_executable(backupctl src/cli/backupctl_main.cpp)
target_link_libraries(backupctl PRIVATE dpc_core)
dpc_enable_sanitizers(backupctl)

# backup-client
#
# 用於 client side 備份傳輸流程。
add_executable(backup-client src/cli/backup_client_main.cpp)
target_link_libraries(backup-client PRIVATE dpc_core)
dpc_enable_sanitizers(backup-client)

# backup-server
#
# 用於 server side 接收備份資料。
add_executable(backup-server src/cli/backup_server_main.cpp)
target_link_libraries(backup-server PRIVATE dpc_core)
dpc_enable_sanitizers(backup-server)

# benchmark 工具
#
# 用來測量備份、壓縮、chunking 等流程的基本效能。
add_executable(backup-bench src/bench/backup_bench_main.cpp)
target_link_libraries(backup-bench PRIVATE dpc_core)
dpc_enable_sanitizers(backup-bench)

# repository corruption 工具
#
# 用於 fault injection / crash recovery 測試，
# 模擬 metadata 或 object store 損壞情境。
add_executable(corrupt-repo src/tools/corrupt_repo_main.cpp)
target_link_libraries(corrupt-repo PRIVATE dpc_core)
dpc_enable_sanitizers(corrupt-repo)

# 安裝規則
#
# 執行 cmake --install 時，會把主要執行檔安裝到 bin/。
install(TARGETS
    backupctl
    backup-client
    backup-server
    backup-bench
    RUNTIME DESTINATION bin
)

# 測試設定
#
# DPC_BUILD_TESTS=ON 時才會建置 unit tests。
if(DPC_BUILD_TESTS)
    include(CTest)
    enable_testing()

    # 優先尋找系統已安裝的 GoogleTest。
    #
    # CI 裡有安裝 libgtest-dev，但不同環境下 GTest CMake package 不一定存在。
    # 因此如果 find_package(GTest QUIET) 找不到，就用 FetchContent 下載指定版本。
    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )

        # Windows / MSVC 常用設定。
        # 即使目前 CI 是 Linux，保留這個設定可提高跨平台相容性。
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(googletest)
    endif()

    # Unit test executable
    #
    # 測試範圍：
    #   1. fixed-size chunking
    #   2. content-defined chunking
    #   3. hash
    #   4. compression
    #   5. manifest
    #   6. WAL
    #   7. packet codec
    #   8. bounded queue
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

    # TSan 模式下，不使用 gtest_discover_tests。
    #
    # 原因：
    #   gtest_discover_tests 會在 CMake configure / build 階段執行 test binary
    #   來列舉測試案例。
    #
    #   在 TSan runtime 下，這種「建置階段先跑一次 test binary」有時候比較容易造成
    #   CI 雜訊或 sanitizer runtime 問題。
    #
    #   所以 TSan 模式下直接註冊整個 dpc_unit_tests。
    #
    # 非 TSan 模式下：
    #   使用 gtest_discover_tests，讓 CTest 可以看到每個 GoogleTest case。
    if(DPC_ENABLE_TSAN)
        add_test(NAME dpc_unit_tests COMMAND dpc_unit_tests)
    else()
        include(GoogleTest)
        gtest_discover_tests(dpc_unit_tests)
    endif()
endif()
