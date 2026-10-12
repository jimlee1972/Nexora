"""Keep generated HLSL within MSVC's per-literal limit without changing source."""
import importlib.util
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'pbr_generator', ROOT / 'Engine/Presentation/shaders/GeneratePbrShaders.py')
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)
LITERALS = re.compile(r'R"NEXORA_PBR\((.*?)\)NEXORA_PBR"', re.S)


class EmbeddedHlslTests(unittest.TestCase):
    def test_long_source_preserves_bytes_and_bounds_literals(self):
        for source in ('float4 main() { return 1; }\n' * 1400,
                       '// source comment: \U0001f680\n' * 1000):
            with self.subTest(unicode=not source.isascii()):
                chunks = LITERALS.findall(GENERATOR.embed_hlsl_source('fixture', source))
                self.assertGreater(len(chunks), 1)
                self.assertEqual(''.join(chunks).encode('utf-8'), ('\n' + source).encode('utf-8'))
                self.assertTrue(all(len(chunk.encode('utf-8')) <= 8192 for chunk in chunks))

    def test_delimiter_is_rejected(self):
        with self.assertRaises(ValueError):
            GENERATOR.embed_hlsl_source('fixture', ')NEXORA_PBR"')

    def test_checked_in_hlsl_is_bounded_and_reproducible(self):
        header = (ROOT / 'Engine/Presentation/src/ScenePbrHlslShaders.h').read_text()
        declarations = re.findall(
            r'inline constexpr char (\w+)\[\] =\s*((?:R"NEXORA_PBR\(.*?\)NEXORA_PBR"\s*)+);\n',
            header, re.S)
        self.assertEqual(len(declarations), 3)
        for name, declaration in declarations:
            with self.subTest(entry=name):
                chunks = LITERALS.findall(declaration)
                self.assertTrue(all(len(chunk.encode('utf-8')) <= 8192 for chunk in chunks))
                source = ''.join(chunks)
                self.assertTrue(source.startswith('\n'))
                self.assertIn(GENERATOR.embed_hlsl_source(name, source[1:]), header)


if __name__ == '__main__':
    unittest.main()
