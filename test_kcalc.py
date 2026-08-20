#!/usr/bin/python3
"""pytest suite for kcalc's /dev/kcalc character device.

Loads kcalc.ko once for the whole session (unlike the old expr=
module-param version, which needed a fresh insmod per case), then
drives each test case by writing to and reading from /dev/kcalc.

Writes/reads go through `tee`/`cat` via subprocess rather than shell
redirection (`>`, `>>`) — the expression is passed as subprocess
stdin, so shell metacharacters in expressions (parens, ^, a literal
newline test case) are never interpreted by a shell at all.

Must run as root (or with passwordless sudo for insmod/rmmod/tee/cat)
on the target machine.
"""

import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

import pytest

MODULE_DIR = Path(__file__).parent
MODULE_NAME = "kcalc"
MODULE_FILE = MODULE_DIR / f"{MODULE_NAME}.ko"
DEVICE_PATH = "/dev/kcalc_chardev"


@dataclass
class KcalcCase:
    expr: str
    should_succeed: bool
    expected_result: int | None = None  # checked only when should_succeed
    expected_error_substr: str | None = None  # checked only when not should_succeed


CASES = [
    KcalcCase(expr="42", should_succeed=True, expected_result=42),
    KcalcCase(expr="12 + 300 - 5", should_succeed=True, expected_result=307),
    KcalcCase(expr="1 2 3", should_succeed=False),
    KcalcCase(expr="((1 + 2) * 3)", should_succeed=True, expected_result=9),
    KcalcCase(expr="100 % 7 ^ 2 / 3", should_succeed=True, expected_result=0),
    KcalcCase(expr="-5 + 3", should_succeed=True, expected_result=-2),
    KcalcCase(expr="12 + +3", should_succeed=True, expected_result=15),
    KcalcCase(
        expr="__2", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(
        expr="\n", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(
        expr="", should_succeed=False
    ),  # write() itself returns 0 for empty input, no error to check
    KcalcCase(expr="2^3^2", should_succeed=True, expected_result=512),
    KcalcCase(expr="(2^3)^2", should_succeed=True, expected_result=64),
    KcalcCase(expr="-2^2", should_succeed=True, expected_result=-4),
    KcalcCase(  # negative exponent
        expr="2^-2", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(expr="-(2+3)-3^2+(-3)^3", should_succeed=True, expected_result=-41),
    KcalcCase(expr="----------41", should_succeed=True, expected_result=41),
    KcalcCase(
        expr="invalid_input",
        should_succeed=False,
        expected_error_substr="Invalid argument",
    ),
    KcalcCase(
        expr="(((((((((((2+2*(3333))))))))))))",
        should_succeed=True,
        expected_result=6668,
    ),
    KcalcCase(
        expr="(1 + 2", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(
        expr="1 + 2)", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(
        expr="((1 + 2)", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(expr="- - 5", should_succeed=True, expected_result=5),
    KcalcCase(expr="1 + - - 2", should_succeed=True, expected_result=3),
    KcalcCase(expr="3 + -4 * 2", should_succeed=True, expected_result=-5),
    KcalcCase(
        expr="1 * / 2", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(expr="+", should_succeed=False, expected_error_substr="Invalid argument"),
    KcalcCase(
        expr="10 / 0", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(
        expr="((((((((((((((((((((((((((((((((((((xd))))))))))))))))))))))))))))))))))))",
        should_succeed=False,
        expected_error_substr="Invalid argument",
    ),
    KcalcCase(expr="123*(2+3)", should_succeed=True, expected_result=615),
    KcalcCase(  # unmatched parens
        expr="123*(2+3))(",
        should_succeed=False,
        expected_error_substr="Invalid argument",
    ),
    KcalcCase(  # overflow
        expr="2^67",
        should_succeed=False,
        expected_error_substr="Value too large for defined data type",
    ),
    KcalcCase(
        expr="aaa", should_succeed=False, expected_error_substr="Invalid argument"
    ),
    KcalcCase(expr="100+333-222/3-0^2", should_succeed=True, expected_result=359),
]


@pytest.fixture(scope="session", autouse=True)
def kcalc_module():
    """Build once, insmod once for the whole session, rmmod at the end."""
    subprocess.run(["make", "-C", str(MODULE_DIR)], check=True, capture_output=True)

    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)
    subprocess.run(
        ["sudo", "insmod", str(MODULE_FILE)], check=True, capture_output=True
    )

    # device_create() is synchronous, but leave a small margin in case
    # your setup runs udev rules that add latency before the node
    # actually appears.
    for _ in range(50):
        if Path(DEVICE_PATH).exists():
            break
        time.sleep(0.05)
    else:
        subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)
        pytest.exit(f"{DEVICE_PATH} never appeared after insmod")

    yield

    subprocess.run(["sudo", "rmmod", MODULE_NAME], capture_output=True)


def write_expr(expr: str) -> subprocess.CompletedProcess:
    """Write `expr` to the device via tee, stdin-based (no shell parsing)."""
    return subprocess.run(
        ["sudo", "tee", DEVICE_PATH],
        input=expr,
        text=True,
        capture_output=True,
    )


def read_result() -> subprocess.CompletedProcess:
    return subprocess.run(
        ["sudo", "cat", DEVICE_PATH],
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize(
    "case",
    CASES,
    ids=[repr(c.expr) for c in CASES],
)
def test_kcalc(case: KcalcCase):
    write = write_expr(case.expr)

    if not case.should_succeed:
        if case.expected_error_substr:
            assert write.returncode != 0, (
                f"expected write to fail for {case.expr!r}, stdout={write.stdout!r}"
            )
            assert case.expected_error_substr in write.stderr, (
                f"expected {case.expected_error_substr!r} in stderr, "
                f"got {write.stderr!r}"
            )
        return

    assert write.returncode == 0, write.stderr

    read = read_result()
    assert read.returncode == 0, read.stderr

    if case.expected_result is not None:
        assert read.stdout.strip() == str(case.expected_result), (
            f"expr={case.expr!r}: expected {case.expected_result}, got {read.stdout!r}"
        )
