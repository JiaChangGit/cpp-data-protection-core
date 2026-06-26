# Docker Status

本文列出目前 Docker 交付狀態。詳細操作見 [docker.md](docker.md)。

## 已存在的檔案

- `Dockerfile`
- `.dockerignore`
- `docker-compose.yml`
- `scripts/docker_build.sh`
- `scripts/docker_test.sh`
- `scripts/docker_demo.sh`
- `scripts/docker_clean.sh`
- `.github/workflows/docker.yml`

## 已驗證流程

```bash
./scripts/docker_build.sh
./scripts/docker_test.sh
./scripts/docker_demo.sh
docker run --rm cpp-data-protection-core:runtime backupctl --help
docker run --rm cpp-data-protection-core:runtime backup-client --help
docker run --rm cpp-data-protection-core:runtime backup-server --help
```

## Image Tags

Local scripts 使用：

```text
cpp-data-protection-core:dev
cpp-data-protection-core:test
cpp-data-protection-core:runtime
```

GitHub Actions 在 `main` push 或 manual dispatch 時可推送。Docker image reference 使用小寫 owner，因此 `JiaChangGit` 會對應到 `jiachanggit`：

```text
ghcr.io/jiachanggit/cpp-data-protection-core:latest
ghcr.io/jiachanggit/cpp-data-protection-core:<git-sha>
```

## 目前未涵蓋

- distroless runtime base image
- SBOM
- image signing
- SLSA provenance
- multi-arch image
- Docker Hub mirror
