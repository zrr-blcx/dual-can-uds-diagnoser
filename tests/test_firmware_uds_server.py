"""Compile and execute the portable firmware UDS server tests."""

from __future__ import annotations

import locale
import os
import shutil
import subprocess
from pathlib import Path
from typing import Mapping

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[1]
INCLUDE = PROJECT_ROOT / "firmware" / "common"
BUILD_ROOT = PROJECT_ROOT / ".host_test"
SOURCES = [
    INCLUDE / "uds_server.c",
    INCLUDE / "uds_reference_routines.c",
    INCLUDE / "uds_did_db.c",
    INCLUDE / "uds_ecu.c",
]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "uds_server_test.c"


def _run(
    command: list[str] | str,
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
    shell: bool = False,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=env,
        shell=shell,
        check=False,
        capture_output=True,
        text=True,
        encoding=locale.getpreferredencoding(False),
        errors="replace",
    )


def _visual_studio_paths() -> tuple[Path, Path] | None:
    vswhere = Path(
        os.environ.get(
            "ProgramFiles(x86)",
            r"C:\Program Files (x86)",
        )
    ) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None

    dev_command = _run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-find",
            r"Common7\Tools\VsDevCmd.bat",
        ],
        cwd=PROJECT_ROOT,
    )
    if dev_command.returncode != 0 or not dev_command.stdout.strip():
        return None

    vsdevcmd = Path(dev_command.stdout.splitlines()[0].strip())
    if not vsdevcmd.is_file():
        return None

    compiler = _run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-find",
            r"VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe",
        ],
        cwd=PROJECT_ROOT,
    )
    if compiler.returncode != 0 or not compiler.stdout.strip():
        return None

    cl = Path(compiler.stdout.splitlines()[0].strip())
    return vsdevcmd, cl


def _visual_studio_environment(vsdevcmd: Path) -> dict[str, str] | None:
    result = _run(
        f'call "{vsdevcmd}" -arch=x64 -host_arch=x64 >nul && set',
        cwd=PROJECT_ROOT,
        shell=True,
    )
    if result.returncode != 0:
        return None

    environment = os.environ.copy()
    for line in result.stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key:
            environment[key] = value
    return environment


@pytest.fixture(scope="module")
def uds_server_test_binary() -> Path:
    build_directory = BUILD_ROOT
    build_directory.mkdir(parents=True, exist_ok=True)
    executable = build_directory / "uds_server_test"

    if os.name == "nt":
        paths = _visual_studio_paths()
        if paths is None:
            pytest.skip("Visual Studio developer environment is unavailable")
        vsdevcmd, compiler = paths
        executable = executable.with_suffix(".exe")
        environment = _visual_studio_environment(vsdevcmd)
        if environment is None:
            pytest.skip("Visual Studio environment setup failed")
        compile_result = _run(
            [
                str(compiler),
                "/nologo",
                "/std:c11",
                "/W4",
                "/WX",
                f"/I{INCLUDE}",
                *(str(source) for source in SOURCES),
                str(TEST_SOURCE),
                f"/Fe:{executable}",
            ],
            cwd=build_directory,
            env=environment,
        )
    else:
        compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
        if compiler is None:
            pytest.skip("a C compiler is unavailable")
        compile_result = _run(
            [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{INCLUDE}",
                *(str(source) for source in SOURCES),
                str(TEST_SOURCE),
                "-o",
                str(executable),
            ],
            cwd=PROJECT_ROOT,
        )

    if compile_result.returncode != 0:
        pytest.fail(
            "firmware UDS test compilation failed:\n"
            f"{compile_result.stdout}\n{compile_result.stderr}"
        )

    return executable


def test_firmware_uds_server_behavior(uds_server_test_binary: Path) -> None:
    result = _run([str(uds_server_test_binary)], cwd=PROJECT_ROOT)
    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "firmware UDS host tests: PASS" in result.stdout
