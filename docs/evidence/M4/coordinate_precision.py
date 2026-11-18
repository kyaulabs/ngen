"""B3 entry model, not engine code. Compare binary64 with an exact rational reference."""

from dataclasses import dataclass
from fractions import Fraction
import math
import sys


WIDTH = 2**32
HALF = WIDTH // 2
MIN_CELL = -(2**63)
MAX_CELL = 2**63 - 1
LOCAL_LIMIT = 2**20  # Probe envelope, not a World admission limit.


def normalize(cell, offset):
    if not MIN_CELL <= cell <= MAX_CELL or not math.isfinite(offset):
        raise ValueError("invalid cell or offset")
    carry = math.trunc(math.ldexp(offset, -32))
    if not MIN_CELL <= carry <= MAX_CELL:
        raise ValueError("carry out of range")
    remainder = offset - math.ldexp(float(carry), 32)
    if remainder >= HALF:
        carry += 1
        remainder -= WIDTH
    elif remainder < -HALF:
        carry -= 1
        remainder += WIDTH
    if not MIN_CELL <= carry <= MAX_CELL or not MIN_CELL <= cell + carry <= MAX_CELL:
        raise ValueError("normalized cell out of range")
    return cell + carry, remainder or 0.0


@dataclass(frozen=True)
class Frame:
    cell: int
    offset: float
    velocity: float
    stamp: tuple[int, int, int]  # frame ID, epoch, simulation tick


def transfer(source, target, stamp, position, velocity):
    if stamp != source.stamp or source.stamp[2] != target.stamp[2]:
        raise ValueError("frame ID, epoch or tick mismatch")
    if not all(MIN_CELL <= f.cell <= MAX_CELL and -HALF <= f.offset < HALF
               and math.isfinite(f.velocity) for f in (source, target)):
        raise ValueError("noncanonical frame")
    if not math.isfinite(position) or not math.isfinite(velocity) or abs(position) > LOCAL_LIMIT:
        raise ValueError("invalid local sample")
    # C++ must check adjacency before subtracting the two signed cells.
    cells = source.cell - target.cell
    if abs(cells) > 1:
        raise ValueError("not a neighboring frame")
    delta = math.ldexp(float(cells), 32) + (source.offset - target.offset)
    local = delta + position
    speed = (source.velocity - target.velocity) + velocity
    if max(abs(delta), abs(local)) > LOCAL_LIMIT or not math.isfinite(speed):
        raise ValueError("outside the probe envelope")
    return local, speed, target.stamp


def exact_position(frame):
    return frame.cell * WIDTH + Fraction(frame.offset)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def rejects(operation, *args):
    try:
        operation(*args)
    except ValueError:
        return
    raise RuntimeError(f"expected rejection from {operation.__name__}")


def main():
    require(sys.float_info.radix == 2 and sys.float_info.mant_dig == 53, "requires binary64")
    inside = math.nextafter(float(HALF), -math.inf)
    outside = math.nextafter(-float(HALF), -math.inf)
    for offset in (inside, float(HALF), outside, -float(HALF), 2.0 * WIDTH + 0.125,
                   -3.0 * WIDTH - 0.125, math.ulp(0.0), -math.ulp(0.0), -0.0):
        cell, local = normalize(17, offset)
        require(-HALF <= local < HALF, "offset was not canonical")
        require(cell * WIDTH + Fraction(local) == 17 * WIDTH + Fraction(offset),
                "normalization changed the exact position")
    require(normalize(17, HALF) == (18, -HALF), "upper tie did not carry")
    require(normalize(17, -HALF) == (17, -HALF), "lower tie unexpectedly carried")
    require(math.copysign(1, normalize(17, -0.0)[1]) == 1, "negative zero retained")
    for cell, offset in ((MAX_CELL, HALF), (MIN_CELL, outside), (0, 2.0**95),
                         (0, math.inf), (0, math.nan)):
        rejects(normalize, cell, offset)

    for name, metres in (("zero", 0), ("1 AU", 149_597_870_700), ("1 ly", 9_460_730_472_580_800),
                         ("galactic", 220_000_000_000 * WIDTH),
                         ("negative galactic", -220_000_000_000 * WIDTH)):
        cell, offset = divmod(metres, WIDTH)
        first = Frame(*normalize(cell, float(offset)), 94.1, (1, 7, 555))
        second = Frame(*normalize(cell, float(offset) + 0.3125), 94.3, (2, 8, 555))
        result = transfer(first, second, first.stamp, 0.125, 0.9)
        reference = exact_position(first) - exact_position(second) + Fraction(0.125)
        split_error = abs(Fraction(result[0]) - reference)
        flat = (float(exact_position(first)) - float(exact_position(second))) + 0.125
        flat_error = abs(Fraction(flat) - reference)
        require(split_error <= Fraction("0.001"), f"split position residual: {name}")
        if name in ("1 ly", "galactic", "negative galactic"):
            require(flat_error > Fraction("0.001"), f"flat negative control not sensitive: {name}")
        print(f"{name}: split={float(split_error):.9g} m; flat={float(flat_error):.9g} m")

    # 10,000 arithmetic round trips at cross-cell boundaries, not live physics rebases.
    pos_error = vel_error = drift = Fraction(0)
    first = Frame(MAX_CELL - 1, math.nextafter(float(HALF), -math.inf), -317.61, (5, 7, 900))
    second = Frame(MAX_CELL, -HALF + 314.125, 411.29, (6, 1, 900))
    expected_shift = exact_position(first) - exact_position(second)
    expected_speed = Fraction(first.velocity) - Fraction(second.velocity)
    local, velocity, stamp = 17.035, 119.729, first.stamp
    local_initial, velocity_initial = Fraction(local), Fraction(velocity)
    for _ in range(10_000):
        p, v, updated = transfer(first, second, stamp, local, velocity)
        pos_error = max(pos_error, abs(Fraction(p) - (local_initial + expected_shift)))
        vel_error = max(vel_error, abs(Fraction(v) - (velocity_initial + expected_speed)))
        local, velocity, stamp = transfer(second, first, updated, p, v)
        drift = max(drift, abs(Fraction(local) - local_initial))
        vel_error = max(vel_error, abs(Fraction(velocity) - velocity_initial))
        require(stamp == first.stamp, "round trip did not return the frame stamp")
    require(max(pos_error, drift) <= Fraction("0.001") and vel_error <= Fraction("0.0001"),
            "round-trip residual exceeded B3")
    print(f"10,000 split round trips: max position {float(pos_error):.9g} m; "
          f"return drift {float(drift):.9g} m; max velocity {float(vel_error):.9g} m/s")

    rejects(transfer, first, second, (5, 6, 900), 0.0, 0.0)
    rejects(transfer, first, second, (6, 7, 900), 0.0, 0.0)
    rejects(transfer, first, Frame(MAX_CELL, -HALF, 0.0, (6, 1, 901)), first.stamp, 0.0, 0.0)
    rejects(transfer, first, Frame(MIN_CELL, 0.0, 0.0, second.stamp), first.stamp, 0.0, 0.0)
    rejects(transfer, first, Frame(MAX_CELL, HALF, 0.0, second.stamp), first.stamp, 0.0, 0.0)
    rejects(transfer, first, second, first.stamp, math.nan, 0.0)
    rejects(transfer, first, second, first.stamp, -LOCAL_LIMIT, 0.0)
    rejects(transfer, first, second, first.stamp, 0.0, math.inf)
    print("normalization, signed bounds, invalid state, frame IDs, epochs and ticks: passed")


if __name__ == "__main__":
    main()
