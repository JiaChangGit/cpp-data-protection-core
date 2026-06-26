# Issue Backlog

This backlog is a planning document. Items below are not completion claims.

Priority:

- P0: build/test/demo failure
- P1: correctness, data safety, or security issue
- P2: important quality improvement
- P3: nice-to-have
- P4: future exploration

## 1. Add property-style manifest parser cases

Title: Add property-style manifest parser cases  
Type: Test  
Priority: P2  
Background: Current malformed manifest tests cover selected deterministic cases. More combinations can improve parser confidence.  
Scope: Add generated-but-deterministic manifest strings for missing fields, extra fields, repeated sections, and out-of-order records.  
Non-goals: Do not introduce a full fuzzing framework in this issue.  
Implementation notes: Keep fixtures small and run under unit tests.  
Acceptance criteria: Parser rejects malformed inputs with `DpcError` and never crashes.  
Validation commands: `./scripts/test.sh`  
Related files: `tests/unit/test_manifest.cpp`, `src/core/ManifestStore.cpp`

## 2. Add restore overwrite policy tests

Title: Add restore overwrite policy tests  
Type: Bug  
Priority: P2  
Background: Restore writes files to the target directory. Existing tests validate content but do not cover pre-existing target files.  
Scope: Define and test current behavior when target files already exist.  
Non-goals: Do not redesign restore CLI.  
Implementation notes: Add integration cases with existing target files and directories.  
Acceptance criteria: Behavior is documented and tested.  
Validation commands: `./scripts/test.sh`  
Related files: `src/core/RestoreEngine.cpp`, `tests/integration/backup_restore_verify_test.sh`

## 3. Harden session index parsing

Title: Harden session index parsing  
Type: Security  
Priority: P1  
Background: `SessionIndex` parses server-side session files. Malformed session files should fail cleanly.  
Scope: Add tests for invalid tags, invalid numbers, hash mismatch, duplicate chunk index mismatch, and truncated records.  
Non-goals: Do not change session file format unless required for correctness.  
Implementation notes: Use deterministic tests under `tests/security` or unit tests.  
Acceptance criteria: Server-side parsing rejects malformed session index without crashing.  
Validation commands: `./scripts/test.sh`  
Related files: `src/network/SessionIndex.cpp`, `tests/security/security_malformed_test.sh`

## 4. Add corrupted zstd fixture variants

Title: Add corrupted zstd fixture variants  
Type: Robustness  
Priority: P2  
Background: Current object corruption test overwrites an object with invalid bytes. More corruption forms can be covered.  
Scope: Test truncated zstd frame, modified valid frame, missing object, and wrong raw hash.  
Non-goals: Do not add randomized fuzzing in this issue.  
Implementation notes: Generate fixtures from a real backup repository.  
Acceptance criteria: `verify` and `restore` return non-zero for each corruption case.  
Validation commands: `./scripts/test.sh`  
Related files: `tests/security/security_malformed_test.sh`, `src/core/ObjectStore.cpp`

## 5. Add packet read/write socketpair tests

Title: Add packet read/write socketpair tests  
Type: Test  
Priority: P2  
Background: `PacketCodec::decode` is covered, but socket read/write loops should also be tested.  
Scope: Use `socketpair` to test partial writes, socket close during payload, and oversized payload from stream.  
Non-goals: Do not start a real TCP server.  
Implementation notes: Add unit tests that write partial frames into one end of a socketpair.  
Acceptance criteria: `readPacket` handles partial stream behavior and fails cleanly on truncation.  
Validation commands: `./scripts/test.sh`  
Related files: `src/network/PacketCodec.cpp`, `tests/unit/test_packet_codec.cpp`

## 6. Add WAL semantic validation report

