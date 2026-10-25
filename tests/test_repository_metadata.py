"""Validate public package metadata and GitHub workflow configuration."""

from __future__ import annotations

from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # Python 3.10
    import tomli as tomllib

import yaml

import diagnoser

PROJECT_ROOT = Path(__file__).resolve().parents[1]


def load_pyproject() -> dict:
    with (PROJECT_ROOT / "pyproject.toml").open("rb") as stream:
        return tomllib.load(stream)


def test_package_metadata_is_complete_and_versioned() -> None:
    project = load_pyproject()["project"]

    assert project["version"] == diagnoser.__version__ == "0.6.0"
    assert project["readme"] == "README.md"
    assert project["license"] == "MIT"
    assert project["authors"]
    assert project["urls"]["Repository"].startswith("https://github.com/")
    assert project["scripts"]["dual-can-uds"] == "diagnoser.cli:main"


def test_github_yaml_files_parse() -> None:
    yaml_files = sorted((PROJECT_ROOT / ".github").rglob("*.yml"))

    assert yaml_files
    for path in yaml_files:
        with path.open(encoding="utf-8") as stream:
            assert yaml.safe_load(stream) is not None, path


def test_ci_covers_python_matrix_and_firmware_builds() -> None:
    with (PROJECT_ROOT / ".github" / "workflows" / "ci.yml").open(
        encoding="utf-8"
    ) as stream:
        workflow = yaml.safe_load(stream)

    assert workflow["permissions"]["contents"] == "read"
    assert workflow["concurrency"]["cancel-in-progress"] is True
    assert set(workflow["jobs"]) == {"test", "firmware"}
    assert workflow["jobs"]["test"]["strategy"]["matrix"]["python-version"] == [
        "3.10",
        "3.11",
        "3.12",
        "3.13",
        "3.14",
    ]


def test_github_community_files_exist() -> None:
    required = [
        PROJECT_ROOT / "CONTRIBUTING.md",
        PROJECT_ROOT / "SECURITY.md",
        PROJECT_ROOT / "CODE_OF_CONDUCT.md",
        PROJECT_ROOT / ".github" / "pull_request_template.md",
        PROJECT_ROOT / ".github" / "ISSUE_TEMPLATE" / "bug_report.yml",
        PROJECT_ROOT / ".github" / "ISSUE_TEMPLATE" / "feature_request.yml",
    ]

    assert all(path.is_file() for path in required)


def test_firmware_build_scripts_use_shared_discovery() -> None:
    for target in ("ecu-f407", "ecu-f103"):
        script = (
            PROJECT_ROOT / "firmware" / target / "build.ps1"
        ).read_text(encoding="utf-8")

        assert "firmware/common/toolchain.ps1" in script
        assert "Resolve-ArmGccToolchain" in script
        assert "E:\\ST" not in script
