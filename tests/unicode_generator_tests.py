import hashlib
import importlib.machinery
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY = Path(sys.argv[1]).resolve()
SCRIPT = REPOSITORY / "tools" / "update-unicode-data"


def load_generator():
    loader = importlib.machinery.SourceFileLoader("unicode_generator", str(SCRIPT))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


GENERATOR = load_generator()


class UnicodeGeneratorTests(unittest.TestCase):
    def test_duplicate_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "emoji-test.txt"
            path.write_text(
                "# group: Smileys & Emotion\n"
                "# subgroup: face-smiling\n"
                "1F600 ; fully-qualified # 😀 E1.0 grinning face\n"
                "1F600 ; fully-qualified # 😀 E1.0 grinning face\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "duplicate"):
                GENERATOR.parse_emoji_test(path)

    def test_empty_input_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "emoji-test.txt"
            path.write_text("# Version: 17.0\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "no records"):
                GENERATOR.parse_emoji_test(path)

    def test_deterministic_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = root / "sources"
            sources.mkdir()
            files = {
                "emoji-test.txt": (
                    "# group: Smileys & Emotion\n"
                    "# subgroup: face-smiling\n"
                    "1F600 ; fully-qualified # 😀 E1.0 grinning face\n"
                    "# group: People & Body\n"
                    "# subgroup: hand-fingers-closed\n"
                    "1F44D ; fully-qualified # 👍 E0.6 thumbs up\n"
                    "1F44D 1F3FF ; fully-qualified # 👍🏿 E1.0 thumbs up: dark skin tone\n"
                ),
                "emoji-sequences.txt": "",
                "emoji-zwj-sequences.txt": "",
                "emoji-data.txt": "",
            }
            annotations = {
                "en": ("grinning face", "face | grin"),
                "de": ("grinsendes Gesicht", "Gesicht | grinsen"),
                "ru": ("широко улыбается", "лицо | улыбка"),
            }
            for locale, (name, keywords) in annotations.items():
                files[f"{locale}.xml"] = (
                    "<ldml><annotations>"
                    f'<annotation cp="😀">{keywords}</annotation>'
                    f'<annotation cp="😀" type="tts">{name}</annotation>'
                    "</annotations></ldml>"
                )
                files[f"{locale}-derived.xml"] = "<ldml><annotations/></ldml>"
            manifest_sources = []
            for name, content in files.items():
                path = sources / name
                path.write_text(content, encoding="utf-8")
                manifest_sources.append(
                    {
                        "name": name,
                        "url": f"https://invalid.example/{name}",
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    }
                )
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "emoji_version": "17.0",
                        "unicode_version": "17.0.0",
                        "cldr_version": "48.2",
                        "sources": manifest_sources,
                    }
                ),
                encoding="utf-8",
            )
            first = root / "first.inc"
            second = root / "second.inc"
            command = [
                sys.executable,
                str(SCRIPT),
                "--manifest",
                str(manifest),
                "--source-dir",
                str(sources),
            ]
            subprocess.run(command + ["--output", str(first)], check=True)
            subprocess.run(command + ["--output", str(second)], check=True)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            generated = first.read_text(encoding="utf-8")
            self.assertIn("kGeneratedEmojiCount = 3", generated)
            self.assertIn('"👍🏿"', generated)
            self.assertIn('"👍"', generated)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
