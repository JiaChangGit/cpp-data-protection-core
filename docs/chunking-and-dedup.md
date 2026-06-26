# Chunking and Dedup

本專案在本機備份流程支援兩種 chunking mode：`fixed` 與 `cdc`。選擇點在 `backupctl create --chunking fixed|cdc`，實作對應 `FixedChunker` 與 `ContentDefinedChunker`。

## Fixed-size Chunking

`FixedChunker` 預設 chunk size 為 64 KiB：

```cpp
static constexpr std::size_t kDefaultChunkSize = 64 * 1024;
```

行為：

- 依固定 offset 切 chunk。
- 空檔案會產生一個空 chunk。
- 同內容的 raw chunk 會得到相同 SHA-256，寫入同一個 object path。

適用情境是資料變動位置穩定、需要較低 CPU overhead 的備份。

## Content-defined Chunking

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

這不是 Rabin fingerprint。它提供內容定義切塊的基本行為，讓插入或刪除少量 bytes 後仍可能維持部分 chunk boundary。

## Dedup

Dedup 發生在 `ObjectStore`：

1. `BackupEngine` 對 raw chunk 計算 SHA-256。
2. `ObjectStore` 以 `objects/<first-two-hex>/<sha256>.zst` 作為 object path。
3. 如果 object 已存在，視為 duplicate chunk，不重寫 object。
4. Manifest 仍會記錄每個檔案的 chunk reference。

`backupctl stats` 會輸出：

```text
total_chunks: 2
unique_chunks: 1
duplicate_chunks: 1
dedup_ratio: 2.000
compression_ratio: 0.123
```

## 已知限制

- `backup-bench` 目前固定使用 `ChunkingMode::Fixed`，不提供 CDC benchmark 比較。
- CDC 的 rolling hash 是簡化實作。
- Dedup 以 chunk hash 為單位，沒有跨 repository 的全域索引。
