# Backup Format

Repository 是一般檔案目錄，由 `ObjectStore`、`ManifestStore` 與 metadata 模組共同寫入。格式目前沒有版本遷移工具；文件描述的是目前程式碼使用的格式。

## Repo Layout

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

`ObjectStore::ensureLayout` 會建立這些目錄。`tmp/` 用於 manifest staging 與其他暫存寫入。

## Object Store

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

如果同一個 object path 已存在，`ObjectStore` 會視為 dedup hit，不重寫內容。

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

`backupctl list` 只列出存在 commit marker 的 manifest。`verify` 與 `restore` 會讀取 manifest，依 object path 讀取 object，再檢查 chunk 與 file checksum。

## Commit Marker

`version-000001.commit` 代表該 version 已完成 commit。crash 發生在 manifest rename 之後、commit marker 之前時，`recover` 不會把該 version 視為 committed。crash 發生在 commit marker 之後時，`recover` 可以重新建立 checkpoint。

## WAL

WAL 使用 binary record，由 `WalLog` 寫入。header 結構對應 `include/dpc/metadata/WalLog.hpp`：

```cpp
struct WalRecordHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_size;
    uint32_t crc32;
};
```

目前 record type：

- `BEGIN_BACKUP`
- `PUT_OBJECT`
- `WRITE_MANIFEST`
- `RENAME_MANIFEST`
- `COMMIT_BACKUP`
- `CHECKPOINT`
- `COMPACT`

`WalLog::readAll` 會檢查 magic、version、payload size 與 CRC。遇到截斷或損毀 record 時會丟出錯誤，不會靜默當作成功 recovery。

## Checkpoint

`metadata/checkpoint.dat` 是文字格式，紀錄 latest committed version 與 committed versions。`Checkpoint::write` 使用 atomic write。

## Session Index

TCP server 將尚未 commit 的 session chunk 寫入：

```text
metadata/sessions/<hex-session-id>.session
```

`SessionIndex` 記錄 global chunk index、chunk hash、file metadata 與 object path。client 使用相同 session id 再次上傳時，server 回覆已收到的 chunk，client 跳過相同 hash 的 chunk。
