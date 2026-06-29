# 切塊與去重

本機備份與 benchmark 支援兩種 chunking mode：`fixed` 與 `cdc`。選擇點在 `backupctl create --chunking fixed|cdc` 或 `backup-bench --chunking fixed|cdc`，實作對應 `FixedChunker` 與 `ContentDefinedChunker`。Client/server 上傳固定使用 `FixedChunker`，沒有 chunking 參數。

## 固定大小切塊（Fixed-size Chunking）

`FixedChunker` 預設 chunk size 為 64 KiB：

```cpp
static constexpr std::size_t kDefaultChunkSize = 64 * 1024;
```

行為：

- 依固定 offset 切 chunk。
- 空檔案會產生一個空 chunk。
- 同內容的 raw chunk 會得到相同 SHA-256，寫入同一個 object path。

此模式不檢查資料內容來決定 boundary。

## 內容定義切塊（Content-defined Chunking）

`ContentDefinedChunker` 的預設參數：

```text
min size = 16 KiB
avg size = 64 KiB
max size = 256 KiB
```

實作使用簡化 rolling hash 判斷 boundary：

```text
boundary = len >= min_size && ((rolling_hash & mask) == 0)
```

這不是 Rabin fingerprint。Boundary 只由目前實作中的 rolling state、最小大小、mask 與最大大小決定。

## 去重（Deduplication）

Dedup 發生在 `ObjectStore`：

1. `BackupEngine` 對 raw chunk 計算 SHA-256。
2. `ObjectStore` 以 `objects/<first-two-hex>/<sha256>.zst` 作為 object path。
3. 如果 object 已存在，`ObjectStore` 會讀取並驗證既有 object；hash 與 raw size 相符時不重寫 object。
4. Manifest 仍會記錄每個檔案的 chunk reference。

`backupctl stats` 會輸出：

```text
total_chunks: 2
unique_chunks: 1
duplicate_chunks: 1
dedup_ratio: 2.000
compression_ratio: 0.123
```

## 統計欄位範圍

- `backupctl stats` 的 `unique_chunks` 是所有 committed manifests 中不同 SHA-256 的數量。
- `backupctl stats` 的 `objects` 是 `repo/objects` 下 regular files 的數量；孤立 object 可能使它與 `unique_chunks` 不同。
- `BackupResult::unique_chunks` 與 benchmark 的 `unique chunk count` 只計算該次 create 新寫入的 object。
- Dedup 只在單一 repository 內依 chunk hash 發生，沒有跨 repository index。
- CDC 使用簡化 rolling hash，未提供 Rabin fingerprint 或參數調整介面。