Title: Add WAL semantic validation report  
Type: Robustness  
Priority: P3  
Background: Recovery validates WAL records and relies on commit markers for visibility. A diagnostic report can make WAL-only states easier to inspect.  
Scope: Add optional internal reporting for BEGIN without COMMIT and COMMIT without BEGIN.  
Non-goals: Do not make WAL the source of truth for committed versions.  
Implementation notes: Keep CLI output stable unless a new diagnostic option is added.  
Acceptance criteria: Tests show no uncommitted backup is exposed as committed.  
Validation commands: `./scripts/test.sh`  
Related files: `src/metadata/RecoveryManager.cpp`, `tests/unit/test_wal.cpp`

## 7. Add benchmark regression fixture

Title: Add benchmark regression fixture  
Type: Performance  
Priority: P3  
Background: Benchmark output is real but not compared against historical baselines.  
Scope: Add optional JSON or text output suitable for storing CI artifacts.  
Non-goals: Do not fail CI on performance changes yet.  
Implementation notes: Keep human-readable output unchanged or add an opt-in flag.  
Acceptance criteria: A script can capture benchmark metrics for later comparison.  
Validation commands: `DPC_BENCH_SIZE=8M ./scripts/bench.sh`  
Related files: `src/bench/backup_bench_main.cpp`, `scripts/bench.sh`, `docs/benchmark.md`

## 8. Add CDC benchmark comparison script

Title: Add CDC benchmark comparison script  
Type: Performance  
Priority: P3  
Background: Manual fixed-vs-CDC comparison is documented, but no helper script runs both modes.  
Scope: Add a script that runs selected workloads with `--chunking fixed` and `--chunking cdc`.  
Non-goals: Do not claim fixed performance improvements.  
Implementation notes: Keep workload size small by default.  
Acceptance criteria: Script produces two comparable metric blocks and exits non-zero on correctness failure.  
Validation commands: `./scripts/bench_compare.sh`  
Related files: `src/bench/backup_bench_main.cpp`, `docs/benchmark.md`

## 9. Centralize big-endian helpers

Title: Centralize big-endian helpers  
Type: Refactor  
Priority: P2  
Background: Packet codec uses local big-endian helpers. Future protocol tests may need the same logic.  
Scope: Move encode/decode integer helpers into a small internal utility if duplication appears.  
Non-goals: Do not change wire format.  
Implementation notes: Keep helpers simple and covered by packet tests.  
Acceptance criteria: Packet tests still pass and protocol output remains compatible.  
Validation commands: `./scripts/test.sh`  
Related files: `src/network/PacketCodec.cpp`

## 10. Add repository format compatibility note

Title: Add repository format compatibility note  
Type: Documentation  
Priority: P3  
Background: Repository format is documented but no compatibility policy exists.  
Scope: Document that the current format has no migration tool or compatibility guarantee.  
Non-goals: Do not implement migration.  
Implementation notes: Update backup format and README limitations.  
Acceptance criteria: Documentation clearly states current compatibility scope.  
Validation commands: Markdown link check  
Related files: `docs/backup-format.md`, `README.md`

## 11. Add Docker runtime smoke test script

Title: Add Docker runtime smoke test script  
Type: Docker  
Priority: P3  
Background: Docker scripts validate build/test/demo. Runtime image help checks are manual.  
Scope: Add a small script that runs runtime image CLI help checks.  
Non-goals: Do not add image signing or SBOM in this issue.  
Implementation notes: Use existing `cpp-data-protection-core:runtime` tag.  
Acceptance criteria: Script exits zero when runtime image can execute all shipped CLIs.  
Validation commands: `./scripts/docker_runtime_smoke.sh`  
Related files: `Dockerfile`, `scripts/docker_build.sh`, `docs/docker.md`

## 12. Add Docker image hardening review

Title: Add Docker image hardening review  
Type: Docker  
Priority: P3  
Background: Runtime image is functional but not hardened.  
Scope: Review base image, user privileges, writable directories, package list, and labels.  
Non-goals: Do not convert to distroless unless evaluated separately.  
Implementation notes: Document tradeoffs in Docker docs.  
Acceptance criteria: Runtime image behavior remains unchanged and hardening choices are documented.  
Validation commands: `./scripts/docker_test.sh && ./scripts/docker_demo.sh`  
Related files: `Dockerfile`, `docs/docker.md`

