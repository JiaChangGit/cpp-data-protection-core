# Diagrams

本文件集中放置目前架構與主要流程圖。圖中的節點都對應目前 repository 中存在的程式碼、腳本或檔案格式。

## System Architecture Diagram

```mermaid
flowchart TD
    CLI["CLI binaries<br/>src/cli"] --> Core["core<br/>src/core"]
    CLI --> Bench["backup-bench<br/>src/bench"]
    CLI --> Net["network<br/>src/network"]
    Core --> Common["common<br/>include/dpc/common"]
    Net --> Common
    Core --> Obj["ObjectStore"]
    Core --> Man["ManifestStore"]
    Core --> Meta["metadata<br/>src/metadata"]
    Net --> Obj
    Net --> Man
    Net --> Meta
    Net --> Conc["concurrency<br/>src/concurrency"]
    Obj --> Repo["repo/objects"]
    Man --> RepoM["repo/manifests"]
    Meta --> RepoMeta["repo/metadata"]
```

對應檔案：`src/cli/*`、`src/core/*`、`src/network/*`、`src/metadata/*`、`src/concurrency/*`。

## Data Flow Diagram

```mermaid
flowchart LR
    Input["source directory"] --> Scan["FileScanner"]
    Scan --> Chunk["FixedChunker / ContentDefinedChunker"]
    Chunk --> RawHash["Hash::sha256Hex(raw chunk)"]
    Chunk --> Zstd["Compressor::compress"]
    RawHash --> Path["objects/<prefix>/<sha>.zst"]
    Zstd --> Obj["ObjectStore::putCompressed"]
    Path --> Manifest["ManifestStore"]
    Obj --> Manifest
    Manifest --> Commit["CommitMarker"]
    Commit --> Verify["VerifyEngine"]
    Commit --> Restore["RestoreEngine"]
```

對應流程：`BackupEngine::create`、`VerifyEngine::verify`、`RestoreEngine::restore`。

## Create Backup Sequence Diagram

```mermaid
sequenceDiagram
    participant CLI as "backupctl"
    participant BE as "BackupEngine"
    participant WAL as "WalLog"
    participant OS as "ObjectStore"
    participant MS as "ManifestStore"
    participant CM as "CommitMarker"
    participant CP as "Checkpoint"

    CLI->>BE: create(source, repo, chunking)
    BE->>WAL: BEGIN_BACKUP
    loop each chunk
        BE->>OS: putCompressed(sha, zstd bytes)
        BE->>WAL: PUT_OBJECT
    end
    BE->>MS: writeTmp(version, manifest)
    BE->>WAL: WRITE_MANIFEST
    BE->>MS: renameTmpToManifest(version)
    BE->>WAL: RENAME_MANIFEST
    BE->>CM: create(version)
    BE->>WAL: COMMIT_BACKUP
    BE->>CP: write(checkpoint)
```

對應檔案：`src/core/BackupEngine.cpp`、`src/core/ObjectStore.cpp`、`src/core/ManifestStore.cpp`、`src/metadata/*`。

## Network Upload Sequence Diagram

```mermaid
sequenceDiagram
    participant C as "backup-client"
    participant S as "backup-server"
    participant SI as "SessionIndex"
    participant OS as "ObjectStore"
    participant MS as "ManifestStore"

    C->>S: HELLO
    S-->>C: HELLO
    C->>S: QUERY_SESSION
    S->>SI: received()
    S-->>C: SESSION_STATUS
    loop missing chunks
        C->>S: PUT_CHUNK
        S->>OS: putCompressed()
        S->>SI: appendChunk()
        S-->>C: PUT_CHUNK_ACK
    end
    C->>S: COMMIT_SESSION
    S->>SI: buildManifest()
    S->>MS: write manifest and commit marker
    S-->>C: COMMIT_ACK
```

對應檔案：`src/network/BackupClient.cpp`、`src/network/BackupServer.cpp`、`src/network/SessionIndex.cpp`。

## Module Diagram

```mermaid
classDiagram
    class BackupEngine
    class RestoreEngine
    class VerifyEngine
    class FixedChunker
    class ContentDefinedChunker
    class Compressor
    class ObjectStore
    class ManifestStore
    class WalLog
    class RecoveryManager
    class BackupClient
    class BackupServer
    class PacketCodec
    class SessionIndex
    class ThreadPool
    class BoundedQueue

    BackupEngine --> FixedChunker
    BackupEngine --> ContentDefinedChunker
    BackupEngine --> Compressor
    BackupEngine --> ObjectStore
    BackupEngine --> ManifestStore
    BackupEngine --> WalLog
    BackupEngine --> RecoveryManager
    RestoreEngine --> ManifestStore
    RestoreEngine --> ObjectStore
    VerifyEngine --> ManifestStore
    VerifyEngine --> ObjectStore
    BackupClient --> PacketCodec
    BackupServer --> PacketCodec
    BackupServer --> SessionIndex
    BackupServer --> ThreadPool
    ThreadPool --> BoundedQueue
```

