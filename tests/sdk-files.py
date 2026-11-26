"""Protect compiler inputs and verify the retained IR and linker option formats."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import cpu6_object
import centos_text
from cpu6_gc import separate


class SDKFiles(unittest.TestCase):
    def test_text_across_sector_boundary(self):
        source = 'A' * 390 + '\n' + 'B' * 80 + '\n'
        self.assertEqual(centos_text.decode(centos_text.encode(source)), source)

    def test_output_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work / 'main.c'
            text = 'int main(void) { return 0; }\n'
            source.write_text(text)
            alias = work / 'alias.map'
            os.link(source, alias)
            output = work / 'app.bin'
            driver = ROOT / 'run/cpu6-bare-sdcc'
            for mapping in (source, alias, output, work / 'app.bin.build.json'):
                with self.subTest(mapping=mapping):
                    run = subprocess.run([driver, source, '-o', output, '--map', mapping],
                                         capture_output=True, text=True)
                    self.assertNotEqual(run.returncode, 0)
                    self.assertIn('overwrite', run.stderr)
                    self.assertEqual(source.read_text(), text)
                    self.assertFalse(output.exists())

    def test_object_ir_round_trip_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / 'main.o'
            subprocess.run([ROOT / 'run/cpu6-bare-sdcc', '-c', ROOT / 'tests/smoke.c',
                            '-o', obj], check=True)
            previous, _ = cpu6_object.unpack(obj.read_bytes())
            obj.write_bytes(previous)
            with patch.object(cpu6_object, 'MAX_IR', 4096):
                with self.assertRaisesRegex(ValueError, 'too large'):
                    cpu6_object.attach(obj, 'a' * 4096)
            self.assertEqual(obj.read_bytes(), previous)

    def test_combined_linker_options(self):
        options, files, symbols = separate(['-bvC512', '-uentry', '-o', 'a.bin'])
        self.assertEqual(options, ['-b', '-v', '-C', '512', '-u', 'entry', '-o', 'a.bin'])
        self.assertEqual(files, [])
        self.assertEqual(symbols, ['entry'])

    def test_rebuild_resident_program(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work / 'main.c'
            command = [ROOT / 'run/cpu6-mmu-cc', source, '-o', work / 'X.exf']
            source.write_text('volatile unsigned x attribute((mmu_page=auto))=12;\n'
                              'int main(void){return x;}\n')
            subprocess.run(command, check=True)
            companion = work / 'X.mmu.exf'
            self.assertTrue(companion.is_file())
            source.write_text('int main(void){return 1;}\n')
            subprocess.run(command, check=True)
            self.assertFalse(companion.exists())
            self.assertFalse((work / 'X.mmu.exf.build.json').exists())


if __name__ == '__main__':
    unittest.main()
