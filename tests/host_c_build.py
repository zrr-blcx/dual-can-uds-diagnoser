"""Compile and run portable firmware sources as host-side C tests."""

from __future__ import annotations

import locale
import os
import shutil
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = PROJECT_ROOT / ".host_test"


def run(
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

    dev_command = run(
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

    compiler = run(
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

    return vsdevcmd, Path(compiler.stdout.splitlines()[0].strip())


def _visual_studio_environment(vsdevcmd: Path) -> dict[str, str] | None:
    result = run(
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


def build_host_binary(name: str, sources: Sequence[Path], test_source: Path) -> Path:
    """Compile the given firmware sources plus a test main into an executable.

    Skips the calling test when no C toolchain is available, and fails with the
    compiler output when the build itself is broken.
    """

    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    include = PROJECT_ROOT / "firmware" / "common"
    executable = BUILD_ROOT / name

    if os.name == "nt":
        paths = _visual_studio_paths()
        if paths is None:
            pytest.skip("Visual Studio developer environment is unavailable")
        vsdevcmd, compiler = paths
        executable = executable.with_suffix(".exe")
        environment = _visual_studio_environment(vsdevcmd)
        if environment is None:
            pytest.skip("Visual Studio environment setup failed")
        compile_result = run(
            [
                str(compiler),
                "/nologo",
                "/std:c11",
                "/W4",
                "/WX",
                f"/I{include}",
                *(str(source) for source in sources),
                str(test_source),
                f"/Fe:{executable}",
            ],
            cwd=BUILD_ROOT,
            env=environment,
        )
    else:
        compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
        if compiler is None:
            pytest.skip("a C compiler is unavailable")
        compile_result = run(
            [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{include}",
                *(str(source) for source in sources),
                str(test_source),
                "-o",
                str(executable),
            ],
            cwd=PROJECT_ROOT,
        )

    if compile_result.returncode != 0:
        pytest.fail(
            f"{name} compilation failed:\n"
            f"{compile_result.stdout}\n{compile_result.stderr}"
        )

    return executable


def run_host_binary(executable: Path) -> subprocess.CompletedProcess[str]:
    return run([str(executable)], cwd=PROJECT_ROOT)
