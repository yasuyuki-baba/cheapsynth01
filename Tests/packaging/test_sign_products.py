import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('sign_products', Path(__file__).parents[2] / 'scripts/sign_products.py')
signing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(signing)


class SigningPreparationTest(unittest.TestCase):
    def test_windows_signs_inner_vst3_binary_and_standalone_and_clap(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path).resolve()
            binary = root / 'Synth.vst3/Contents/x86_64-win/Synth.vst3'
            binary.parent.mkdir(parents=True)
            for product in [binary, root / 'Synth.exe', root / 'Synth.clap']:
                product.write_bytes(b'product')
            commands = signing.signing_commands(root, 'Windows', 'a' * 40)
            self.assertEqual(sum(c[1] == 'sign' for c in commands), 3)
            self.assertEqual(sum(c[1] == 'verify' for c in commands), 3)
            self.assertTrue(any(c[-1] == str(binary) for c in commands))

    def test_mac_signs_nested_macho_before_enclosing_bundle(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path).resolve()
            product = root / 'Synth.app'
            binary = product / 'Contents/MacOS/Synth'
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b'\xcf\xfa\xed\xfecode')
            commands = signing.signing_commands(root, 'macOS', 'Developer ID Application: Example')
            self.assertEqual(commands[0][-1], str(binary))
            self.assertEqual(commands[1][-1], str(product))
            self.assertNotIn('--deep', commands[0])
            self.assertIn('--strict', commands[-1])

    def test_missing_products_and_invalid_identity_fail(self):
        with tempfile.TemporaryDirectory() as path:
            for platform, identity in [('Windows', 'a' * 40), ('Windows', 'password'), ('macOS', 'Example'), ('macOS', '-bad')]:
                with self.subTest(platform=platform, identity=identity), self.assertRaises(ValueError):
                    signing.signing_commands(Path(path), platform, identity)

    def test_escaping_product_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path).resolve()
            outside = root / 'outside'; outside.mkdir()
            product = root / 'out'; product.mkdir()
            (product / 'Synth.app').symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                signing.signing_commands(product, 'macOS', 'Example')
