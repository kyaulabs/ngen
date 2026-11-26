"""Behavior of the B4 evidence checker; no window or hardware required."""
import unittest

from tools import b4_report


def example():
    out = [f"b4_mem,{s * 1000},250000000,28,0,230000000,237000000,1"
           for s in range(3601)]
    out.extend(f"b4_sale,{n},{(n - 1) // 4 + 1},{(8, 12, 18, 22)[(n - 1) % 4]},"
               f"{n * 15000},{1000 + n},{1000000 + n}"
               for n in range(1, 241))
    out.extend(["b4_present,216060,60", "b4_frame,200,216000",
                "b4_frame_end,216000,20000000,52000000,1000,3599984",
                "b4_end,3600000,60,240,120,120,240,3601"])
    return out


class Verification(unittest.TestCase):
    def test_separately_quantifies_admitted_market_and_persistence(self):
        report = b4_report.summarize(example())
        self.assertEqual(report["sales"], 240)
        self.assertEqual(report["market_ns"]["p95"], 1228)
        self.assertEqual(report["persistence_ns"]["p95"], 1000228)
        self.assertEqual(report["rss_bytes"]["settled_gain"], 0)
        self.assertEqual(report["flight_frame_ns"]["p95_upper"], 20_000_000)

    def test_rejects_a_short_run_or_disconnected_memory_sampling(self):
        rows = example()
        with self.assertRaisesRegex(ValueError, "one-hour run"):
            b4_report.summarize(rows[:-1] + [rows[-1].replace("3600000", "90000")])
        with self.assertRaisesRegex(ValueError, "memory sampling"):
            b4_report.summarize(rows[:550] + rows[551:])

    def test_rejects_an_unstable_settled_resource_plateau(self):
        rows = example()
        for second in range(3300, 3600):
            rows[second] = rows[second].replace(",28,0,", ",29,0,")
        with self.assertRaisesRegex(ValueError, "growth"):
            b4_report.summarize(rows)

    def test_requires_flight_measurements(self):
        with self.assertRaisesRegex(ValueError, "flight timing"):
            b4_report.summarize([row for row in example() if not row.startswith("b4_frame")])

    def test_rejects_a_single_over_budget_queue_gap_even_with_stable_percentiles(self):
        rows = example()
        rows[-2] = "b4_frame_end,216000,20000000,200000001,1000,3599984"
        with self.assertRaisesRegex(ValueError, "flight frame or queue budget"):
            b4_report.summarize(rows)

    def test_counts_only_post_warmup_queues_and_accepts_exact_flight_limits(self):
        rows = example()[:-3]
        rows.extend(["b4_frame,500,108000", "b4_frame,667,97200", "b4_frame,1000,10800",
                     "b4_frame_end,216000,100000000,200000000,1000,3599984",
                     "b4_end,3600000,60,240,120,120,240,3601"])
        self.assertEqual(b4_report.summarize(rows)["flight_frame_ns"]["p95_upper"], 66_700_000)
        rows[-4] = "b4_frame,667,97199"
        with self.assertRaisesRegex(ValueError, "timing histogram"):
            b4_report.summarize(rows)

    def test_distinguishes_a_slow_market_from_slow_disk(self):
        rows = example()
        for offset in range(36):
            sale = 3601 + offset
            fields = rows[sale].rsplit(",", 2)
            rows[sale] = f"{fields[0]},{fields[1]},24000000"
        self.assertEqual(b4_report.summarize(rows)["persistence_ns"]["p95"], 24000000)
        for offset in range(36):
            sale = 3601 + offset
            fields = rows[sale].rsplit(",", 2)
            rows[sale] = f"{fields[0]},21000000,{fields[2]}"
        with self.assertRaisesRegex(ValueError, "market p95"):
            b4_report.summarize(rows)


if __name__ == "__main__":
    unittest.main()
