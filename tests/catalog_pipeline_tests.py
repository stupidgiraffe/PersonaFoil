import importlib.util
import pathlib
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("catalog", pathlib.Path(__file__).resolve().parents[1] / "tools/build_catalog.py")
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.sources = [{"url": "https://feed.example/index.json", "title": "Feed", "name": "Curated feed", "discover_directories": True}]

    def fetch(self, url):
        if url.endswith("index.json"):
            return {"health": "online", "http_result": 200, "redirect_target": ""}, {"directories": ["https://shop.example/", "https://SHOP.example:443/", "https://user:fixture@invalid.example/", "https://shop.example/?token=fixture"]}
        return {"health": "authentication_required", "http_result": 401, "redirect_target": ""}, None

    def test_discovery_dedupe_credentials_and_provenance(self):
        result = catalog.build(self.sources, fetch=self.fetch)
        self.assertEqual(len(result["entries"]), 2)
        self.assertTrue(all(entry["compatibility"] == "auto" for entry in result["entries"]))
        self.assertTrue(any(entry["authentication"] == "required" for entry in result["entries"]))
        self.assertEqual(catalog.validate_previous(result), result)
        self.assertNotIn("fixture", str(result))

    def test_failed_feed_preserves_last_good(self):
        previous = catalog.build(self.sources, fetch=self.fetch)
        def offline(_url):
            return {"health": "offline", "http_result": 0, "redirect_target": ""}, None
        updated = catalog.build(self.sources, previous, offline)
        self.assertEqual({value["id"] for value in previous["entries"]}, {value["id"] for value in updated["entries"]})

    def test_transaction_and_corrupt_previous(self):
        result = catalog.build(self.sources, fetch=self.fetch)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "catalog.json"
            catalog.write_atomic(output, result)
            self.assertTrue(output.exists())
            self.assertFalse(output.with_suffix(".json.new").exists())
        result["entries"][0]["password"] = "fixture"
        with self.assertRaises(ValueError):
            catalog.validate_previous(result)


if __name__ == "__main__":
    unittest.main()
