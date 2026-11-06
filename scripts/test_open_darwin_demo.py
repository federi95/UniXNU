import struct
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('qemu-entry') / 'make-dt.py'


def nodes(blob, offset=0):
    properties, children = struct.unpack_from('<II', blob, offset)
    offset += 8
    values = {}
    for _ in range(properties):
        name = blob[offset:offset + 32].split(b'\0')[0].decode()
        size, = struct.unpack_from('<I', blob, offset + 32)
        values[name] = blob[offset + 36:offset + 36 + size]
        offset += 36 + ((size + 3) & ~3)
    rows = [values]
    for _ in range(children):
        child, offset = nodes(blob, offset)
        rows.extend(child)
    return rows, offset


class OpenDarwinADTTests(unittest.TestCase):
    def generate(self, *flags):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'fixture.adt'
            result = subprocess.run([sys.executable, str(SCRIPT), '--output', str(output), *flags], capture_output=True)
            return result, output.read_bytes() if output.exists() else None

    def test_pl011_is_explicit_and_uses_the_virt_bus(self):
        result, blob = self.generate('--armvirt', '--armcpu', '--no-ufs-test-nodes', '--pl011-console')
        self.assertEqual(result.returncode, 0, result.stderr)
        rows, end = nodes(blob)
        self.assertEqual(end, len(blob))
        uart, = [row for row in rows if row['name'] == b'uart0\0']
        bus, = [row for row in rows if row['name'] == b'arm-io\0']
        offset, size = struct.unpack('<QQ', uart['reg'])
        _, base, span = struct.unpack('<QQQ', bus['ranges'])
        self.assertEqual((base + offset, size), (0x09000000, 0x1000))
        self.assertLessEqual(offset + size, span)
        self.assertEqual(uart['compatible'], b'arm,pl011\0')
        self.assertFalse(any(row['name'] in (b'ufshc\0', b'ufsphy\0') for row in rows))

    def test_default_virtual_fixture_remains_without_uart(self):
        result, blob = self.generate('--armvirt', '--no-ufs-test-nodes')
        self.assertEqual(result.returncode, 0)
        self.assertFalse(any(row['name'] == b'uart0\0' for row in nodes(blob)[0]))

    def test_uart_without_virtual_platform_is_refused(self):
        result, blob = self.generate('--pl011-console')
        self.assertEqual(result.returncode, 2)
        self.assertIsNone(blob)


if __name__ == '__main__': unittest.main()