對應標頭：`include/dpc/core/*`、`include/dpc/metadata/*`、`include/dpc/network/*`、`include/dpc/concurrency/*`。

## Repository Layout Diagram

```mermaid
flowchart TD
    Repo["repo"] --> Obj["objects"]
    Obj --> Shard["ab"]
    Shard --> Zst["abcdef.zst"]
    Repo --> Man["manifests"]
    Man --> M1["version-000001.manifest"]
    Man --> C1["version-000001.commit"]
    Repo --> Meta["metadata"]
    Meta --> WAL["wal.log"]
    Meta --> CP["checkpoint.dat"]
    Meta --> Sessions["sessions/*.session"]
    Repo --> Tmp["tmp"]
```

對應格式：[backup-format.md](backup-format.md)。

## CLI To Internal API Diagram

```mermaid
flowchart TD
    Backupctl["backupctl<br/>src/cli/backupctl_main.cpp"] --> Create["BackupEngine::create"]
    Backupctl --> Restore["RestoreEngine::restore"]
    Backupctl --> Verify["VerifyEngine::verify"]
    Backupctl --> Recover["RecoveryManager::recover"]
    Backupctl --> Stats["ObjectStore::stats<br/>ManifestStore::load"]
    Client["backup-client<br/>src/cli/backup_client_main.cpp"] --> Upload["BackupClient::upload"]
    Server["backup-server<br/>src/cli/backup_server_main.cpp"] --> Run["BackupServer::run"]
    Bench["backup-bench<br/>src/bench/backup_bench_main.cpp"] --> BenchFlow["generate workload<br/>backup / verify / restore / compare"]

    Create --> CoreStore["ObjectStore / ManifestStore / WalLog"]
    Restore --> CoreStore
    Verify --> CoreStore
    Upload --> Transfer["TransferSession / PacketCodec"]
    Run --> Session["SessionIndex / PacketCodec / ObjectStore"]
```

此圖描述 CLI binary 如何進入目前 C++ 類別 API。它對應 `src/cli/*`、`include/dpc/core/*`、`include/dpc/network/*` 與 `src/bench/backup_bench_main.cpp`。目前沒有 REST/gRPC API；這裡的 API 指 repository 內部 C++ 介面。

## Backup Implementation Detail

```mermaid
sequenceDiagram
    participant CLI as "backupctl create"
    participant BE as "BackupEngine::create"
    participant FS as "FileScanner::scanRegularFiles"
    participant CH as "FixedChunker / ContentDefinedChunker"
    participant Hash as "Hash::sha256Hex"
    participant Zstd as "Compressor::compress"
    participant OS as "ObjectStore::putRaw"
    participant MS as "ManifestStore"
    participant WAL as "WalLog"
    participant CM as "CommitMarker"

    CLI->>BE: source, repo, chunking mode
    BE->>WAL: BeginBackup(version)
    BE->>FS: scan source directory
    loop each regular file
        BE->>CH: chunkFile(file)
        loop each chunk
            BE->>Hash: hash raw chunk
            BE->>Zstd: compress raw chunk
            BE->>OS: write objects/<prefix>/<sha>.zst
            BE->>WAL: PutObject
        end
        BE->>Hash: finalize file SHA-256
    end
    BE->>MS: write tmp manifest
    BE->>MS: rename tmp manifest
    BE->>CM: create commit marker
    BE->>WAL: CommitBackup(version)
```

對應檔案：`src/core/BackupEngine.cpp`、`src/core/FileScanner.cpp`、`src/core/FixedChunker.cpp`、`src/core/ContentDefinedChunker.cpp`、`src/core/ObjectStore.cpp`、`src/core/ManifestStore.cpp`。

## Restore Safety Sequence

```mermaid
sequenceDiagram
    participant CLI as "backupctl restore"
    participant RE as "RestoreEngine"
    participant MS as "ManifestStore::load"
    participant FU as "fileutil::safeRelativePath"
    participant OS as "ObjectStore::readRaw"
    participant Hash as "Hash::sha256Hex"
    participant Out as "target directory"

    CLI->>RE: repo, version, target
    RE->>MS: load committed manifest
    loop each file in manifest
        RE->>FU: validate relative_path
        FU-->>RE: safe path or error
        loop each chunk
            RE->>OS: readRaw(sha256, object_path)
            OS->>OS: recompute expected object path from sha256
            OS->>Hash: validate decompressed raw bytes
            OS-->>RE: raw chunk or error
        end
        RE->>Hash: validate restored file SHA-256
        RE->>Out: write file under target
    end
```

此圖對應 `src/core/RestoreEngine.cpp`、`src/core/ObjectStore.cpp`、`include/dpc/common/FileUtils.hpp`。manifest 內的 `relative_path` 與 `object_path` 都視為不可信輸入。

## Verify And Object Integrity Flow

