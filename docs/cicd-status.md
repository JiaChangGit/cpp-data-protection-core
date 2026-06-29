# CI/CD 配置摘要

本文列出 repository 內的 CI/CD workflows 與各自定義的驗證工作。詳細說明見 [cicd.md](cicd.md)，release 說明見 [release.md](release.md)。

## Workflows

- `.github/workflows/ci.yml`
- `.github/workflows/sanitizer.yml`
- `.github/workflows/docker.yml`
- `.github/workflows/release.yml`
- `.github/workflows/codeql.yml`

## Workflow jobs

- GCC build and unit tests
- Clang build and unit tests
- Integration test
- Fault injection test
- Security malformed input test
- Benchmark smoke test
- ASan/UBSan test
- TSan unit test workflow
- Docker test image
- Docker Compose demo
- CodeQL C/C++ analysis
- Release tar.gz and sha256 artifact

## GHCR

`docker.yml` 可在 `main` push 或 manual dispatch 時推送。Docker image reference 必須是小寫，因此 `JiaChangGit` 會在 workflow 中轉成 `jiachanggit`：

```text
ghcr.io/jiachanggit/cpp-data-protection-core:latest
ghcr.io/jiachanggit/cpp-data-protection-core:<git-sha>
```

Repository 需要啟用 GitHub Actions write permission，workflow 已設定：

```yaml
permissions:
  contents: read
  packages: write
```

## 目前未涵蓋

- nightly stress workflow
- performance regression baseline
- Docker Hub publish
- SBOM upload
- image signing
- SLSA provenance
