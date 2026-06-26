# Interview Q&A

本文提供技術討論題綱。內容只描述目前程式碼已實作的行為與限制。

## 1. Why hash the raw chunk instead of compressed bytes?

Raw chunk SHA-256 represents the data identity. zstd output can depend on encoder behavior or compression settings, so using compressed bytes as identity would make dedup less stable.

Related files:

- `include/dpc/common/Hash.hpp`
- `src/core/BackupEngine.cpp`
- `src/core/ObjectStore.cpp`

## 2. How does the repository decide that a version is committed?

A version is visible only when its manifest and commit marker both exist. `backupctl list` uses committed versions from `ManifestStore::listCommittedVersions`.

Related files:

- `src/core/ManifestStore.cpp`
- `src/metadata/CommitMarker.cpp`
- `src/metadata/RecoveryManager.cpp`

## 3. What does WAL recovery do?

Recovery validates WAL records, removes temporary files, scans committed versions, and rewrites checkpoint data. It does not treat WAL-only `COMMIT_BACKUP` records as committed versions without commit markers.

Related files:

- `src/metadata/WalLog.cpp`
- `src/metadata/RecoveryManager.cpp`
- `tests/unit/test_wal.cpp`

## 4. How is restore path traversal prevented?

Manifest relative paths are decoded and validated with `fileutil::safeRelativePath`. Absolute paths, empty paths, null bytes, and parent directory traversal are rejected before restore writes output.

Related files:

- `include/dpc/common/FileUtils.hpp`
- `src/core/ManifestStore.cpp`
- `src/core/RestoreEngine.cpp`
- `tests/security/security_malformed_test.sh`

## 5. Why does verify recompute the expected object path?

Manifest object paths are not trusted as authority. `ObjectStore::readRaw` computes the expected path from chunk SHA-256 and rejects mismatches before reading the object.

Related files:

- `src/core/ObjectStore.cpp`
- `tests/security/security_malformed_test.sh`

## 6. Why not write C++ packet structs directly to the socket?

Direct struct writes are sensitive to padding, alignment, endianness, and compiler layout. `PacketCodec` uses explicit big-endian encode/decode and validates magic, version, type, header CRC, and payload size.

Related files:

- `include/dpc/network/PacketCodec.hpp`
- `src/network/PacketCodec.cpp`
- `tests/unit/test_packet_codec.cpp`

## 7. What is the TCP upload recovery mechanism?

The server stores received chunk records in `metadata/sessions/<hex-session-id>.session`. When the client reconnects with the same session id, it queries server state and skips chunks already present with matching SHA-256.

Related files:

- `src/network/BackupClient.cpp`
- `src/network/BackupServer.cpp`
- `src/network/SessionIndex.cpp`

## 8. What is the server concurrency model?

The server uses blocking sockets and a bounded thread pool. This keeps the implementation direct, but one active connection can occupy one worker. There is no epoll event loop.

Related files:

- `src/network/BackupServer.cpp`
- `include/dpc/concurrency/ThreadPool.hpp`
- `include/dpc/concurrency/BoundedQueue.hpp`

## 9. How is benchmark correctness checked?

`backup-bench` creates workload data, performs backup, verify, restore, compares source and restored file lists, compares file bytes, then prints metrics.

Related files:

- `src/bench/backup_bench_main.cpp`
- `docs/benchmark.md`

## 10. What are the current security limits?

The project currently does not implement authentication, authorization, encryption, TLS, key management, or access control. These should be treated as future work, not implied behavior.

Related files:

- `README.md`
- `docs/issue-backlog.md`
