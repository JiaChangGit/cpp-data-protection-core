cmake_minimum_required(VERSION 3.16)

# 專案名稱會出現在 IDE、CMake configure log 與 install metadata 中。
# VERSION 可讓後續封裝、release script 或 --version 指令共用同一份版本資訊。
project(cpp-data-protection-core VERSION 0.1.0 LANGUAGES CXX)

# ============================================================
# 使用者可調整的建置選項
# ============================================================
# ON/OFF 都是 CMake 的布林值：
#   cmake -S . -B build -DDPC_BUILD_TESTS=OFF
#   cmake -S . -B build -DDPC_ENABLE_ASAN=ON
#
# Sanitizer 建議一次只開需要的種類，避免不同 runtime 互相干擾。
option(DPC_BUILD_TESTS "Build unit tests" ON)
option(DPC_FETCH_GTEST "Download GoogleTest with FetchContent when no system package is found" ON)
option(DPC_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DPC_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(DPC_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(DPC_ENABLE_WARNINGS "Enable common compiler warnings" ON)

# ============================================================
# 全域建置設定
# ============================================================
# C++17 是此專案的最低語言層級。
# CMAKE_CXX_EXTENSIONS=OFF 會要求標準 C++，避免不小心依賴 GNU extension。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 將執行檔與 library 集中放在 build/bin、build/lib。
# 這樣 demo script、CI artifact 與清理流程都比較穩定。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 產生 compile_commands.json，方便 clangd、clang-tidy、VS Code C/C++ extension 讀取真實編譯參數。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ThreadSanitizer 會替換部分 runtime 行為，通常不能和 ASan/UBSan 混用。
# 直接在 configure 階段擋下，避免等到 link 或 test 階段才出現難判讀的錯誤。
if(DPC_ENABLE_TSAN AND (DPC_ENABLE_ASAN OR DPC_ENABLE_UBSAN))
    message(FATAL_ERROR "DPC_ENABLE_TSAN cannot be combined with DPC_ENABLE_ASAN or DPC_ENABLE_UBSAN")
endif()

# ============================================================
# 共用 target 設定函式
# ============================================================
# CMake 的 target 是建置系統的核心單位，例如 library、executable。
# 把重複設定收斂到函式內，可以降低複製貼上造成的漏改風險。
function(dpc_apply_common_target_options target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Target '${target_name}' does not exist")
    endif()

    # target_compile_features 是 target 層級的語言標準需求。
    # 即使上方已有 CMAKE_CXX_STANDARD，這裡仍保留明確標記，方便 IDE 與相依 target 理解需求。
    target_compile_features("${target_name}" PRIVATE cxx_std_17)

    if(DPC_ENABLE_WARNINGS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            # -Wall      ：開啟常見警告。
            # -Wextra    ：補上更多容易漏掉的問題，例如未使用參數。
            # -Wpedantic ：提醒非標準 C++ 寫法，讓程式更容易跨平台。
            target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
        elseif(MSVC)
            # /W4 是 MSVC 常用的較完整警告層級。
            # /permissive- 會讓 MSVC 更接近標準 C++ 行為。
            target_compile_options("${target_name}" PRIVATE /W4 /permissive-)
        endif()
    endif()

    dpc_enable_sanitizers("${target_name}")
endfunction()

# 對單一 target 套用 sanitizer。
# sanitizer 是動態檢查工具，常用於 CI 或 debug build：
#   ASan  ：偵測 use-after-free、buffer overflow 等記憶體錯誤。
#   UBSan ：偵測 signed overflow、invalid shift 等未定義行為。
#   TSan  ：偵測 data race，但執行成本較高，也不建議和其他 sanitizer 混用。
function(dpc_enable_sanitizers target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Target '${target_name}' does not exist")
    endif()

    # MSVC 的 sanitizer 支援與 GCC/Clang 不完全一致。
    # 這裡先只對 GNU/Clang 系列套用，避免跨平台 configure 失敗。
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
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
        # compile option 會影響 .cpp 如何被編成 object file。
        # link option 會影響最後 executable / library 如何連到 sanitizer runtime。
        target_compile_options("${target_name}" PRIVATE ${sanitizer_flags})
        target_link_options("${target_name}" PRIVATE ${sanitizer_flags})
    endif()
endfunction()

# 建立連到 dpc_core 的工具程式。
# 所有 CLI / bench / internal tool 都走同一條路徑，避免漏掉 warning 或 sanitizer 設定。
function(dpc_add_core_executable target_name source_file)
    add_executable("${target_name}" "${source_file}")
    target_link_libraries("${target_name}" PRIVATE dpc_core)
    dpc_apply_common_target_options("${target_name}")
endfunction()

# ============================================================
# 第三方相依套件
# ============================================================
# OpenSSL::Crypto：提供雜湊、加密與完整性檢查相關功能。
# Threads::Threads：提供跨平台 thread link flags，例如 Linux 上可能需要 -pthread。
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(Threads REQUIRED)

# zstd 在不同 Linux 發行版、macOS Homebrew、CI image 中的安裝方式可能不同。
# 優先使用 pkg-config 匯出的 imported target；找不到時再退回 find_library。
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZSTD QUIET IMPORTED_TARGET libzstd)
endif()

if(TARGET PkgConfig::ZSTD)
    set(DPC_ZSTD_TARGET PkgConfig::ZSTD)
else()
    find_library(DPC_ZSTD_LIBRARY
        NAMES zstd libzstd.so.1
        PATHS
            /usr/local
            /usr
            /usr/lib/x86_64-linux-gnu
            /usr/lib64
            /usr/lib
            /opt/homebrew
            /opt/local
        PATH_SUFFIXES lib lib64
    )

    if(NOT DPC_ZSTD_LIBRARY)
        message(FATAL_ERROR "libzstd shared library is required")
    endif()

    add_library(dpc_zstd UNKNOWN IMPORTED)
    set_target_properties(dpc_zstd PROPERTIES IMPORTED_LOCATION "${DPC_ZSTD_LIBRARY}")
    set(DPC_ZSTD_TARGET dpc_zstd)
endif()

# ============================================================
# dpc_core：核心 library
# ============================================================
# 建議把 source list 獨立成變數，原因是：
#   1. source 的分層會比塞在 add_library 裡更清楚。
#   2. 未來要拆子目錄、做 object library、加入 clang-tidy 時比較好調整。
#   3. Code review 時能快速看出某次 commit 新增了哪個模組。
set(DPC_CORE_SOURCES
    # backup / restore / verify 的主要流程。
    src/core/BackupEngine.cpp
    src/core/RestoreEngine.cpp
    src/core/VerifyEngine.cpp
    src/core/FileScanner.cpp
    src/core/FixedChunker.cpp
    src/core/ContentDefinedChunker.cpp
    src/core/Compressor.cpp
    src/core/ObjectStore.cpp
    src/core/ManifestStore.cpp

    # Metadata 與 crash recovery 相關元件。
    src/metadata/WalLog.cpp
    src/metadata/Checkpoint.cpp
    src/metadata/CommitMarker.cpp
    src/metadata/RecoveryManager.cpp
    src/metadata/MetadataCompactor.cpp
    src/metadata/FaultInjector.cpp

    # Client / server 傳輸、封包編解碼與 session 管理。
    src/network/BackupClient.cpp
    src/network/BackupServer.cpp
    src/network/PacketCodec.cpp
    src/network/TransferSession.cpp
    src/network/SessionIndex.cpp

    # 平行處理與 producer-consumer queue。
    src/concurrency/ThreadPool.cpp
    src/concurrency/BoundedQueue.cpp
)

add_library(dpc_core ${DPC_CORE_SOURCES})

# PUBLIC：使用 dpc_core 的 target 也需要看到 include/。
# BUILD_INTERFACE：在原始碼目錄建置時使用。
# INSTALL_INTERFACE：未來若擴充 install/export package，會使用安裝後的 include 路徑。
target_include_directories(dpc_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(dpc_core
    PUBLIC
        OpenSSL::Crypto
        ${DPC_ZSTD_TARGET}
        Threads::Threads
)

dpc_apply_common_target_options(dpc_core)

# ============================================================
# 使用者可執行工具與內部輔助工具
# ============================================================
# backupctl      ：主要 CLI 入口。
# backup-client  ：網路備份 client。
# backup-server  ：網路備份 server。
# backup-bench   ：效能量測工具。
# corrupt-repo   ：故意破壞 WAL / repository 狀態，供 recovery 測試使用，不納入 release install。
dpc_add_core_executable(backupctl src/cli/backupctl_main.cpp)
dpc_add_core_executable(backup-client src/cli/backup_client_main.cpp)
dpc_add_core_executable(backup-server src/cli/backup_server_main.cpp)
dpc_add_core_executable(backup-bench src/bench/backup_bench_main.cpp)
dpc_add_core_executable(corrupt-repo src/tools/corrupt_repo_main.cpp)

install(TARGETS
    backupctl
    backup-client
    backup-server
    backup-bench
    RUNTIME DESTINATION bin
)

# ============================================================
# 單元測試
# ============================================================
if(DPC_BUILD_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        if(NOT DPC_FETCH_GTEST)
            message(FATAL_ERROR "GTest was not found and DPC_FETCH_GTEST is OFF")
        endif()

        include(FetchContent)

        # 若系統沒有安裝 GTest，才由 CMake 下載指定版本。
        # 在不能連外的 CI 環境，建議改成：
        #   1. 預先安裝 libgtest-dev / googletest。
        #   2. 或設定 -DDPC_FETCH_GTEST=OFF，讓 configure 直接失敗並暴露環境問題。
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )

        # Windows / MSVC 常見設定：讓 GTest 使用與專案一致的 C runtime。
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    set(DPC_UNIT_TEST_SOURCES
        tests/unit/test_fixed_chunker.cpp
        tests/unit/test_cdc_chunker.cpp
        tests/unit/test_hash.cpp
        tests/unit/test_compressor.cpp
        tests/unit/test_manifest.cpp
        tests/unit/test_wal.cpp
        tests/unit/test_packet_codec.cpp
        tests/unit/test_bounded_queue.cpp
    )

    add_executable(dpc_unit_tests ${DPC_UNIT_TEST_SOURCES})

    target_link_libraries(dpc_unit_tests
        PRIVATE
            dpc_core
            GTest::gtest_main
    )

    dpc_apply_common_target_options(dpc_unit_tests)

    # gtest_discover_tests 會在 build 階段執行測試 binary，列出每個 TEST/TEST_F。
    # 但 TSan instrumented binary 啟動較慢，且 discovery 時執行可能讓 CI log 混雜。
    # 因此 TSan 模式使用單一 CTest entry，等 ctest 階段再完整執行。
    if(DPC_ENABLE_TSAN)
        add_test(NAME dpc_unit_tests COMMAND dpc_unit_tests)
    else()
        include(GoogleTest)
        gtest_discover_tests(
            dpc_unit_tests
            DISCOVERY_TIMEOUT 60
        )
    endif()
endif()
