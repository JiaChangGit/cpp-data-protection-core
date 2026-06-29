# 備份格式

Repository 是一般檔案目錄，由 `ObjectStore`、`ManifestStore` 與 metadata 模組共同寫入。目前沒有格式遷移工具或跨版本 compatibility policy；下列內容只描述目前程式碼使用的格式。

## Repository 目錄

```text
repo/
├── objects/
│   └── ab/
│       └── abcdef....zst
├── manifests/
│   ├── version-000001.manifest
│   └── version-000001.commit
├── metadata/
│   ├── wal.log
│   ├── checkpoint.dat
│   └── sessions/
│       └── <hex-session-id>.session
└── tmp/
```

`ObjectStore::ensureLayout` 會建立這些目錄。`tmp/` 目前用於 staging manifest。

## Object store

Object identity 來自 raw chunk 的 SHA-256，不是壓縮後 bytes 的 hash。object path 格式：

```text
objects/<first-two-hex>/<sha256>.zst
```

寫入流程：

1. 建立 parent directory。
2. 將 zstd bytes 寫入 tmp file。
3. `fsync` tmp file。
4. `rename` 到正式 object path。
5. `fsync` parent directory。

如果同一個 object path 已存在，`ObjectStore` 會先讀取、解壓並驗證 hash 和 raw size；驗證成功後視為 dedup hit，不重寫內容。

## Manifest

Manifest 是文字格式，由 `ManifestStore` 寫入與解析。路徑欄位會使用 hex encoding，避免空白或特殊字元破壞欄位解析。

目前 manifest 包含：

- `version`
- `created_at`
- `chunking_mode`
- `source_root`
- `file_count`
- `total_input_bytes`
- `total_stored_bytes`
- 每個檔案的 relative path、size、mode、file SHA-256、chunk count
- 每個 chunk 的 index、raw size、compressed size、SHA-256、object path

`backupctl list` 由 `.commit` 檔名列出版本，再載入相同版本的 manifest。`verify` 與 `restore` 會由 chunk SHA-256 重算 expected object path，要求它與 manifest 中的 `object_path` 一致，再解壓並檢查 chunk 與 file checksum。

## Commit marker

`version-000001.commit` 代表該 version 已完成 commit。crash 發生在 manifest rename 之後、commit marker 之前時，`recover` 不會把該 version 視為 committed。crash 發生在 commit marker 之後時，`recover` 可以重新建立 checkpoint。

## WAL

WAL 使用 `WalLog` 編碼的 binary record。每筆 record 的 16-byte header 採 little-endian 整數欄位：

```text
offset  size  field
0       4     magic (0x44505741)
4       2     version (1)
6       2     record type
8       4     payload size
12      4     payload CRC-32
```

目前 record type：

- `BEGIN_BACKUP`
- `PUT_OBJECT`
- `WRITE_MANIFEST`
- `RENAME_MANIFEST`
- `COMMIT_BACKUP`
- `CHECKPOINT`
- `COMPACT`

`WalLog::readAll` 會檢查 magic、version、record type、payload size、record size 與 CRC。遇到截斷或損毀 record 時會回報錯誤；`RecoveryManager` 不會略過該 record，也不會依 payload 重播操作。

## Checkpoint

`metadata/checkpoint.dat` 是文字格式，紀錄 latest committed version 與 committed versions。`Checkpoint::write` 使用 atomic write。

## Session index

TCP server 將尚未 commit 的 session chunk 寫入：

```text
metadata/sessions/<hex-session-id>.session
```

`SessionIndex` 記錄 global chunk index、chunk hash、file metadata 與 object path。client 使用相同 session id 再次上傳時，server 回覆已收到的 chunk，client 跳過 global index 與 hash 都相符的 chunk。Commit 後會追加 `COMMITTED <version>`，檔案不會自動刪除。
