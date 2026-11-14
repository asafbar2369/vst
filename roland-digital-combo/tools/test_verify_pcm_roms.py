import io
from pathlib import Path
import tempfile
import unittest
import zipfile
import zlib

from verify_pcm_roms import Chip, audit, classify

class TestVerifier(unittest.TestCase):
    def test_matching_crc_and_size(self):
        data = b'original-user-chip-copy-for-test'
        chip = Chip('D-50', 'PCM combined A+B', len(data), f'{zlib.crc32(data):08x}')
        self.assertEqual(classify(data, (chip,))['role'], 'PCM combined A+B')
        self.assertFalse(classify(data + b'X', (chip,))['verified'])
        self.assertFalse(classify(data[:-1] + b'X', (chip,))['verified'])

    def test_zip_unknown_does_not_falsely_claim_success(self):
        with tempfile.TemporaryDirectory() as root:
            f = Path(root) / 'roms.zip'
            with zipfile.ZipFile(f, 'w') as z:
                z.writestr('d50_fake.bin', b'NOT AN ORIGINAL ROM')
            r = audit([f])
            self.assertEqual(len(r['files']), 1)
            self.assertFalse(r['files'][0]['verified'])
            self.assertFalse(r['D-50']['pcm_dump_complete'])
            self.assertFalse(r['JD-800']['pcm_dump_complete'])

    def test_missing_file(self):
        self.assertIn('error', audit([Path('/path/which/does/not/exist.bin')])['files'][0])

if __name__ == '__main__':
    unittest.main()
