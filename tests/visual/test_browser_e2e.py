"""Full browser E2E: Chromium UI backed by real compiled C binaries.

Requires Playwright+Chromium and OSLLM_CLI/OSLLM_RACE/OSLLM_DEADLOCK.
The CI job must fail if the dependency or a compiled binary is missing.
"""
import json
import os
from pathlib import Path
import sys
from threading import Thread
import unittest

from playwright.sync_api import sync_playwright, expect

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "apps" / "visual"))
import server


class BrowserE2E(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        names = ("OSLLM_CLI", "OSLLM_RACE", "OSLLM_DEADLOCK")
        values = [os.environ.get(name) for name in names]
        if any(not value for value in values):
            raise AssertionError("Real compiled CLI, race and deadlock binaries are required")
        server.Handler.cli, server.Handler.race_lab, server.Handler.deadlock_lab = (
            Path(path).resolve() for path in values)
        for path in (server.Handler.cli, server.Handler.race_lab, server.Handler.deadlock_lab):
            if not path.is_file():
                raise AssertionError("Missing compiled binary: " + str(path))
        cls.httpd = server.ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
        cls.addClassCleanup(cls.httpd.server_close)
        cls.thread = Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()
        cls.addClassCleanup(lambda: cls.thread.join(timeout=5))
        cls.addClassCleanup(cls.httpd.shutdown)
        cls.url = "http://127.0.0.1:" + str(cls.httpd.server_port)
        cls.playwright = sync_playwright().start()
        cls.addClassCleanup(cls.playwright.stop)
        cls.browser = cls.playwright.chromium.launch(headless=True, args=["--no-sandbox"])
        cls.addClassCleanup(cls.browser.close)

    def setUp(self):
        self.page = self.browser.new_page(accept_downloads=True,
                                          viewport={"width": 1440, "height": 900})
        self.addCleanup(self.page.close)
        self.errors = []
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))
        self.page.goto(self.url, wait_until="networkidle")
        expect(self.page.locator("#run-btn")).to_be_visible()

    def tearDown(self):
        self.assertEqual(self.errors, [], "Uncaught browser JavaScript exceptions")

    def execute(self):
        self.page.locator("#run-btn").click()
        expect(self.page.locator("#run-state")).to_have_text("CONCLUÍDO", timeout=25000)
        expect(self.page.locator("#results")).to_be_visible()

    def test_scheduler_gantt_replay_comparison_and_export(self):
        self.execute()
        expect(self.page.locator("#result-rows tr")).to_have_count(3)
        expect(self.page.locator("#timeline-panel")).to_be_visible()
        self.assertGreater(self.page.locator("#timeline-chart .timeline-bar").count(), 0)
        self.page.locator("#timeline-start").click()
        self.assertTrue(self.page.locator("#timeline-position").inner_text().startswith("0 /"))
        self.page.locator("#timeline-step").click()
        self.assertTrue(self.page.locator("#timeline-position").inner_text().startswith("1 /"))
        self.page.locator("#policy").select_option("rr")
        expect(self.page.locator("#quantum")).to_be_enabled()
        self.execute()
        expect(self.page.locator("#report-runs tr")).to_have_count(2)
        with self.page.expect_download() as event:
            self.page.locator("#report-export").click()
        self.assertEqual(event.value.suggested_filename, "osllm-run.json")

    def test_paging_lru_real_lab_and_events(self):
        self.page.locator("#mem-on").check()
        self.page.locator("#mem-frames").fill("2")
        self.page.locator("#mem-tokens").fill("2")
        self.page.locator("#mem-latency").fill("1")
        self.page.locator("#mem-policy").select_option("lru")
        self.execute()
        expect(self.page.locator("#mem-result")).to_be_visible()
        self.assertGreater(int(self.page.locator("#mem-faults").inner_text()), 0)
        self.assertGreater(self.page.locator("#mem-log li").count(), 0)
        self.page.locator('[data-lab="race"][data-mode="safe"]').click()
        expect(self.page.locator("#lab-status")).to_contain_text("mode=safe", timeout=12000)

    def test_csv_input_bounds_and_no_silent_uint64_rounding(self):
        self.page.locator("#file").set_input_files({
            "name": "scenario.csv", "mimeType": "text/csv",
            "buffer": b"1,0,4,5,1\n2,1,3,4,2\n"})
        expect(self.page.locator("#entry-count")).to_have_text("2 / 256 requisições")
        row = self.page.locator("#requests tr").first
        row.locator("input").first.fill("0")
        self.page.locator("#run-btn").click()
        expect(self.page.locator("#notice")).to_contain_text("fora do intervalo")
        expect(self.page.locator("#results")).to_be_hidden()
        row.locator("input").first.fill("1")
        row.locator("input").nth(1).fill("18446744073709551614")
        self.page.locator("#run-btn").click()
        expect(self.page.locator("#notice")).to_contain_text("precisão de ticks")
        expect(self.page.locator("#run-state")).to_have_text("AGUARDANDO")

    def test_h24_report_import_is_labeled_synthetic(self):
        fixture = {
            "schema": "os-llm-report/v1",
            "provenance": {"source_claim": "synthetic_fixture"},
            "scheduling": [{"name": "fcfs", "average_wait_ticks": 7,
                            "throughput_per_tick": 0.1}], "memory": []
        }
        self.page.locator("#report-file").set_input_files({
            "name": "h24.json", "mimeType": "application/json",
            "buffer": json.dumps(fixture).encode("utf-8")})
        expect(self.page.locator("#report-h24")).to_be_visible()
        expect(self.page.locator("#report-h24-source")).to_contain_text("synthetic_fixture")
        expect(self.page.locator("#report-h24-bars .report-bar-row")).to_have_count(1)


if __name__ == "__main__":
    unittest.main()
