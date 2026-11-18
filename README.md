# Xenon 2 Megablast (Image Works, 1989)

Port set-up using the `amiga-porting` skill (`~/git/retrofoundry/amiga_porting`).

## Disks

The game is two disks; the single-disk Black Monks release is not enough.

- Disk 1: `Copylock Amiga Key Disk` (PNA crack), boots, then asks for the game disk.
- Disk 2: game disk, boot block `X2B ` at offset 2 (not a DOS boot block).
- The game's own raw MFM loader ($921a, `rabs`: d0 = first sector, d1 = count, a0 = destination) reads DF0 only.
  At $8fde it requires `X2B ` in sector 0, otherwise it shows INSERT GAME DISK and retries.
- So disk 2 has to replace disk 1 in DF0: PageDown in the window (host disk control), or `ami.disk_eject/select`.
- `local/xenon2.adf` is the Black Monks single-disk release (flag `bm`); it never gets past INSERT GAME DISK.

## Files (git-ignored `local/`)

- `local/system/kick34005.A500`: Kickstart 1.3 (CRC32 c4f0f55f)
- `local/xenon2-disk1.adf`, `local/xenon2-disk2.adf`, `local/xenon2.m3u`
- `local/save/frame-8134.state` loading from disk, `frame-8693.state` level loaded (waits for fire),
  `frame-8880.state` level running. Older states `6998/7637/8973` are the single-disk release (`bm`),
  `6159/6538` are pre-menu and menu with both disks.

## Run

```
AMI=../amiga_porting/ami9000/build/ami9000_host
python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/xenon2.lua --script-args joy1          # window
python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/xenon2.lua --script-args "joy1 load=frame-8880"
```

The title and menu poll the joystick in port 1 (`joy1`); arrows = stick, Ctrl = fire. Flags are listed at the top
of `host/xenon2.lua`. `host/ramdump.lua` dumps chip RAM and registers from a state, `host/rabs_trace.lua` logs the
game's disk reads.
