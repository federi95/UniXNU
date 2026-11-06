# QEMU testing

QEMU tests UniXNU, the injection paths, DarwinBoot and the kernel entry without a device.

## Procedure {#procedure}

1. Install the [requirements](#requirements), including AArch64 firmware. Choose the behavior to exercise: common UEFI, a staged ESP or XNU entry.
2. For the maintained common UEFI scenarios, run the [smoke test](#smoke-test). For custom EFI inputs, first [stage an ESP](#stage-an-esp), then use [Run](#run) with expected log markers.
3. For kernel-entry work, prepare the [XNU entry fixture](#xnu-entry-fixture) and follow its runner options. Record the kernel and ADT used.
4. Inspect the selected scenario's logs and screenshot. Record the exact expected marker reached and the timeout or failure when it was not reached. Continue with device [bring-up](/porting/bring-up) for board resources.

## Requirements

| Tool | Purpose |
| --- | --- |
| `qemu-system-aarch64` | AArch64 virtual machine |
| AAVMF or EDK2 AArch64 firmware | UEFI environment |
| `dtc` | Device tree compilation |
| `mtools` or `pyfatfs` | FAT image for the block slice test |

The QEMU runner looks for the firmware in the common Homebrew and distribution paths, or takes `--firmware`.

## Stage an ESP

```sh
python3 package/Scripts/stage-esp.py -o out/esp \
  package/UniXNUPkg/Test/Qemu/esp \
  --put out/<device>/<os_profile>/<config>/package/Build/Out/AARCH64-DEBUG/UniXNU.efi=EFI/BOOT/BOOTAA64.EFI \
  --put out/<device>/<os_profile>/<config>/package/Build/Out/AARCH64-DEBUG/HelloPayload.efi=EFI/UniXNU/Payloads/HelloPayload.efi \
  --put out/<device>/<os_profile>/<config>/package/Build/Out/AARCH64-DEBUG/SampleDriver.efi=EFI/UniXNU/Kits/SampleDriver.kit/SampleDriver.efi
```

`stage-esp.py` copies the tree, compiles `.dts` to `.dtb` and `.dtso` to `.dtbo`, and places each `--put` file at its destination.

## Run

```sh
python3 package/Scripts/qemu-run.py out/esp \
  --log out/qemu.log --screenshot out/menu.png \
  --expect 'UNIXNU: menu: shown' \
  --expect 'UNIXNU-HELLO: RESULT PASS'
```

| Option | Meaning |
| --- | --- |
| `--firmware FILE` | AAVMF code image |
| `--log FILE` | Serial log |
| `--screenshot FILE` | PNG captured when the menu appears |
| `--keys LIST` | QEMU key codes sent after the menu |
| `--expect REGEX` | Pattern that must match the serial log |
| `--reject REGEX` | Pattern that must not match |
| `--no-gop` | No display device, text console only |
| `--display-size WxH` | Virtio GPU display of this size instead of ramfb, for example `1080x2400` for a portrait panel. The firmware needs at least 640 pixels of width |
| `--disk FILE` | Extra read-only raw disk |
| `--timeout SECONDS` | Run timeout |
| `--cpus N` | Virtual CPU count |
| `--finish-on-expect` | Stop two seconds after all expectations match |
| `--menu-marker TEXT` | Log marker that triggers the screenshot and keys |

The runner exposes the ESP read-only through QEMU `vvfat`, captures the serial log, and talks to QEMU over QMP for screenshots and keys.

## Smoke test

```sh
JOBS=8 bash package/Scripts/smoke.sh
```

The script builds the package and runs six scenarios:

| Scenario | Checks |
| --- | --- |
| Default entry | Device tree patches, overlay, kit driver, initrd media, entry variable |
| Keyboard | Down and Enter select the second entry |
| Degraded | No GOP and no configuration, built-in entries, text console |
| Block slice | Configuration on a FAT partition head, tail published as a block device |
| Kext staging | Revision 2 handoff, revision 1 refusal, invalid bundles, duplicates |
| Layout | A 1080x2400 display uses portrait, an 800x1000 display stays landscape |

Logs and screenshots go to `package/Build/Smoke/`.

## XNU entry fixture

The QEMU entry generator builds an Apple-format device tree for the kernel:

```sh
python3 scripts/qemu-entry/make-dt.py \
  --armvirt --armcpu --smp 2 \
  --owned-memory-size 0x20000000 \
  --output build/qemu-test.adt
```

| Option | Meaning |
| --- | --- |
| `--armvirt` | Describe QEMU virt devices |
| `--armcpu` | Add the GICv3 distributor and redistributor |
| `--smp N` | CPU count and PSCI |
| `--owned-memory-size` | Contiguous DarwinBoot window |
| `--data-ramdisk` | Scratch Data placeholder |
| `--trust-cache` | Trust cache placeholder |
| `--nvram-proxy` | Volatile CHRP NVRAM proxy |
| `--root-match-volume-uuid` | Root volume match |
| `--osenvironment` | `main`, `recoveryos` or `ramdisk` |
| `--qemu-vmm-present` | Report the VMM in `/defaults` |
| `--crypto-hash-method` | Image4 hash method |

The generator describes QEMU and uses QEMU values only.

## HVF

On Apple Silicon macOS, QEMU can use HVF:

```sh
python3 scripts/qemu-entry/make-dt.py --armvirt --armcpu --qemu-vmm-present \
  --qemu-accel hvf --output build/qemu-hvf.adt
```

The generator reads the host counter frequency for the CPU timebase. The bound fixture is used to observe the actual `TCR_EL1`, `SCTLR_EL1` and `TTBR` registers at the kernel stop point.

## Stop point

```sh
python3 scripts/qemu-entry/gdb-stop.py --help
```

The helper stops QEMU at a symbol and records registers, memory and the last checkpoint. The output is a JSON report.

## Next steps

- [Bring-up order](/porting/bring-up)
- [Four-kilobyte pages](/xnu/four-k-page-tables)
- [Troubleshooting](/reference/troubleshooting)

## Public source crypto fixture

`XNU_PUBLIC_CRYPTO_FIXTURE=1` is an opt-in ordinary ARM64/iOS 17 ArmVirt
source-owner diagnostic. Select `profiles/arch/arm64/patches/xnu/source-public-crypto-fixture.patch`
after the existing source-provider bootstrap patch. The build links the fixture
archive and recompiles only the bootstrap caller with its diagnostic define.
The fixture runs after successful complete default POST, preserves registration
and exports bounded counters plus owner pointers before/after. It executes
340 deterministic digest cases (85 via the existing kernel entry) and 120 DER
cases. `unixnu_crypto_fixture_bad_expected=1` deliberately changes one expected
digest and must panic after recording one failure. No original component, keys
or entropy state is read, and no provider/OS/hardware compatibility is implied.

## Separate public Darwin console

`XNU_OPEN_SOURCE_DEMO=1` is a separate ArmVirt/MockFS source-owner profile,
with the original-security runtime disabled. Apply the opt-in
`source-open-demo-entitlements.patch` after the ordinary source patch series.
It permits an empty entitlement-query context for a plain ad-hoc image when
AMFI is absent, refuses raw entitlement-bearing/platform/non-ad-hoc images
before signature reconstitution, and answers entitlement queries with false
or NULL when no provider exists. It installs no fake provider. Code-signing
structure/page checks remain active; process-wide code integrity is enabled.
The source crypto POST is unchanged. This does not qualify Image4 or iOS.

The existing PL011 backend is selected only for this profile. The independent
ADT option `--pl011-console` exposes virt UART0 at 0x09000000/0x1000 through
the existing arm-io range. It requires `--armvirt`. Default fixtures omit it;
TCG and HVF keep distinct timebases. A source-built PID 1 must align the initial
exec stack before calling C; the raw exec stack can be only 8-byte aligned.
No phone addresses, firmware or Apple device identity are used.


## Isolated public GUI scanout

The additional `XNU_OPEN_SOURCE_GUI=1` mode requires `XNU_OPEN_SOURCE_DEMO=1`.
Its separate `source-open-gui-scanout.patch` is not in the default patch list.
It provides `kern.open_darwin_fb` only in this public ArmVirt profile: root
PID 1 and explicit `open-gui=1` are required. Geometry comes from
`PE_current_console`, not a board constant. Only unrotated 32-bit GOP modes
up to 2048x1536, an aligned stride no larger than 8192 and an exact full-frame
write are admitted. The mapper declaration matches the pinned XNU header;
no user-supplied physical address or direct userspace MMIO mapping is accepted.
The 16-byte query returns version, width, height and byte stride. Writes copy
pixels from the caller to the bounded scanout and retain the existing source
crypto/integrity policy. This is an experimental public-demo API, not the
MobileFramebuffer/IOSurface ABI or an Apple WindowServer/SpringBoard result.

The parent q5q workspace supplies an owned static GUI process and a local
viewer: QMP transports actual ramfb pixels, and click/key events reach the
guest over serial. HVF and TCG prototypes render a home view, notes and a
working calculator; an actual short-frame write returns EINVAL. This does
not validate USB/HID, a GPU or any physical phone driver. Existing native
GopFramebuffer/IOSurface integration remains a separate component path.
