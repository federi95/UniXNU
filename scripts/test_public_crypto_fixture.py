import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('fixture_builder',Path(__file__).with_name('build-public-crypto-fixture.py'))
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)

class FixtureBuildTests(unittest.TestCase):
    def command(self):return ['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-mkernel','-ffreestanding','-DKERNEL=1','-DCC_USE_ASM=0']
    def report(self):return {'source_revision':'11fba65b12d8830630e248dfa8344caf8b065f26','failures':0,'archive_sha256':'a'*64}
    def test_actual_provider_contract(self):builder.admit_provider(self.report(),'a'*64,self.command())
    def test_wrong_archive_revision_or_failures_refuse(self):
        for key,value in [('archive_sha256','b'*64),('source_revision','other'),('failures',1)]:
            record=self.report();record[key]=value
            with self.assertRaises(ValueError):builder.admit_provider(record,'a'*64,self.command())
    def test_userland_or_wrong_architecture_refuse(self):
        for flag in ('-mkernel','-ffreestanding','-DKERNEL=1','-DCC_USE_ASM=0'):
            command=self.command();command.remove(flag)
            with self.assertRaises(ValueError):builder.admit_provider(self.report(),'a'*64,command)
        command=self.command();command[command.index('-target')+1]='arm64e-apple-ios17.0'
        with self.assertRaises(ValueError):builder.admit_provider(self.report(),'a'*64,command)
    def test_golden_vectors_are_deterministic_and_cover_all_algorithms(self):
        first=builder.expected_header();self.assertEqual(first,builder.expected_header())
        self.assertIn('[5][17][64]',first)
        # Fixed SHA-256 abc anchor, independent of fixture's generated syntax.
        expected=bytes.fromhex('ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
        self.assertIn('{'+','.join(str(x) for x in expected)+'}',first)

if __name__=='__main__':unittest.main()
