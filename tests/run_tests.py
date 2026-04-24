#!/usr/bin/env python3
"""Run test executables in parallel and collect results."""

import argparse
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# --- Configuration ---
# Default build directory for quick execution
DEFAULT_BUILD_DIR = r"C:\Users\corey\Dev\build_output\JsonTreeViewer" 

# The executables and their corresponding log files.
# If relative, they are relative to the 'tests' directory in the build output.
TESTS_CONFIG = [
    {"exe": "test_strategies.exe", "log": "test_strategies.log"},
    {"exe": "test_paging.exe", "log": "test_paging.log"},
]
# --- End Configuration ---


def run_test(exe_path, log_path, args=None, cwd=None):
    """Run a single test executable and capture output."""
    cmd = [str(exe_path)]
    if args:
        cmd.extend(args)

    print(f"Starting {exe_path.name} -> {log_path.name}")

    try:
        result = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=cwd
        )

        log_path.write_text(result.stdout, encoding="utf-8")
        return exe_path.name, result.returncode, log_path
    except Exception as e:
        error_msg = f"Error running {exe_path.name}: {e}"
        print(error_msg)
        log_path.write_text(error_msg, encoding="utf-8")
        return exe_path.name, 1, log_path


def main():
    parser = argparse.ArgumentParser(description="Run tests in parallel")
    parser.add_argument(
        "--build-dir",
        default=DEFAULT_BUILD_DIR,
        help=f"Build directory (default: {DEFAULT_BUILD_DIR})",
    )
    parser.add_argument("--filter", default="", help="Test filter")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    tests_dir = build_dir / "tests"

    if not tests_dir.exists():
        print(f"Error: Tests directory not found: {tests_dir}")
        print("Please check --build-dir or DEFAULT_BUILD_DIR in the script.")
        return 1

    run_list = []
    for config in TESTS_CONFIG:
        exe_name = config["exe"]
        log_name = config["log"]

        exe_path = tests_dir / exe_name
        log_path = tests_dir / log_name

        run_list.append((exe_path, log_path))

    if not run_list:
        print("Nothing to run. Check TESTS_CONFIG.")
        return 0

    print(f"Running {len(run_list)} tests in parallel...\n")

    # Run tests in parallel
    with ThreadPoolExecutor(max_workers=len(run_list)) as executor:
        test_args = [args.filter] if args.filter else None
        futures = [
            executor.submit(run_test, exe, log, test_args, cwd=build_dir)
            for exe, log in run_list
        ]
        results = [f.result() for f in futures]

    # Print summary
    print("\n" + "=" * 60)
    print(f"{'Log File':<25} {'Executable':<20} {'Result'}")
    print("-" * 60)

    failed = 0
    for name, code, log_file in results:
        if log_file.exists():
            content = log_file.read_text(encoding="utf-8")
            totals = [
                line for line in content.splitlines() if line.startswith("Totals:")
            ]
            if totals:
                line = totals[-1]
                color = "\033[92m" if ", 0 failed" in line else "\033[91m"
                print(f"{color}{log_file.name:<25} {name:<20} {line}\033[0m")
            else:
                status = "PASSED" if code == 0 else "FAILED"
                color = "\033[92m" if code == 0 else "\033[91m"
                print(f"{color}{log_file.name:<25} {name:<20} Status: {status}\033[0m")
        else:
            print(f"\033[91m{log_file.name:<25} {name:<20} Log file missing!\033[0m")

        if code != 0:
            failed += 1

    print("=" * 60)
    print(f"Total: {len(results)}, Failed: {failed}")

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
