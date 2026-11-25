"""Verify the one-hour windowed B4 trace against the declared fixture budgets."""
import argparse
import json
from pathlib import Path

MIB = 1024 ** 2
HOUR_MS = 3_600_000
MIN_SALES = 100
MARKET_P95_NS = 20_000_000
SALE_REVISIONS = (8, 12, 18, 22)


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def quantile(samples, percent):
    ordered = sorted(samples)
    return ordered[(len(ordered) * percent + 99) // 100 - 1]


def latency(samples):
    return {f"p{p}": quantile(samples, p) for p in (50, 95, 99)} | {"max": max(samples)}


def summarize(lines):
    memory, sales, end = [], [], None
    sizes = {"b4_mem": 7, "b4_sale": 6, "b4_end": 7}
    for line in lines:
        tag, *fields = line.rstrip().split(",")
        if tag == "b4_sale_failed" or line.startswith("Flight demo:"):
            raise ValueError("flight or sale failed")
        if not tag.startswith("b4_"):
            continue
        require(end is None and tag in sizes and len(fields) == sizes[tag],
                "unexpected B4 trace row")
        values = tuple(map(int, fields))
        require(all(value >= 0 for value in values), "negative B4 trace value")
        if tag == "b4_mem":
            memory.append(values)
        elif tag == "b4_sale":
            sales.append(values)
        else:
            end = values
    require(end is not None, "no B4 completion record")
    elapsed, exhausted, sales_count, wins, losses, reopens, samples = end
    require(HOUR_MS <= elapsed <= HOUR_MS + 5000, "not a bounded one-hour run")
    require(sales_count == len(sales) == reopens and sales_count >= MIN_SALES and
            exhausted == sales_count // 4 and wins > 0 and losses > 0 and
            sales_count <= wins + losses <= sales_count + 1, "incomplete charter or replay counts")
    previous = 0
    for i, (ordinal, charter, revision, elapsed_ms, market, persistence) in enumerate(sales):
        require((ordinal, charter, revision) == (i + 1, i // 4 + 1, SALE_REVISIONS[i % 4]),
                "sale order, charter or receipt revision is inconsistent")
        require(0 < elapsed_ms - previous <= 45_000 and 0 < market and 0 < persistence,
                "missing timing or sale progress")
        previous = elapsed_ms
    require(0 <= elapsed - previous <= 45_000, "invalid final sale gap")
    market = latency([row[4] for row in sales])
    require(market["p95"] <= MARKET_P95_NS, "market p95 exceeds 20 ms")
    require(len(memory) == samples and 3565 <= samples <= 3606 and
            memory[0][0] < 1000 and 0 <= elapsed - memory[-1][0] <= 1500,
            "incomplete one-second memory sampling")
    for current, upcoming in zip(memory, memory[1:]):
        require(current[0] // 1000 < upcoming[0] // 1000 and
                upcoming[0] - current[0] <= 5000, "memory sampling gap or unordered clock")
    require(all(0 < rss <= 6 * 1024 * MIB and 0 < live and
                0 < allocated <= reserved <= 1024 * MIB and settled in (0, 1) and
                (not settled or not retiring)
                for _, rss, live, retiring, allocated, reserved, settled in memory),
            "memory or resource budget exceeded")
    first = [row for row in memory if 300_000 <= row[0] < 600_000 and row[-1]]
    last = [row for row in memory if 3_300_000 <= row[0] < HOUR_MS and row[-1]]
    require(min(len(first), len(last)) >= 120, "too few comparable settled starter samples")

    def settled_metrics(column, allowance):
        early = quantile([row[column] for row in first], 95)
        late = quantile([row[column] for row in last], 95)
        require(late - early <= allowance, "settled resource growth exceeds the declared allowance")
        return {"peak": max(row[column] for row in memory), "early_settled_p95": early,
                "late_settled_p95": late, "settled_gain": late - early}

    return {
        "elapsed_ms": elapsed, "exhausted_charters": exhausted, "sales": sales_count,
        "victories": wins, "losses": losses, "reopens": reopens, "memory_samples": samples,
        "early_settled_samples": len(first), "late_settled_samples": len(last),
        "rss_bytes": settled_metrics(1, 32 * MIB), "gpu_live": settled_metrics(2, 0),
        "peak_gpu_retiring": max(row[3] for row in memory),
        "gpu_allocation_bytes": settled_metrics(4, 8 * MIB),
        "gpu_reserved_bytes": settled_metrics(5, 8 * MIB),
        "market_ns": market, "persistence_ns": latency([row[5] for row in sales]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    try:
        with args.trace.open(encoding="utf-8") as trace:
            print(json.dumps(summarize(trace), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f"B4 trace rejected: {error}\n")


if __name__ == "__main__":
    main()
