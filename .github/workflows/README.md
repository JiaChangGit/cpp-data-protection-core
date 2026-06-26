# GitHub Actions Workflows

本目錄包含目前 repository 使用的 workflow YAML。

## Workflows

| File | Purpose |
| --- | --- |
| `ci.yml` | GCC/Clang build、unit tests、integration test、fault injection test、security malformed input test、benchmark smoke test、cppcheck、scoped clang-tidy |
| `sanitizer.yml` | ASan/UBSan full test、TSan unit test workflow |
| `docker.yml` | Docker test/runtime image build、Docker tests、Compose demo、GHCR publish on `main` push or manual dispatch |
| `release.yml` | `v*` tag release，產生 Linux x86_64 tar.gz 與 sha256 |
| `codeql.yml` | GitHub CodeQL C/C++ analysis |

## GHCR Permission

`docker.yml` 使用 `GITHUB_TOKEN` 推送 GHCR image，不需要額外 secret。repository 需要允許 workflow write permission。因為 Docker image reference 必須是小寫，workflow 會把 `JiaChangGit` 轉成 `jiachanggit` 後組出 GHCR tag：

```text
ghcr.io/jiachanggit/cpp-data-protection-core:latest
ghcr.io/jiachanggit/cpp-data-protection-core:<git-sha>
```

repository 需要設定：

1. GitHub repository -> Settings -> Actions -> General
2. Workflow permissions -> Read and write permissions
3. Workflow YAML 內保留：

```yaml
permissions:
  contents: read
  packages: write
```

## Release

Release workflow 由 tag 觸發：

```bash
git tag v1.0.0
git push origin v1.0.0
```

Artifact：

```text
cpp-data-protection-core-linux-x86_64.tar.gz
cpp-data-protection-core-linux-x86_64.sha256
```

更多說明見 [../../docs/cicd.md](../../docs/cicd.md) 與 [../../docs/release.md](../../docs/release.md)。
