#!/usr/bin/env python3
"""CTest gate for the pinned upstream AMFlow package version."""

from __future__ import annotations

import argparse
import copy
import json
import os
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REGISTRY_PATH = Path(
    "tools/reference-harness/specs/release/amflow-upstream-version-pin.registry.json"
)
PROBE_PREFIX = "AMFLOW_PIN_PROBE_"


class VerificationError(RuntimeError):
    """Raised when the AMFlow pin registry or live probe is inconsistent."""


@dataclass(frozen=True)
class PinConfig:
    version: str
    release_date: str
    package_file: Path
    wolframscript: Path
    package_info_symbol: str
    version_expression: str
    release_date_expression: str


@dataclass(frozen=True)
class ProbeResult:
    version: str
    release_date: str
    package_info: str
    stdout: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def require_mapping(value: Any, label: str) -> dict[str, Any]:
    expect(isinstance(value, dict), f"{label} must be an object")
    return value


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str), f"{label} must be a string")
    expect(value.strip() == value, f"{label} must not contain surrounding whitespace")
    expect(value != "", f"{label} must not be empty")
    return value


def load_json(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise VerificationError(f"{path} is not valid JSON: {exc}") from exc
    return require_mapping(payload, str(path))


def resolve_registry_path(root: Path, path: Path | None) -> Path:
    if path is None:
        return root / REGISTRY_PATH
    if path.is_absolute():
        return path
    return root / path


def resolve_file(root: Path, value: str, label: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = root / path
    expect(path.is_file(), f"{label} does not exist or is not a file: {path}")
    return path


def validate_registry(
    root: Path,
    payload: dict[str, Any],
    *,
    wolframscript_override: Path | None = None,
    package_file_override: Path | None = None,
    require_files: bool = True,
) -> PinConfig:
    expect(payload.get("schema_version") == 1, "schema_version must be 1")
    expect(payload.get("package") == "AMFlow", "package must be AMFlow")

    expected = require_mapping(payload.get("expected"), "expected")
    source = require_mapping(payload.get("source"), "source")
    wolfram = require_mapping(payload.get("wolfram"), "wolfram")
    probe = require_mapping(payload.get("probe"), "probe")

    version = require_string(expected.get("version"), "expected.version")
    release_date = require_string(expected.get("release_date"), "expected.release_date")
    require_string(source.get("url"), "source.url")
    require_string(source.get("requested_ref"), "source.requested_ref")
    require_string(source.get("resolved_commit"), "source.resolved_commit")
    package_file_text = require_string(source.get("package_file"), "source.package_file")

    package_info_symbol = require_string(
        probe.get("package_info_symbol"), "probe.package_info_symbol"
    )
    version_expression = require_string(
        probe.get("version_expression"), "probe.version_expression"
    )
    release_date_expression = require_string(
        probe.get("release_date_expression"), "probe.release_date_expression"
    )
    expect(
        version_expression.startswith(package_info_symbol),
        "probe.version_expression must read from probe.package_info_symbol",
    )
    expect(
        release_date_expression.startswith(package_info_symbol),
        "probe.release_date_expression must read from probe.package_info_symbol",
    )

    package_file = package_file_override or Path(package_file_text)
    if not package_file.is_absolute():
        package_file = root / package_file

    wolframscript_text = require_string(wolfram.get("wolframscript"), "wolfram.wolframscript")
    wolframscript = wolframscript_override or Path(wolframscript_text)
    if not wolframscript.is_absolute():
        wolframscript = root / wolframscript

    if require_files:
        expect(package_file.is_file(), f"source.package_file is missing: {package_file}")
        expect(wolframscript.is_file(), f"wolfram.wolframscript is missing: {wolframscript}")

    return PinConfig(
        version=version,
        release_date=release_date,
        package_file=package_file,
        wolframscript=wolframscript,
        package_info_symbol=package_info_symbol,
        version_expression=version_expression,
        release_date_expression=release_date_expression,
    )


def wolfram_string(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
    )
    return f'"{escaped}"'


def probe_source(config: PinConfig) -> str:
    return "\n".join(
        [
            f"packageFile = {wolfram_string(str(config.package_file))};",
            "Get[packageFile];",
            (
                f'Print["{PROBE_PREFIX}PACKAGE_INFO=" <> '
                f"ToString[InputForm[{config.package_info_symbol}]]];"
            ),
            (
                f'Print["{PROBE_PREFIX}VERSION=" <> '
                f"ToString[{config.version_expression}]];"
            ),
            (
                f'Print["{PROBE_PREFIX}RELEASE_DATE=" <> '
                f"ToString[{config.release_date_expression}]];"
            ),
        ]
    ) + "\n"


def parse_probe_stdout(stdout: str) -> ProbeResult:
    values: dict[str, str] = {}
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if not line.startswith(PROBE_PREFIX) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.removeprefix(PROBE_PREFIX)] = value.strip()

    missing = {"PACKAGE_INFO", "VERSION", "RELEASE_DATE"} - set(values)
    expect(not missing, "AMFlow live probe did not print: " + ", ".join(sorted(missing)))
    return ProbeResult(
        version=values["VERSION"],
        release_date=values["RELEASE_DATE"],
        package_info=values["PACKAGE_INFO"],
        stdout=stdout,
    )


def run_live_probe(config: PinConfig, timeout_seconds: int) -> ProbeResult:
    with tempfile.TemporaryDirectory(prefix="amflow-pin-probe-") as tmp:
        probe_path = Path(tmp) / "probe.wls"
        probe_path.write_text(probe_source(config), encoding="utf-8")
        completed = subprocess.run(
            [str(config.wolframscript), "-file", str(probe_path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
    if completed.returncode != 0:
        stderr_tail = completed.stderr.strip().splitlines()[-20:]
        stdout_tail = completed.stdout.strip().splitlines()[-20:]
        details = "\n".join(stdout_tail + stderr_tail)
        raise VerificationError(
            f"AMFlow live probe failed with exit code {completed.returncode}:\n{details}"
        )
    return parse_probe_stdout(completed.stdout)


def compare_pin_to_probe(config: PinConfig, probe: ProbeResult) -> None:
    expect(
        probe.version == config.version,
        f"AMFlow live version {probe.version!r} does not match pinned version {config.version!r}",
    )
    expect(
        probe.release_date == config.release_date,
        "AMFlow live release date "
        f"{probe.release_date!r} does not match pinned release date {config.release_date!r}",
    )


def verify(
    root: Path,
    registry_path: Path,
    *,
    wolframscript_override: Path | None,
    package_file_override: Path | None,
    timeout_seconds: int,
) -> ProbeResult:
    expect(registry_path.is_file(), f"missing AMFlow pin registry: {registry_path}")
    payload = load_json(registry_path)
    config = validate_registry(
        root,
        payload,
        wolframscript_override=wolframscript_override,
        package_file_override=package_file_override,
    )
    probe = run_live_probe(config, timeout_seconds)
    compare_pin_to_probe(config, probe)
    return probe


def fake_wolframscript(path: Path, *, version: str, release_date: str) -> None:
    script = f"""#!/usr/bin/env python3
import sys
print('{PROBE_PREFIX}PACKAGE_INFO={{{{"{version}", "{release_date}"}}}}')
print('{PROBE_PREFIX}VERSION={version}')
print('{PROBE_PREFIX}RELEASE_DATE={release_date}')
"""
    path.write_text(script, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def self_check_registry(tmp: Path) -> tuple[Path, dict[str, Any]]:
    package_file = tmp / "AMFlow.m"
    package_file.write_text('$PackageInfo = {"1.1", "5-Jun-2022"};\n', encoding="utf-8")
    payload: dict[str, Any] = {
        "schema_version": 1,
        "package": "AMFlow",
        "expected": {
            "version": "1.1",
            "release_date": "5-Jun-2022",
        },
        "source": {
            "url": "https://example.invalid/amflow.git",
            "requested_ref": "1.1",
            "resolved_commit": "775162498ab18493c45254b861669b4151b841ee",
            "package_file": str(package_file),
        },
        "wolfram": {
            "wolframscript": str(tmp / "wolframscript"),
        },
        "probe": {
            "package_info_symbol": "AMFlow`$PackageInfo",
            "version_expression": "AMFlow`$PackageInfo[[1]]",
            "release_date_expression": "AMFlow`$PackageInfo[[2]]",
        },
    }
    registry = tmp / "registry.json"
    registry.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return registry, payload


def expect_self_check_failure(
    root: Path,
    registry: Path,
    expected_error: str,
    *,
    wolframscript_override: Path | None = None,
    package_file_override: Path | None = None,
) -> None:
    try:
        verify(
            root,
            registry,
            wolframscript_override=wolframscript_override,
            package_file_override=package_file_override,
            timeout_seconds=5,
        )
    except VerificationError as exc:
        expect(expected_error in str(exc), f"unexpected self-check error: {exc}")
    else:
        raise VerificationError("self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="amflow-pin-self-check-") as tmp_text:
        tmp = Path(tmp_text)
        registry, payload = self_check_registry(tmp)
        fake = tmp / "wolframscript"
        fake_wolframscript(fake, version="1.1", release_date="5-Jun-2022")
        probe = verify(
            tmp,
            registry,
            wolframscript_override=fake,
            package_file_override=tmp / "AMFlow.m",
            timeout_seconds=5,
        )
        expect(probe.version == "1.1", "self-check did not parse fake AMFlow version")

        drifted = copy.deepcopy(payload)
        drifted["expected"]["version"] = "1.2"
        drifted_registry = tmp / "drifted-registry.json"
        drifted_registry.write_text(json.dumps(drifted, indent=2) + "\n", encoding="utf-8")
        expect_self_check_failure(
            tmp,
            drifted_registry,
            "does not match pinned version",
            wolframscript_override=fake,
            package_file_override=tmp / "AMFlow.m",
        )

        bad_probe = copy.deepcopy(payload)
        bad_probe["probe"]["version_expression"] = "Other`$PackageInfo[[1]]"
        bad_probe_registry = tmp / "bad-probe-registry.json"
        bad_probe_registry.write_text(json.dumps(bad_probe, indent=2) + "\n", encoding="utf-8")
        expect_self_check_failure(
            tmp,
            bad_probe_registry,
            "must read from probe.package_info_symbol",
            wolframscript_override=fake,
            package_file_override=tmp / "AMFlow.m",
        )

    print("AMFlow upstream version pin self-check passed")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--verify", action="store_true", help="verify the live AMFlow version pin")
    mode.add_argument("--self-check", action="store_true", help="run synthetic verifier checks")
    parser.add_argument("--registry", type=Path, help="override the AMFlow pin registry path")
    parser.add_argument("--wolframscript", type=Path, help="override the wolframscript executable")
    parser.add_argument("--amflow-package", type=Path, help="override the AMFlow.m package path")
    parser.add_argument("--timeout-seconds", type=int, default=120)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            run_self_check()
            return 0
        root = repo_root()
        registry = resolve_registry_path(root, args.registry)
        probe = verify(
            root,
            registry,
            wolframscript_override=args.wolframscript,
            package_file_override=args.amflow_package,
            timeout_seconds=args.timeout_seconds,
        )
    except (OSError, subprocess.SubprocessError, VerificationError) as exc:
        print(f"AMFlow upstream version pin verification failed: {exc}", file=sys.stderr)
        return 1

    print(
        "AMFlow upstream version pin verified: "
        f"live version {probe.version}, release date {probe.release_date}, "
        f"package info {probe.package_info}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
