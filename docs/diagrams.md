# 架構與流程圖

本文件的節點只包含目前程式碼、腳本與 repository 格式中存在的元件。

## 系統架構圖（System Architecture Diagram）

```mermaid
flowchart LR
    subgraph Entry["Command-line entry points"]
        CTL["backupctl"]
        Client["backup-client"]
        Server["backup-server"]
        Bench["backup-bench"]
    end

    CTL --> Core["core engines"]
    CTL --> MetaCmd["metadata commands"]
    Bench --> Core
    Client --> NetClient["TransferSession / PacketCodec"]
    NetClient -->|"TCP"| Server
    Server --> NetServer["BackupServer / SessionIndex / ThreadPool"]

    Core --> Store["ObjectStore / ManifestStore"]
    Core --> Metadata["WalLog / CommitMarker / Checkpoint"]
    MetaCmd --> Metadata
    MetaCmd --> Store
    NetServer --> Store
    NetServer --> Metadata
    Store --> Repo["repository filesystem"]
    Metadata --> Repo
```

此圖對應 `src/cli/*`、`src/bench/backup_bench_main.cpp`、`src/core/*`、`src/network/*`、`src/metadata/*` 與 `src/concurrency/*`。`corrupt-repo` 未列入主要入口，因為它只在 WAL 尾端追加測試資料。

## 資料流圖（Data Flow Diagram）

```mermaid
flowchart TD
    Input["source directory"] --> Scan["FileScanner"]
    Scan --> Entry{"entry point"}
    Entry -->|"backupctl / backup-bench"| LocalChunk["FixedChunker / ContentDefinedChunker"]
    Entry -->|"backup-client"| Transfer["TransferSession + FixedChunker"]

    LocalChunk --> PutRaw["ObjectStore::putRaw"]
    PutRaw --> Hash["SHA-256 raw bytes"]
    PutRaw --> Compress["zstd compress"]

    Transfer --> Client["backup-client"]
    Client --> Packet["PacketCodec"]
    Packet --> Server["BackupServer"]
    Server --> Decompress["zstd decompress + SHA-256 validation"]
    Decompress --> PutCompressed["ObjectStore::putCompressed"]

    Hash --> Object["objects/<prefix>/<sha>.zst"]
    Compress --> Object
    PutCompressed --> Object
    Object --> Manifest["version manifest"]
    Manifest --> Commit["commit marker"]
    Commit --> Verify["VerifyEngine"]
    Commit --> Restore["RestoreEngine"]
    Restore --> Output["target directory"]
```

本機路徑對應 `BackupEngine::create` 與 `ObjectStore::putRaw`。網路路徑對應 `TransferSession::prepareChunks`、`BackupClient::upload`、`BackupServer::handlePacket` 與 `ObjectStore::putCompressed`。

## 建立本機備份時序圖（Sequence Diagram）

```mermaid
sequenceDiagram
    participant CLI as "backupctl"
    participant BE as "BackupEngine"
    participant WAL as "WalLog"
    participant OS as "ObjectStore"
    participant MS as "ManifestStore"
    participant CM as "CommitMarker"
    participant CP as "Checkpoint"

    CLI->>BE: create(source, repo, mode)
    BE->>WAL: BEGIN_BACKUP
    loop each regular file and chunk
        BE->>OS: putRaw(chunk bytes)
        OS-->>BE: PutObjectResult
        BE->>WAL: PUT_OBJECT
    end
    BE->>MS: writeTmp(version, manifest)
    BE->>WAL: WRITE_MANIFEST
    BE->>MS: renameTmpToManifest(version)
    BE->>WAL: RENAME_MANIFEST
    BE->>CM: create(version)
    BE->>WAL: COMMIT_BACKUP
    BE->>CP: write(committed versions)
    BE-->>CLI: BackupResult
```

此圖對應 `src/core/BackupEngine.cpp`、`src/core/ObjectStore.cpp`、`src/core/ManifestStore.cpp` 與 `src/metadata/{WalLog,CommitMarker,Checkpoint}.cpp`。`after-commit-marker` fault 會在 commit marker 建立後、`COMMIT_BACKUP` record 寫入前中止。

## 網路上傳時序圖

