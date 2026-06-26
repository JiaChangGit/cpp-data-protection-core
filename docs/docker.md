# Docker

Docker 設定包含 multi-stage `Dockerfile`、`docker-compose.yml` 與四個 helper script。Docker 主要用於建置、測試與 demo，不包含 production hardening、SBOM 或 image signing。

## Dockerfile Stages

| Stage | 用途 |
| --- | --- |
| `dev` | 安裝 compiler、CMake、Ninja、OpenSSL/zstd headers、GTest、gdb、clang-tidy、cppcheck、gcovr |
| `test` | 複製 source，建置專案，執行 unit/integration/fault/local/crash demo 與小型 benchmark |
| `build` | Release build，`DPC_BUILD_TESTS=OFF`，安裝 binary 到 `/opt/dpc` |
| `runtime` | 安裝 runtime libraries，複製 `backupctl`、`backup-client`、`backup-server`、`backup-bench`、README 與 docs |

## Build Images

```bash
./scripts/docker_build.sh
```

產生 image：

```text
cpp-data-protection-core:dev
cpp-data-protection-core:test
cpp-data-protection-core:runtime
```

只建 runtime image：

```bash
docker build --target runtime -t cpp-data-protection-core:runtime .
```

## Docker Test

```bash
./scripts/docker_test.sh
```

此 script 會：

1. 建置 `test` image。
2. 在 container 內執行 `./scripts/test.sh`。
3. 執行 `./scripts/demo_local_backup.sh`。
4. 執行 `./scripts/demo_crash_recovery.sh`。

## Docker Compose Demo

```bash
./scripts/docker_demo.sh
```

`docker-compose.yml` 只有一個 `demo-runner` service。container 內部會啟動 `backup-server` 背景 process，然後在同一個 container 內執行 client/server demo。

流程：

1. 建立 `/tmp/dpc-compose/source`。
2. 啟動 `backup-server --repo /data/repo --port 19090 --threads 4`。
3. 等待 server ready。
4. 執行正常上傳並驗證 version 1。
5. 使用 `--exit-after-chunks 10` 中斷一次上傳。
6. 使用同一個 session id 再次上傳，補齊缺少的 chunk。
7. 驗證 version 2。
8. 還原到 `/tmp/dpc-compose/restore`。
9. 執行 `diff -r`。

成功輸出：

```text
docker compose client/server demo ok
```

變更 host port：

```bash
DPC_DEMO_PORT=19190 ./scripts/docker_demo.sh
```

## Runtime Image Checks

```bash
docker run --rm cpp-data-protection-core:runtime backupctl --help
docker run --rm cpp-data-protection-core:runtime backup-client --help
docker run --rm cpp-data-protection-core:runtime backup-server --help
```

`backup-bench` 目前沒有 `--help`，請使用 [benchmark.md](benchmark.md) 的參數範例。

## Cleanup

清理 Compose container 與 volume：

```bash
./scripts/docker_clean.sh
```

同時移除專案 image：

```bash
./scripts/docker_clean.sh --images
```

`--images` 只移除 `cpp-data-protection-core:dev`、`:test`、`:runtime`。

## Troubleshooting

Docker daemon：

```bash
docker ps
```

Port already in use：

```bash
DPC_DEMO_PORT=19190 ./scripts/docker_demo.sh
```

查看 Compose log：

```bash
docker compose -p cpp-data-protection-core logs
```

Docker workflow 說明見 [cicd.md](cicd.md)。
