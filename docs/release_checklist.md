# v1.0.0 Release Checklist

## Hardware Gates

- [x] F407/F103 formal firmware built and flashed from the release commit.
- [x] Dual-node read-only smoke passes with 20/20 requests per node.
- [x] Bus-Off recovery is demonstrated in three independent rounds.
- [x] Four-hour stability run completes continuously with no surprise reset.
- [x] Final `RCC_CSR`, health snapshots, `uwTick`, and UDS counters are archived.
- [x] Normal-load 10,000-frame run shows physical frame loss `< 0.1%`.
- [x] The 90% supplemental 2000-frame run is recorded separately.
- [x] Formal firmware is restored after all isolated test images.

## Software Gates

- [x] Full pytest matrix passes locally.
- [x] F407 and F103 formal builds pass without warnings introduced by this work.
- [ ] GitHub Actions Python 3.10-3.14 and firmware jobs pass.
- [x] Wheel build succeeds.
- [x] CLI entry point starts successfully.
- [x] `pyproject.toml`, `diagnoser.__version__`, changelog, and tag all agree.

## Documentation Gates

- [x] Chinese README and English README cross-link correctly.
- [x] Architecture, wiring, stability, final-test, and test reports are current.
- [x] Release notes summarize code, verified results, limitations, and upgrade
  notes.
- [x] `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, and community files exist.
- [x] Raw logs and local hardware details remain outside Git.

## Publishing Gates

- [x] `git diff --check` passes.
- [ ] Scheduled commit contains only this task's code, tests, and docs.
- [ ] No COM ports, probe serials, personal paths, credentials, or raw local logs.
- [ ] Tag `v1.0.0` points to the verified commit.
- [ ] Commit and tag are pushed to `origin`.
- [ ] GitHub Release publishes source, firmware artifacts, wheel, test report,
  and technical documentation.

Any unchecked hardware gate keeps the corresponding Roadmap phase as `Target`.
