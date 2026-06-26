# Release

Release workflow 由 Git tag 觸發。它會在 GitHub hosted `ubuntu-24.04` runner 上建置、測試、打包 Linux x86_64 binary，並建立 GitHub Release。

## Trigger

```bash
git tag v1.0.0
git push origin v1.0.0
```

Tag 需符合 `v*`，例如 `v1.0.0` 或 `v1.1.0-rc1`。

## Workflow

`.github/workflows/release.yml` 的流程：

1. checkout repository
2. install `build-essential`、`cmake`、`ninja-build`、`libssl-dev`、`libzstd-dev`、`libgtest-dev`
3. 使用 `CMAKE_GENERATOR=Ninja` 與 `CMAKE_BUILD_TYPE=Release` 執行 `./scripts/test.sh`
4. 複製 binary、README 與 docs 到 `release-staging`
5. 建立 tar.gz
6. 產生 sha256
7. 使用 `softprops/action-gh-release@v2` 建立 GitHub Release

如果 build 或 test 失敗，release artifact 不會產生。

## Artifacts

```text
cpp-data-protection-core-linux-x86_64.tar.gz
cpp-data-protection-core-linux-x86_64.sha256
```

Tarball 內容：

```text
bin/backupctl
bin/backup-client
bin/backup-server
bin/backup-bench
README.md
docs/
```

`corrupt-repo` 是 build 產物，但目前 release workflow 不打包它。

## Checksum

下載 artifact 後可驗證：

```bash
sha256sum -c cpp-data-protection-core-linux-x86_64.sha256
```

成功輸出範例：

```text
cpp-data-protection-core-linux-x86_64.tar.gz: OK
```

## Troubleshooting

- Release 沒有觸發：確認 tag 名稱符合 `v*`。
- Build/test 失敗：先在本機執行 `./scripts/test.sh`。
- Artifact step 失敗：確認 `build/bin/backupctl`、`backup-client`、`backup-server`、`backup-bench` 存在。
- GitHub Release step 失敗：確認 workflow 有 `contents: write` permission。
