# 實作決策

本文件記錄目前程式碼採用的資料格式與處理方式。每一項均可由列出的實作或測試檔案核對。

## Raw chunk 作為 object identity

Object path 由 raw chunk 的 SHA-256 決定。zstd frame 只作為儲存格式，不參與 object identity，因此相同 raw bytes 會對應到相同 repository path。

對應檔案：

- `include/dpc/common/Hash.hpp`
- `src/core/BackupEngine.cpp`
- `src/core/ObjectStore.cpp`

## Commit marker 決定版本可見性

`ManifestStore::listCommittedVersions` 掃描 `version-*.commit`。`load` 會先確認 commit marker，再讀取同版本 manifest；只有 WAL record 或暫存 manifest 不會讓版本出現在 `backupctl list`。

對應檔案：

- `src/core/ManifestStore.cpp`
- `src/metadata/CommitMarker.cpp`
- `tests/fault_injection/crash_recovery_test.sh`

## Recovery 不重播 WAL 操作

`RecoveryManager::recover` 呼叫 `WalLog::readAll` 驗證所有 record 的 header、長度、type 與 CRC。驗證成功後，它刪除 `repo/tmp` 下的 regular files，掃描 commit marker，並重寫 checkpoint。WAL payload 中的 BEGIN/COMMIT 不會取代 commit marker 成為版本狀態來源。

對應檔案：

- `src/metadata/WalLog.cpp`
- `src/metadata/RecoveryManager.cpp`
- `tests/unit/test_wal.cpp`

## Restore path 與 object path 檢查

Manifest parser 會拒絕空路徑、absolute path、null byte 與 parent traversal。`ObjectStore::readRaw` 另外由 chunk SHA-256 重算 object path，要求它與 manifest 欄位完全一致，再讀取、解壓並驗證 raw bytes。

對應檔案：

- `include/dpc/common/FileUtils.hpp`
- `src/core/ManifestStore.cpp`
- `src/core/ObjectStore.cpp`
- `src/core/RestoreEngine.cpp`
- `tests/security/security_malformed_test.sh`

## Packet 使用明確的 wire encoding

`PacketCodec` 將整數欄位編碼為 big-endian，不直接把 C++ struct 寫入 socket。Decoder 會檢查 magic、version、packet type、payload size、frame size 與 header CRC。

對應檔案：

- `include/dpc/network/PacketCodec.hpp`
- `src/network/PacketCodec.cpp`
- `tests/unit/test_packet_codec.cpp`

## Session id 用於中斷補傳

Server 將已收到的 chunk record 追加到 `metadata/sessions/<hex-session-id>.session`。Client 以相同 session id 查詢 global chunk index 與 SHA-256，並跳過 hash 一致的既有 chunk。Session id 不是 authentication token，protocol 也沒有 authorization 檢查。

對應檔案：

- `src/network/BackupClient.cpp`
- `src/network/BackupServer.cpp`
- `src/network/SessionIndex.cpp`

## Server 使用 blocking I/O 與 bounded thread pool

`BackupServer::run` 在主執行緒執行 `accept`，每個連線提交到 `ThreadPool`。Connection handler 使用 blocking read；queue capacity 固定為 128。CLI 未註冊 signal handler，也沒有 graceful shutdown 命令。

對應檔案：

- `src/network/BackupServer.cpp`
- `src/cli/backup_server_main.cpp`
- `include/dpc/concurrency/ThreadPool.hpp`
- `include/dpc/concurrency/BoundedQueue.hpp`

## Benchmark 先驗證內容再輸出 metrics

`backup-bench` 會產生 workload、建立備份、驗證、還原，最後比對 source 與 restore 的 regular file 清單和 bytes。任何階段失敗都會回傳 non-zero；metrics 只在比對成功後輸出。

對應檔案：

- `src/bench/backup_bench_main.cpp`
- `scripts/bench.sh`
- [benchmark.md](benchmark.md)
