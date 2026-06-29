# Benchmark

`backup-bench` 用於做可重現的本機 benchmark。它會建立暫存目錄、產生測試資料、執行 backup、verify、restore，最後比對 source 與 restore 的檔案清單和內容。只有 correctness check 通過後才輸出 benchmark metrics。

## Script

```bash
./scripts/bench.sh
DPC_BENCH_SIZE=128M ./scripts/bench.sh
DPC_BENCH_CHUNKING=cdc ./scripts/bench.sh
```

`scripts/bench.sh` 會先執行 `scripts/build.sh`，再呼叫：

```bash
build/bin/backup-bench --workload large-file --size "${DPC_BENCH_SIZE:-64M}" --chunking "${DPC_BENCH_CHUNKING:-fixed}"
```

## Direct Usage

```bash
build/bin/backup-bench --workload large-file --size 64M --chunking fixed
build/bin/backup-bench --workload small-files --files 1000 --file-size 4096 --chunking fixed
build/bin/backup-bench --workload duplicated --size 64M --chunking fixed
build/bin/backup-bench --workload modified --base-size 64M --chunking cdc
```

支援的 size suffix：`K`、`M`、`G`。

`backup-bench` 目前沒有 `--help`；參數解析位於 `src/bench/backup_bench_main.cpp`。

## Workloads

| Workload | 用途 |
| --- | --- |
| `large-file` | 產生單一大檔，觀察 sequential chunking、compression、verify、restore throughput |
| `small-files` | 產生多個小檔，觀察 file scanning、manifest metadata 與 per-file overhead |
| `duplicated` | 產生兩個內容 pattern 相同的檔案，觀察 chunk-level dedup |
| `modified` | 產生 `base.bin`（seed 11）與較大 1% 的 `modified.bin`（seed 12）；可比較兩種 chunking 在此 synthetic input 的輸出，但不是版本間增量備份測試 |

## Correctness Flow

`backup-bench` 的順序：

1. 建立 `/tmp/dpc-bench-<pid>`。
2. 產生 source workload。
3. 執行 `BackupEngine::create`。
4. 執行 `VerifyEngine::verify`。
5. 執行 `RestoreEngine::restore`。
6. 比對 source 與 restore 的 regular file list。
7. 逐檔比對 bytes。
8. correctness 通過後輸出 metrics。
9. 成功時清理暫存目錄。

任何階段失敗都會輸出英文錯誤訊息並回傳 non-zero exit code。

## Metrics

範例輸出：

```text
workload: large-file
chunking mode: fixed
total input size: 67108864
stored object size: 6523
dedup ratio: 1024.000
compression ratio: 0.000
backup throughput MB/s: 123.456
restore throughput MB/s: 456.789
verify throughput MB/s: 789.012
file count: 1
chunk count: 1024
unique chunk count: 1
duplicate chunk count: 1023
elapsed time: 0.123
```

Metric 定義：

| Metric | 定義 |
| --- | --- |
| `workload` | 產生資料的 workload 名稱 |
| `chunking mode` | `fixed` 或 `cdc` |
| `total input size` | source files 的總 bytes |
| `stored object size` | repository `objects/` 內 regular file bytes 加總 |
| `dedup ratio` | `chunk count / unique chunk count` |
| `compression ratio` | `stored object size / total input size` |
| `backup throughput MB/s` | input MiB / backup elapsed seconds |
| `restore throughput MB/s` | input MiB / restore elapsed seconds |
| `verify throughput MB/s` | input MiB / verify elapsed seconds |
| `file count` | manifest 中的檔案數 |
| `chunk count` | chunk reference 數 |
| `unique chunk count` | 此次 create 新寫入的 object 數；benchmark 使用全新 repository，因此等同該次資料的 unique object 數 |
| `duplicate chunk count` | 此次 create 遇到既有 object 的 chunk reference 數 |
| `elapsed time` | backup + verify + restore 秒數 |

## Fixed-size 與 CDC 比較

同一個 workload 可分別跑 fixed 與 cdc：

```bash
build/bin/backup-bench --workload modified --base-size 128M --chunking fixed
build/bin/backup-bench --workload modified --base-size 128M --chunking cdc
```

比較時應使用同一台機器、同一份 binary、同一個 workload，並檢查：

- `dedup ratio`
- `stored object size`
- `backup throughput MB/s`
- `chunk count`
- `unique chunk count`

`modified` workload 不是對既有 repository 建立第二個版本，也沒有在同一份檔案中央插入資料；它不驗證跨版本增量變更的去重率。單次結果也不代表固定性能比例，OS page cache、CPU、storage、compiler、input size 都會影響數值。

## CDC 限制

目前 `ContentDefinedChunker` 使用簡化 rolling hash：

```text
min size = 16 KiB
avg size = 64 KiB
max size = 256 KiB
```

它不是 Rabin fingerprint，程式也沒有提供 CDC 參數選項或基準資料集。輸出只代表上述 synthetic workload 與目前執行環境。
