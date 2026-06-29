# Technical Notes

本文記錄目前程式碼中較容易被誤解的設計選擇。內容只描述現有實作，不代表後續方向。

## Content Addressing

Object identity 使用 raw chunk 的 SHA-256，而不是 zstd frame 的 hash。原因是壓縮輸出可能受 compression level 或 encoder 行為影響；raw chunk hash 較適合用來判斷資料內容是否相同。

對應檔案：

- `src/core/BackupEngine.cpp`
- `src/core/ObjectStore.cpp`
- `include/dpc/common/Hash.hpp`

## Atomic Write

object、manifest、checkpoint 與 commit marker 都透過 tmp file、`fsync`、`rename`、parent directory `fsync` 這類流程降低中斷時的 metadata 不一致風險。

對應檔案：

- `include/dpc/common/FileUtils.hpp`
- `src/core/ObjectStore.cpp`
- `src/core/ManifestStore.cpp`
- `src/metadata/Checkpoint.cpp`
- `src/metadata/CommitMarker.cpp`

## Commit Marker

Manifest rename 完成後不代表 version 已可見。`ManifestStore::listCommittedVersions` 依 commit marker 列出 version，`backupctl list` 隨後載入相同 version 的 manifest。沒有 commit marker 的 manifest 不會列出；只有 commit marker、缺少 manifest 時，載入會回報錯誤。

對應檔案：

- `src/core/ManifestStore.cpp`
- `src/metadata/CommitMarker.cpp`
- `src/metadata/RecoveryManager.cpp`

## Packet Codec

TCP protocol 不直接傳送 C++ struct。`PacketCodec` 明確處理 big-endian encode/decode、header CRC、payload size limit、partial read 與 write loop。

對應檔案：

- `include/dpc/network/PacketCodec.hpp`
- `src/network/PacketCodec.cpp`

## Session Index

Server 端以 `metadata/sessions/<hex-session-id>.session` 保存已收到 chunk。client 使用相同 session id 再次上傳時，server 回覆 chunk index 與 SHA-256，client 跳過 server 已有且 hash 一致的 chunk。

對應檔案：

- `src/network/BackupClient.cpp`
- `src/network/BackupServer.cpp`
- `src/network/SessionIndex.cpp`

## Threading

`BackupServer` 使用 blocking socket 與 bounded thread pool。這個模型簡單直接，但大量 idle connections 會佔用 worker。專案目前沒有 event-driven I/O。

對應檔案：

- `include/dpc/concurrency/ThreadPool.hpp`
- `include/dpc/concurrency/BoundedQueue.hpp`
- `src/network/BackupServer.cpp`

## Benchmark Scope

`backup-bench` 會產生資料、執行 backup/verify/restore，並在比對 source 與 restore 內容後印出 throughput。它支援 `--chunking fixed|cdc`，但目前沒有 memory tracking。

對應檔案：

- `src/bench/backup_bench_main.cpp`
- `scripts/bench.sh`
