#!/usr/bin/python3
"""pytest replacement for test.zsh.

Exercises kcalc.ko by insmod/rmmod with module params (a, b, op), then
checks both the insmod exit status and the "Result = ..." line it left
in dmesg. Must run as root (or with passwordless sudo for insmod/rmmod)
on the target machine — this loads and unloads a real kernel module,
so tests cannot run in parallel and cannot run in a container without
kernel module privileges.
"""

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest

MODULE_DIR = Path(__file__).parent
MODULE_NAME = "kcalc"
MODULE_FILE = MODULE_DIR / f"{MODULE_NAME}.ko"


@dataclass
class KcalcCase:  # mirrors ExprCase in shuntingyard's test.py
    a: int
    b: int
    op: str
    should_succeed: bool
    expected_result: int | None = None  # only checked when should_succeed


# Ported 1:1 from test.zsh's insmod lines, plus expected outcomes
# inferred from kcalc.c's calc().
CASES = [
    KcalcCase(9223372036854775807, 1, "+", True),  # overflow: warns, still succeeds
    KcalcCase(300, 1, "-", True, 299),
    KcalcCase(114514, 67, "*", True, 114514 * 67),
    KcalcCase(23010, 3314, "/", True, 23010 // 3314),
    KcalcCase(100, 1, "+abb", False),  # op string len > 1 -> -EINVAL
    KcalcCase(100, 0, "/", False),  # division by zero -> -EINVAL
    KcalcCase(1, 0, "%", False),  # mod by zero -> -EINVAL
    KcalcCase(2, 3, "^", True, 8),
    KcalcCase(2, 67, "^", True),  # overflow: result not checked, just no crash
    KcalcCase(199, 193, "^", True),  # overflow: result not checked, just no crash
    KcalcCase(1, -10, "^", False),  # negative exponent -> -EINVAL
]


@pytest.fixture(scope="session", autouse=True)
def build_module():
    """Build kcalc.ko once for the whole session."""
    subprocess.run(
        ["make", "-C", str(MODULE_DIR)],
        check=True,
        capture_output=True,
    )
    yield
    subprocess.run(["make", "-C", str(MODULE_DIR), "clean"], capture_output=True)


@pytest.fixture(autouse=True)
def ensure_unloaded():
    """Guarantee a clean slate before and after every test."""
    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)
    yield
    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)


def dmesg_tail(lines: int = 20) -> list[str]:
    result = subprocess.run(["dmesg"], capture_output=True, text=True, check=True)
    return result.stdout.strip().splitlines()[-lines:]


@pytest.mark.parametrize(
    "case",
    CASES,
    ids=[f"a={c.a},b={c.b},op={c.op!r}" for c in CASES],
)
def test_kcalc(case: KcalcCase):
    insmod = subprocess.run(
        [
            "sudo",
            "insmod",
            str(MODULE_FILE),
            f"a={case.a}",
            f"b={case.b}",
            f"op={case.op}",
        ],
        capture_output=True,
        text=True,
    )

    if not case.should_succeed:
        assert insmod.returncode != 0, (
            f"expected insmod to fail for {case}, stderr={insmod.stderr!r}"
        )
        return

    assert insmod.returncode == 0, insmod.stderr

    log = dmesg_tail()
    result_lines = [line for line in log if "Result = " in line]
    assert result_lines, "no 'Result = ' line found in dmesg after insmod"

    subprocess.run(["sudo", "rmmod", MODULE_NAME], check=True)

    if case.expected_result is not None:
        match = re.search(r"Result = (-?\d+)", result_lines[-1])
        assert match, f"could not parse result from: {result_lines[-1]}"
        assert int(match.group(1)) == case.expected_result
