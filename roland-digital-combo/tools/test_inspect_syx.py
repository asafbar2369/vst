import unittest
from inspect_syx import inspect, messages

def dt1(model: int, linear: int, data: bytes) -> bytes:
    assert 1 <= len(data) <= 256 and all(x < 128 for x in data)
    addr = bytes([(linear >> 14) & 127, (linear >> 7) & 127, linear & 127])
    checksum = (-sum(addr + data)) & 127
    return b'\xf0\x41\x00' + bytes([model, 0x12]) + addr + data + bytes([checksum, 0xf7])

class InventoryTest(unittest.TestCase):
    def test_complete_d50_and_jd_slot(self):
        d = bytearray([0] * 448)
        d[384:388] = [1, 2, 3, 4]  # ABCD
        j = bytearray([32] * 384)
        j[:5] = b'HELLO'
        stream = b'header' + dt1(0x14, 0x8000, d[:256]) + dt1(0x14, 0x8100, d[256:])
        stream += dt1(0x3D, 0x14000, j[:256]) + dt1(0x3D, 0x14100, j[256:])
        result = inspect(stream)
        self.assertEqual(result['stats']['dt1_valid'], 4)
        self.assertEqual(result['D-50']['complete_count'], 1)
        self.assertEqual(result['JD-800']['complete_count'], 1)
        self.assertTrue(result['D-50']['slots'][0]['complete'])
        self.assertTrue(result['JD-800']['slots'][0]['complete'])
        self.assertEqual(result['D-50']['slots'][0]['name'], 'ABCD')
        self.assertEqual(result['JD-800']['slots'][0]['name'], 'HELLO')

    def test_partial_and_bad_checksum(self):
        good = dt1(0x14, 0x8000 + 2 * 448, b'ABC')
        bad = bytearray(dt1(0x14, 0x8000, b'ABC'))
        bad[-2] ^= 1
        out = inspect(bytes(bad) + good)
        self.assertEqual(out['stats']['bad_checksum'], 1)
        self.assertEqual(out['stats']['dt1_valid'], 1)
        self.assertEqual(out['D-50']['complete_count'], 0)
        self.assertEqual(out['D-50']['slots'][0]['covered_bytes'], 3)
        self.assertEqual(out['D-50']['slots'][0]['display_slot'], '1-3')

    def test_unsupported_nonroland_and_truncation(self):
        stream = b'\xf0\x7d\x01\xf7' + b'\xf0\x41\x00\x14\x42\xf7' + dt1(0x14, 0x8000, b'Z')[:-1]
        out = inspect(stream)
        self.assertEqual(out['stats']['not_roland'], 1)
        self.assertEqual(out['stats']['unsupported'], 1)
        self.assertEqual(out['stats']['dt1_valid'], 0)
        self.assertEqual(len(list(messages(b'\xf0\x41\x00'))), 0)

    def test_cross_patch_boundary(self):
        stream = dt1(0x3D, 0x14000 + 383, b'AB')
        out = inspect(stream)
        self.assertEqual(len(out['JD-800']['slots']), 2)
        self.assertEqual([x['covered_bytes'] for x in out['JD-800']['slots']], [1, 1])

if __name__ == '__main__':
    unittest.main()
