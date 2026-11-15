#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Pre-build formatting runner (C/C++ first, optional GLSL).

Install to <Project>/Tools/format_before_build.py alongside indent_regions.py.
Run before *every* build via the actual build system (not just VS Code Run On Save).

  python Tools/format_before_build.py
  python Tools/format_before_build.py --source Source --source Include
  python Tools/format_before_build.py --include-shaders
  python Tools/format_before_build.py --check

Only project-owned directories are scanned by default; Docs, Tools, Build,
ThirdParty, generated sources and the vault itself are not scanned.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys

CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
GLSL_SUFFIXES = {".glsl", ".comp", ".vert", ".frag", ".geom", ".tesc", ".tese"}
DEFAULT_SOURCES = ("Source", "Include", "Src")
DEFAULT_SHADER_DIRS = ("Shaders",)
SKIP_NAMES = {".git", ".vscode", ".obsidian", "Build", "build", "CMakeFiles", "ThirdParty", "Vendor", "Generated", "__pycache__"}


def collect(root: Path, directories: list[str], extensions: set[str]) -> list[Path]:
    found: set[Path] = set()
    for directory in directories:
        candidate = (root / directory).resolve()
        if not candidate.is_relative_to(root):
            raise ValueError(f"Directory outside project root: {directory}")
        if not candidate.is_dir():
            continue
        for file in candidate.rglob("*"):
            if (file.is_file() and not file.is_symlink() and file.suffix.lower() in extensions
                    and not any(part in SKIP_NAMES for part in file.relative_to(root).parts)):
                found.add(file)
    return sorted(found)


def run_command(args: list[str], root: Path) -> None:
    subprocess.run(args, cwd=root, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--source", action="append", dest="sources", default=None,
                        help="Source directory relative to project root; repeat as needed")
    parser.add_argument("--shader-dir", action="append", dest="shader_dirs", default=None)
    parser.add_argument("--include-shaders", action="store_true", help="Also format GLSL region indentation")
    parser.add_argument("--check", action="store_true", help="Fail if code would change (no writes)")
    parser.add_argument("--clang-format", default="clang-format", help="Clang-format executable or absolute path")
    opts = parser.parse_args()
    root = opts.root.resolve()
    formatter = root / "Tools" / "indent_regions.py"
    config = root / ".clang-format"

    if not root.is_dir() or not config.is_file() or not formatter.is_file():
        print("Missing project root, .clang-format or Tools/indent_regions.py", file=sys.stderr)
        return 2

    clang = shutil.which(opts.clang_format)
    if not clang:
        print(f"clang-format not found: {opts.clang_format}", file=sys.stderr)
        return 2

    try:
        sources = collect(root, opts.sources or list(DEFAULT_SOURCES), CPP_SUFFIXES)
        if not sources:
            print("No C/C++ files found: update --source paths for this project", file=sys.stderr)
            return 2

        # Per-file execution avoids command-length limits and preserves fail-fast behavior.
        for path in sources:
            if opts.check:
                # Check clang-format's current output without writing a temporary file.
                original = path.read_bytes()
                formatted = subprocess.run([clang, "--style=file", str(path)], cwd=root,
                                           capture_output=True, check=True).stdout
                if formatted != original:
                    print(f"clang-format differs: {path.relative_to(root)}", file=sys.stderr)
                    return 1
                run_command([sys.executable, str(formatter), "--check", str(path)], root)
            else:
                run_command([clang, "-i", "--style=file", str(path)], root)
                run_command([sys.executable, str(formatter), str(path)], root)

        shaders: list[Path] = []
        if opts.include_shaders:
            shaders = collect(root, opts.shader_dirs or list(DEFAULT_SHADER_DIRS), GLSL_SUFFIXES)
            for path in shaders:
                cmd = [sys.executable, str(formatter)]
                if opts.check:
                    cmd.append("--check")
                run_command(cmd + [str(path)], root)
        print(f"Pre-build formatting OK: C/C++ {len(sources)} files, GLSL {len(shaders)} files")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"Pre-build formatting failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