## 13. Add CI artifact upload for benchmark logs

Title: Add CI artifact upload for benchmark logs  
Type: CI/CD  
Priority: P3  
Background: CI currently validates build/test/Docker but does not retain benchmark output.  
Scope: Add optional artifact upload for a small benchmark run.  
Non-goals: Do not fail CI on benchmark thresholds.  
Implementation notes: Keep benchmark size small to avoid slow CI.  
Acceptance criteria: Workflow artifact includes benchmark output.  
Validation commands: GitHub Actions run  
Related files: `.github/workflows/ci.yml`, `scripts/bench.sh`

## 14. Add release smoke test after packaging

Title: Add release smoke test after packaging  
Type: Release  
Priority: P2  
Background: Release workflow packages binaries after tests, but does not extract tarball and run shipped binaries.  
Scope: Extract tar.gz in workflow and run `bin/backupctl --help`, `bin/backup-client --help`, `bin/backup-server --help`.  
Non-goals: Do not change artifact names.  
Implementation notes: Add a step before `softprops/action-gh-release`.  
Acceptance criteria: Release fails if packaged binaries cannot execute.  
Validation commands: GitHub Actions tag run  
Related files: `.github/workflows/release.yml`, `docs/release.md`

## 15. Expand troubleshooting documentation

Title: Expand troubleshooting documentation  
Type: Documentation  
Priority: P3  
Background: README and Docker docs include basic troubleshooting, but failure modes could be easier to locate.  
Scope: Add a troubleshooting document for dependency, port, Docker daemon, sanitizer, and corrupted repository errors.  
Non-goals: Do not duplicate every README command.  
Implementation notes: Link from README.  
Acceptance criteria: Common failures have concrete commands and expected output.  
Validation commands: Markdown link check  
Related files: `README.md`, `docs/docker.md`, `docs/cicd.md`

## 16. Add long-running client/server stress test

Title: Add long-running client/server stress test  
Type: Robustness  
Priority: P4  
Background: Current client/server demo validates interrupted upload, but not repeated sessions or concurrent clients.  
Scope: Add opt-in stress script for repeated uploads and interrupted sessions.  
Non-goals: Do not run long stress tests in default CI.  
Implementation notes: Make runtime and dataset size configurable.  
Acceptance criteria: Script can run locally and fail on data mismatch or server error.  
Validation commands: `DPC_STRESS_SECONDS=60 ./scripts/stress_client_server.sh`  
Related files: `src/network/*`, `scripts/demo_client_server.sh`

## 17. Evaluate optional object encryption

Title: Evaluate optional object encryption  
Type: Future Feature  
Priority: P4  
Background: Object encryption is not implemented.  
Scope: Draft a design for optional authenticated encryption at rest.  
Non-goals: Do not implement encryption in the design issue.  
Implementation notes: Cover key management assumptions and metadata impact.  
Acceptance criteria: Design document lists format changes, migration impact, and test plan.  
Validation commands: Documentation review  
Related files: `docs/backup-format.md`, `src/core/ObjectStore.cpp`

## 18. Prepare technical walkthrough notes

Title: Prepare technical walkthrough notes  
Type: Interview Preparation  
Priority: P4  
Background: `docs/interview-qa.md` lists core discussion points. More focused walkthroughs can help explain implementation tradeoffs.  
Scope: Add a concise walkthrough for backup flow, recovery flow, and TCP upload flow.  
Non-goals: Do not present planned work as completed.  
Implementation notes: Link to diagrams and source files.  
Acceptance criteria: Notes reference only existing code paths and documented limitations.  
Validation commands: Markdown link check  
Related files: `docs/interview-qa.md`, `docs/diagrams.md`, `README.md`
