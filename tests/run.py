#!/usr/bin/env python3
"""Run pp-va regressions in a temporary localhost-only open.mp server."""

import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-root", type=Path, required=True, help="open.mp installation containing omp-server, qawno, components and plugins")
    args = parser.parse_args()
    repo = args.server_root.resolve()
    library = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="pp-va-tests-") as temporary:
        runtime = Path(temporary)
        for directory in ("components", "plugins", "gamemodes", "scriptfiles"):
            (runtime / directory).mkdir()
        for component in (repo / "components/LINUX/default").glob("*.so"):
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
                str(library / "tests/runtime.pwn"),
                f"-i{library / 'includes'}",
                "-Dgamemodes", "-;+", "-(+", "-d3", "-Z+",
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
        if (
            run.returncode
            or "PP_VA_TEST_RESULT failures=0" not in output
            or "PP_VA_PRINTF_CHECK Alice 42 1.25%" not in output
            or "PP_VA_PRINTF_LITERAL %d %%" not in output
            or "FAIL:" in output
            or "Run time error" in output
            or "[Error]" in output
        ):
            print(output)
            raise SystemExit("Runtime regression tests failed")
        print("PASS: variadic forwarding, nested frames, bounded output, empty arguments, and dynamic formatting")


if __name__ == "__main__":
    main()
