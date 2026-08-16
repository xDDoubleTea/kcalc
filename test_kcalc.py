#!/usr/bin/python3
"""pytest suite for kcalc.ko, driven by the expr= module param.

Must run as root (or with passwordless sudo for insmod/rmmod) on the
target machine — this loads and unloads a real kernel module, so
tests cannot run in parallel and cannot run in a container without
kernel module privileges.

Cases below are adapted from shuntingyard's userspace test.py (same
tokenizer/shunting-yard/eval logic), translated from
"exit code + stdout" checks into "insmod success/failure + dmesg
Result= line" checks, since kcalc reports through pr_info rather than
stdout. insmod itself always exits 1 on any init failure regardless of
the underlying errno, so failure cases additionally check insmod's
stderr message where the errno is distinguishable (e.g. -EINVAL vs
-EOVERFLOW map to different strerror() text).
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
class KcalcCase:
    expr: str
    should_succeed: bool
    expected_result: int | None = None  # checked only when should_succeed
    expected_error_substr: str | None = None  # checked only when not should_succeed


CASES = [
    # --- ported from shuntingyard's test.py (same algorithm, expr-based) ---
    KcalcCase(expr="42", should_succeed=True, expected_result=42),
    KcalcCase(expr="12 + 300 - 5", should_succeed=True, expected_result=307),
    KcalcCase(expr="1 2 3", should_succeed=False),
    KcalcCase(expr="((1 + 2) * 3)", should_succeed=True, expected_result=9),
    KcalcCase(expr="100 % 7 ^ 2 / 3", should_succeed=True, expected_result=0),
    KcalcCase(expr="-5 + 3", should_succeed=True, expected_result=-2),
    KcalcCase(expr="12 + +3", should_succeed=True, expected_result=15),
    KcalcCase(
        expr="__2", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="\n", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(expr="2^3^2", should_succeed=True, expected_result=512),
    KcalcCase(expr="(2^3)^2", should_succeed=True, expected_result=64),
    KcalcCase(expr="-2^2", should_succeed=True, expected_result=-4),
    KcalcCase(  # negative exponent
        expr="2^-2", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(expr="-(2+3)-3^2+(-3)^3", should_succeed=True, expected_result=-41),
    KcalcCase(expr="----------41", should_succeed=True, expected_result=41),
    KcalcCase(
        expr="invalid_input",
        should_succeed=False,
        expected_error_substr="Invalid parameters",
    ),
    KcalcCase(
        expr="(((((((((((2+2*(3333))))))))))))",
        should_succeed=True,
        expected_result=6668,
    ),
    KcalcCase(
        expr="(1 + 2", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="1 + 2)", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="((1 + 2)",
        should_succeed=False,
        expected_error_substr="Invalid parameters",
    ),
    KcalcCase(expr="- - 5", should_succeed=True, expected_result=5),
    KcalcCase(expr="1 + - - 2", should_succeed=True, expected_result=3),
    KcalcCase(expr="3 + -4 * 2", should_succeed=True, expected_result=-5),
    KcalcCase(
        expr="1 * / 2", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="+", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="10 / 0", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(
        expr="((((((((((((((((((((((((((((((((((((xd))))))))))))))))))))))))))))))))))))",
        should_succeed=False,
        expected_error_substr="Invalid parameters",
    ),
    # --- new cases from manual testing this session ---
    KcalcCase(expr="123*(2+3)", should_succeed=True, expected_result=615),
    KcalcCase(  # unmatched parens
        expr="123*(2+3))(",
        should_succeed=False,
        expected_error_substr="Invalid parameters",
    ),
    KcalcCase(  # overflow
        expr="2^67",
        should_succeed=False,
        expected_error_substr="Value too large for defined data type",
    ),
    KcalcCase(
        expr="aaa", should_succeed=False, expected_error_substr="Invalid parameters"
    ),
    KcalcCase(expr="100+333-222/3-0^2", should_succeed=True, expected_result=359),
]


@pytest.fixture(scope="session", autouse=True)
def build_module():
    """Build kcalc.ko once for the whole session."""
    subprocess.run(
        ["make"],
        check=True,
        capture_output=True,
    )
    # yield
    # subprocess.run(["make", "clean"], capture_output=True)


@pytest.fixture(autouse=True)
def ensure_unloaded():
    """Guarantee a clean slate before and after every test."""
    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)
    yield
    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)


def dmesg_snapshot() -> list[str]:
    """Full current dmesg content, one line per list entry."""
    result = subprocess.run(
        ["sudo", "dmesg"], capture_output=True, text=True, check=True
    )
    return result.stdout.splitlines()


def dmesg_new_lines(baseline: list[str]) -> list[str]:
    """Lines appended to dmesg since `baseline` was captured.

    Non-destructive (no `dmesg -C`), so it's safe to run without extra
    root steps and doesn't erase kernel log history from outside this
    test run. Assumes the ring buffer didn't rotate `baseline` itself
    out between the two reads — true in practice for the sub-second
    gap around one insmod call, but would silently under-report if
    something flooded dmesg concurrently.
    """
    after = dmesg_snapshot()
    return after[len(baseline) :]


@pytest.mark.parametrize(
    "case",
    CASES,
    ids=[repr(c.expr) for c in CASES],
)
def test_kcalc(case: KcalcCase):
    baseline = dmesg_snapshot()

    insmod = subprocess.run(
        ["sudo", "insmod", str(MODULE_FILE), f"expr={case.expr}"],
        capture_output=True,
        text=True,
    )

    if not case.should_succeed:
        assert insmod.returncode != 0, (
            f"expected insmod to fail for {case.expr!r}, stdout={insmod.stdout!r}"
        )
        if case.expected_error_substr:
            assert case.expected_error_substr in insmod.stderr, (
                f"expected {case.expected_error_substr!r} in stderr, "
                f"got {insmod.stderr!r}"
            )
        return

    subprocess.run(["sudo", "rmmod", MODULE_NAME], check=True)

    assert insmod.returncode == 0, insmod.stderr

    new_lines = dmesg_new_lines(baseline)
    result_lines = [line for line in new_lines if "Result = " in line]
    assert result_lines, (
        f"no 'Result = ' line found in dmesg for {case.expr!r}; "
        f"new lines were: {new_lines!r}"
    )
    assert len(result_lines) == 1, (
        f"expected exactly one 'Result = ' line for {case.expr!r}, "
        f"got {len(result_lines)}: {result_lines!r}"
    )

    if case.expected_result is not None:
        match = re.search(r"Result = (-?\d+)", result_lines[0])
        assert match, f"could not parse result from: {result_lines[0]}"
        assert int(match.group(1)) == case.expected_result
