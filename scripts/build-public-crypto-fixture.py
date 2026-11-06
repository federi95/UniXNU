#!/usr/bin/env python3
"""Build deterministic public service fixtures with the source provider's flags.

Opt-in native-only test archive; never acquires firmware or registers a provider.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from kernel_target import check_object

LENGTHS=(0,1,3,55,56,63,64,65,111,112,127,128,129,255,256,257,385)
ALGORITHMS=('sha1','sha224','sha256','sha384','sha512')
FIELDS=('version','state','digest_cases','der_cases','failures','guard_checks','clear_checks','bridge_cases',
        'der_accepts','der_refusals','guard_failures','clear_failures')

def expected_header():
    vectors=[]
    for algorithm in ALGORITHMS:
        rows=[]
        for length in LENGTHS:
            data=b'abc' if length==3 else bytes((17*i+3)&255 for i in range(length))
            rows.append(hashlib.new(algorithm,data).digest())
        vectors.append(rows)
    return 'static const unsigned char unixnu_digest_expected[5][17][64] = {\n'+',\n'.join('{'+','.join('{'+','.join(str(b) for b in row)+'}' for row in rows)+'}' for rows in vectors)+'\n};\n'


def admit_provider(report, archive_sha256, command):
    if (report.get('source_revision')!='11fba65b12d8830630e248dfa8344caf8b065f26' or report.get('failures')!=0
            or report.get('archive_sha256')!=archive_sha256):
        raise ValueError('A matching tested public source-provider archive is required')
    if (command[:4]!=['xcrun','--sdk','iphoneos','clang'] or '-target' not in command
            or command[command.index('-target')+1]!='arm64-apple-ios17.0'
            or any(flag not in command for flag in ('-mkernel','-ffreestanding','-DKERNEL=1','-DCC_USE_ASM=0'))):
        raise ValueError('Expected actual ordinary ARM64 public-provider kernel compile flags')


def build():
    provider=ROOT/'build/corecrypto-2023'
    report=json.loads((provider/'build-report-namespaced.json').read_text())
    archive=provider/'libcorecrypto_kernel_noasm-namespaced.a'
    command=json.loads((provider/'compile-command-namespaced.json').read_text())
    admit_provider(report,hashlib.sha256(archive.read_bytes()).hexdigest(),command)
    out=ROOT/'build/public-crypto-fixture';out.mkdir(exist_ok=True)
    source=ROOT/'scripts/fixtures/public-crypto-fixture.c'
    header=out/'public-crypto-expected.h'
    header.write_text(expected_header())
    obj=out/'public-crypto-fixture.o'
    argv=command+['-I',str(out),'-c',str(source),'-o',str(obj)]
    (out/'command.json').write_text(json.dumps(argv,indent=2)+'\n')
    result=subprocess.run(argv,cwd=ROOT,capture_output=True,text=True)
    (out/'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode:raise ValueError('Fixture compilation failed: '+result.stderr[-4000:])
    check_object(obj.read_bytes(),obj.name)
    output=out/'libPublicCryptoFixture.a';output.unlink(missing_ok=True)
    subprocess.run(['xcrun','ar','rcs',str(output),str(obj)],check=True)
    record={'schema':'unixnu-public-crypto-fixture-build-v1','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'expected_header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),
            'provider_archive_sha256':report['archive_sha256'],'archive_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
            'fields':list(FIELDS),'result_bytes':48,'expected':dict(zip(FIELDS,(1,2,340,120,0,340,170,85,54,66,0,0))),
            'oracle':'Python hashlib; deterministic owned vectors','original_code_executed':False,'provider_registration_added':False}
    (out/'build-report.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS public service fixture archive; 340 digest/120 DER cases; no provider initialization')

if __name__=='__main__':build()
