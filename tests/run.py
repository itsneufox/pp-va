#!/usr/bin/env python3
"""Run pp-va regressions in a temporary localhost-only open.mp server."""

import argparse
import json
import os
import re
from pathlib import Path
import socket
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-root", type=Path, required=True, help="open.mp installation containing omp-server, qawno, components and plugins")
    parser.add_argument("--components-dir", type=Path, help="override the default server-root/components directory")
    parser.add_argument("--include-dir", type=Path, action="append", default=[], help="additional compiler include directory (repeatable)")
    parser.add_argument("--spread", action="store_true", help="test default argument spreading (requires amx_assembly)")
    parser.add_argument("--optimization", choices=("0", "1"), default="0", help="compiler optimization level")
    args = parser.parse_args()
    repo = args.server_root.resolve()
    library = Path(__file__).resolve().parents[1]
    components_dir = args.components_dir.resolve() if args.components_dir else repo / "components"
    components = sorted(components_dir.glob("*.so"))
    if not components:
        parser.error(f"No component .so files found in {components_dir}; use --components-dir for a custom layout")
    with tempfile.TemporaryDirectory(prefix="pp-va-tests-") as temporary:
        runtime = Path(temporary)
        for directory in ("components", "plugins", "gamemodes", "scriptfiles"):
            (runtime / directory).mkdir()
        for component in components:
            (runtime / "components" / component.name).symlink_to(component)
        for plugin in ("crashdetect", "PawnPlus"):
            (runtime / "plugins" / f"{plugin}.so").symlink_to(repo / "plugins" / f"{plugin}.so")

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as port_probe:
            port_probe.bind(("127.0.0.1", 0))
            port = port_probe.getsockname()[1]
        config = {
            "name": "pp-va isolated tests",
            "announce": False,
            "max_players": 1,
            "network": {"bind": "127.0.0.1", "port": port},
            "pawn": {
                "legacy_plugins": ["crashdetect", "PawnPlus"],
                "main_scripts": ["pp_va_runtime"],
                "side_scripts": [],
            },
            "rcon": {"enable": False, "password": "isolated-test-only"},
        }
        (runtime / "config.json").write_text(json.dumps(config, indent=2))
        environment = os.environ.copy()
        environment["LD_LIBRARY_PATH"] = str(repo / "qawno")
        build = subprocess.run(
            [
                str(repo / "qawno/pawncc"),
                str(library / ("tests/spread.pwn" if args.spread else "tests/runtime.pwn")),
                f"-i{library / 'includes'}",
                *(f"-i{directory.resolve()}" for directory in args.include_dir),
                "-Dgamemodes", "-;+", "-(+", "-d3", "-Z+", f"-O{args.optimization}",
                f"-o{runtime / 'gamemodes/pp_va_runtime.amx'}",
            ],
            cwd=repo, env=environment, capture_output=True, text=True, timeout=60,
        )
        print(build.stdout + build.stderr, end="")
        if build.returncode or "warning " in build.stdout + build.stderr:
            raise SystemExit("Fixture compilation failed or produced warnings")
        try:
            run = subprocess.run(
                [str(repo / "omp-server")], cwd=runtime,
                capture_output=True, text=True, timeout=30,
            )
        except subprocess.TimeoutExpired as error:
            print(error.stdout or "")
            print(error.stderr or "")
            raise SystemExit("Test server did not finish within 30 seconds") from error
        output = run.stdout + run.stderr
        runtime_errors = re.findall(r'Run time error (\d+):', output)
        expected_errors = ["10"] * 257 if args.spread else []
        if (
            run.returncode
            or ("PP_VA_SPREAD_RESULT failures=0" if args.spread else "PP_VA_TEST_RESULT failures=0") not in output
            or (not args.spread and "PP_VA_PRINTF_CHECK Alice 42 1.25%" not in output)
            or (not args.spread and "PP_VA_PRINTF_LITERAL %d %%" not in output)
            or "FAIL:" in output
            or runtime_errors != expected_errors
            or (args.spread and "PP_VA_SPREAD_PRINTF works 42" not in output)
            or "[Error]" in output
        ):
            print(output)
            raise SystemExit("Runtime regression tests failed")
        print("PASS: " + ("direct spread, mixed arguments, nesting, recursion, arrays, references, and repeated stack cleanup" if args.spread else "variadic forwarding, nested frames, bounded output, empty arguments, and dynamic formatting"))


if __name__ == "__main__":
    main()