```mermaid
sequenceDiagram
    participant C as "BackupClient"
    participant S as "BackupServer"
    participant SI as "SessionIndex"
    participant OS as "ObjectStore"
    participant WAL as "WalLog"
    participant MS as "ManifestStore"
    participant CM as "CommitMarker"
    participant CP as "Checkpoint"

    C->>S: HELLO
    S-->>C: HELLO
    C->>S: QUERY_SESSION(session id)
    S->>SI: received()
    SI-->>S: global index -> SHA-256
    S-->>C: SESSION_STATUS
    loop each missing chunk
        C->>S: PUT_CHUNK(metadata + zstd payload)
        S->>S: decompress and verify SHA-256
        S->>OS: putCompressed()
        S->>SI: appendChunk()
        S-->>C: PUT_CHUNK_ACK
    end
    C->>S: COMMIT_SESSION(total chunks)
    S->>WAL: BEGIN_BACKUP
    S->>SI: buildManifest()
    S->>MS: writeTmp + rename
    S->>WAL: WRITE_MANIFEST + RENAME_MANIFEST
    S->>CM: create(version)
    S->>WAL: COMMIT_BACKUP
    S->>CP: write(committed versions)
    S->>SI: markCommitted(version)
    S-->>C: COMMIT_ACK
```

此圖對應 `src/network/BackupClient.cpp`、`src/network/BackupServer.cpp`、`src/network/SessionIndex.cpp`、`src/core/{ObjectStore,ManifestStore}.cpp` 與 metadata 模組。Client/server 上傳固定使用 `FixedChunker`。

## 模組關係圖（Module Diagram）

```mermaid
classDiagram
    BackupEngine --> FileScanner
    BackupEngine --> FixedChunker
    BackupEngine --> ContentDefinedChunker
    BackupEngine --> ObjectStore
    BackupEngine --> ManifestStore
    BackupEngine --> WalLog
    BackupEngine --> CommitMarker
    BackupEngine --> Checkpoint
    ObjectStore --> Compressor
    RestoreEngine --> ManifestStore
    RestoreEngine --> ObjectStore
    VerifyEngine --> ManifestStore
    VerifyEngine --> ObjectStore
    RecoveryManager --> WalLog
    RecoveryManager --> ManifestStore
    RecoveryManager --> Checkpoint
    MetadataCompactor --> ManifestStore
    MetadataCompactor --> Checkpoint
    MetadataCompactor --> WalLog
    BackupClient --> TransferSession
    BackupClient --> PacketCodec
    TransferSession --> FixedChunker
    BackupServer --> PacketCodec
    BackupServer --> SessionIndex
    BackupServer --> ObjectStore
    BackupServer --> ManifestStore
    BackupServer --> WalLog
    BackupServer --> ThreadPool
    ThreadPool --> BoundedQueue
```

此圖對應 `include/dpc` 下各模組的標頭與 `src` 下的同名實作。`BackupEngine` 不呼叫 `RecoveryManager`；recovery 是獨立的 `backupctl recover` 命令。

## Recovery 流程圖

```mermaid
flowchart TD
    Start["backupctl recover"] --> Layout["ensure repository directories"]
    Layout --> Read["WalLog::readAll"]
    Read --> Valid{"all records structurally valid?"}
    Valid -- "no" --> Fail["return non-zero; do not clean tmp"]
    Valid -- "yes" --> Clean["remove regular files under repo/tmp"]
    Clean --> List["ManifestStore::listCommittedVersions"]
    List --> CP["Checkpoint::write"]
    CP --> Report["print recovery summary"]
```

此圖對應 `src/metadata/RecoveryManager.cpp` 與 `src/metadata/WalLog.cpp`。Recovery 不依 WAL payload 重播 object 或 manifest 操作，也不依 `COMMIT_BACKUP` record 單獨建立可見版本。

## Repository 目錄圖

```mermaid
flowchart TD
    Repo["repo"] --> Objects["objects"]
    Objects --> Prefix["<first-two-hex>"]
    Prefix --> Zst["<sha256>.zst"]
    Repo --> Manifests["manifests"]
    Manifests --> Manifest["version-000001.manifest"]
    Manifests --> Commit["version-000001.commit"]
    Repo --> Metadata["metadata"]
    Metadata --> WAL["wal.log"]
    Metadata --> Checkpoint["checkpoint.dat"]
    Metadata --> Sessions["sessions/<hex-session-id>.session"]
    Repo --> Tmp["tmp"]
```

目錄與格式說明見 [backup-format.md](backup-format.md)。