```mermaid
flowchart TD
    Start["backupctl verify"] --> Load["ManifestStore::load(version)"]
    Load --> FileLoop["for each FileManifest"]
    FileLoop --> ChunkLoop["for each ChunkRef"]
    ChunkLoop --> ExpectedPath["ObjectStore::objectPathForHash(sha256)"]
    ExpectedPath --> ReadObj["read objects/<prefix>/<sha>.zst"]
    ReadObj --> Zstd["Compressor::decompress"]
    Zstd --> Hash["Hash::sha256Hex(raw chunk)"]
    Hash --> Match{"chunk hash matches?"}
    Match -- "no" --> Fail["verify fails"]
    Match -- "yes" --> FileHash["update file SHA-256"]
    FileHash --> FileMatch{"file hash matches?"}
    FileMatch -- "no" --> Fail
    FileMatch -- "yes" --> OK["verify_ok"]
```

對應檔案：`src/core/VerifyEngine.cpp`、`src/core/ObjectStore.cpp`、`src/core/Compressor.cpp`、`include/dpc/common/Hash.hpp`。

## Packet Encode / Decode Flow

```mermaid
flowchart LR
    Packet["Packet struct"] --> Encode["PacketCodec::encode"]
    Encode --> Header["explicit big-endian header fields"]
    Header --> Hcrc["header_crc32"]
    Hcrc --> Frame["64-byte header + payload"]
    Frame --> Write["PacketCodec::writePacket<br/>fileutil::writeAll"]
    Read["PacketCodec::readPacket<br/>readExact loop"] --> Decode["PacketCodec::decode"]
    Decode --> Checks["magic / version / type / size / CRC"]
    Checks --> Valid{"valid?"}
    Valid -- "no" --> Error["DpcError"]
    Valid -- "yes" --> PacketOut["Packet struct"]
```

對應檔案：`src/network/PacketCodec.cpp`、`include/dpc/network/PacketCodec.hpp`。此流程避免直接把 C++ struct 寫到 socket，避免 padding、endianness 與 ABI 差異。

## Client / Server API Chain

```mermaid
sequenceDiagram
    participant BC as "BackupClient::upload"
    participant TS as "TransferSession::prepareChunks"
    participant PC as "PacketCodec"
    participant BS as "BackupServer::handlePacket"
    participant SI as "SessionIndex"
    participant OS as "ObjectStore"
    participant MS as "ManifestStore"

    BC->>TS: prepare fixed-size chunks
    TS-->>BC: vector<PreparedChunk>
    BC->>PC: encode HELLO
    PC->>BS: TCP frame
    BS-->>BC: HELLO
    BC->>BS: QUERY_SESSION
    BS->>SI: received()
    SI-->>BS: map<global_index, sha256>
    BS-->>BC: SESSION_STATUS
    loop chunks not already received
        BC->>BS: PUT_CHUNK
        BS->>OS: putCompressed()
        BS->>SI: appendChunk()
        BS-->>BC: PUT_CHUNK_ACK
    end
    BC->>BS: COMMIT_SESSION
    BS->>SI: buildManifest()
    BS->>MS: write tmp manifest and commit
    BS-->>BC: COMMIT_ACK
```

對應檔案：`src/network/BackupClient.cpp`、`src/network/BackupServer.cpp`、`src/network/TransferSession.cpp`、`src/network/SessionIndex.cpp`。client/server transfer 目前使用 fixed-size chunks。

## WAL Recovery Decision Flow

```mermaid
flowchart TD
    Start["backupctl recover"] --> Read["WalLog::readAll"]
    Read --> Crc{"record CRC and size valid?"}
    Crc -- "no" --> Fail["recover fails cleanly"]
    Crc -- "yes" --> Replay["scan records"]
    Replay --> Begin{"BEGIN seen?"}
    Begin -- "no" --> CommitOnly{"COMMIT without BEGIN?"}
    CommitOnly -- "yes" --> Ignore["do not mark committed"]
    Begin -- "yes" --> Commit{"matching COMMIT seen?"}
    Commit -- "no" --> Cleanup["remove temporary files only"]
    Commit -- "yes" --> Visible["committed version remains visible"]
    Visible --> Checkpoint["rewrite checkpoint from committed versions"]
    Cleanup --> Checkpoint
    Ignore --> Checkpoint
```

對應檔案：`src/metadata/WalLog.cpp`、`src/metadata/RecoveryManager.cpp`、`tests/unit/test_wal.cpp`、`tests/fault_injection/crash_recovery_test.sh`。

## Benchmark Correctness Gate

```mermaid
flowchart TD
    Args["backup-bench args"] --> Temp["create temporary workspace"]
    Temp --> Workload["generate workload"]
    Workload --> Backup["BackupEngine::create"]
    Backup --> Verify["VerifyEngine::verify"]
    Verify --> Restore["RestoreEngine::restore"]
    Restore --> Compare["compare source and restored files"]
    Compare --> Pass{"same file list and bytes?"}
    Pass -- "no" --> Error["exit non-zero"]
    Pass -- "yes" --> Metrics["print benchmark metrics"]
```

對應檔案：`src/bench/backup_bench_main.cpp`、`scripts/bench.sh`、[benchmark.md](benchmark.md)。
