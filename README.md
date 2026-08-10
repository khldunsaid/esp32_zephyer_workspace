# ESP32 Zephyr Workspace Application

A Zephyr workspace application targeting Espressif ESP32 SoCs, based on the
[Zephyr example-application][example_app] T2 topology template.

[example_app]: https://github.com/zephyrproject-rtos/example-application

---

## Conventions used in this document

| Placeholder | Meaning |
|---|---|
| `<workspace>` | West workspace root (the directory containing `.west/`); `esp32_app` in the examples below |
| `<app-repo>` | This repository, cloned inside `<workspace>` |
| `<board>` | Target board identifier, e.g. `esp32_devkitc/esp32/procpu` |
| `<port>` | Serial port, e.g. `COM3` or `/dev/ttyUSB0` |

Directory layout:

```
<workspace>/
├── .venv/                 Python virtual environment
├── .west/                 West configuration
├── zephyr/                Zephyr RTOS
├── modules/               Zephyr modules (incl. hal/espressif)
└── <app-repo>/            This repository
    ├── west.yml           Manifest
    └── app/               Application source
        ├── CMakeLists.txt
        ├── prj.conf
        └── src/
```

---

## Prerequisites

- Python 3.10 or newer
- CMake, Ninja, and Git
- **7-Zip on `PATH`** (Windows only) — the Zephyr SDK setup script invokes `7z`
  directly and cannot locate it via the registry

Activate the virtual environment before every session. A new terminal always
starts outside it, and running `west` without it picks up a different Python,
producing misleading `ModuleNotFoundError` failures.

```shell
# Windows
<workspace>\.venv\Scripts\Activate.ps1
# Linux / macOS
source <workspace>/.venv/bin/activate
```

Verify:

```shell
which west      # or: where.exe west
```

The path shown must be inside the workspace virtual environment.

---

## Initial setup

### 1. Initialize the workspace

```shell
west init -m https://github.com/zephyrproject-rtos/example-application --mr main esp32_app
cd esp32_app
west update
```

If the application directory is later renamed, west's recorded manifest path
will no longer match. Repair it with:

```shell
west manifest --path                    # show the manifest west is reading
west config manifest.path <app-repo>
```

`west config` does not parse the manifest, so it works even when the workspace
is otherwise broken.

### 2. Add the Espressif HAL to the manifest

The example-application template imports Zephyr through a `name-allowlist`
containing HALs for other vendors and omitting Espressif. Replace that list with
`hal_espressif` in `<app-repo>/west.yml`:

```yaml
    - name: zephyr
      remote: zephyrproject-rtos
      revision: main
      import:
        name-allowlist:
          - hal_espressif
```

Removing the `name-allowlist` block entirely imports Zephyr's full module set —
a larger clone, but it avoids further missing-dependency errors.

Then update the workspace:

```shell
west update
west list | grep hal
```

### 3. Install the Zephyr SDK

```shell
west sdk install -t xtensa-espressif_esp32_zephyr-elf
west sdk list
```

Select the toolchain matching the target silicon:

| Target | Toolchain |
|---|---|
| ESP32, ESP32-S2, ESP32-S3 | `xtensa-espressif_esp32_zephyr-elf` |
| ESP32-C3, C6, H2 | `riscv64-zephyr-elf` |

Omitting `-t` downloads the full multi-gigabyte SDK.

### 4. Export the Zephyr CMake package

```shell
west zephyr-export
```

Registers Zephyr in the local CMake package registry so `find_package(Zephyr)`
resolves.

### 5. Fetch the Espressif binary blobs

```shell
west blobs fetch hal_espressif
```

Downloads the closed-source RF libraries — Wi-Fi MAC/PHY (`libnet80211.a`,
`libpp.a`, `libcore.a`, `libphy.a`), Bluetooth controller, and coexistence — to
`modules/hal/espressif/zephyr/blobs/`. Espressif does not publish sources for
these.

**Wi-Fi and Bluetooth will not link without them.** `west update` deliberately
does not fetch blobs; bringing non-source binaries into the tree is an explicit
choice.

```shell
west blobs list hal_espressif
west blobs clean hal_espressif
```

### 6. Install Python requirements

```shell
west packages pip --install
```

Collects requirements from Zephyr and every module in the workspace, including
the `esptool` version pinned by `hal_espressif`.

---

## Build and flash

```shell
cd <workspace>/<app-repo>
west build -b <board> -p always app
west flash
```

- The source directory argument is relative to the current working directory.
- `-p always` forces a clean build. **Required** after editing `prj.conf`,
  devicetree files, or any Kconfig — stale configuration otherwise produces
  results that do not match the source.
- Subsequent `west build` calls reuse the previous board and source directory.

List available boards:

```shell
west boards | grep esp32
```

Flash to an explicit port:

```shell
west flash --esp-device <port>
```

Serial monitor:

```shell
west espressif monitor
```

Apply an extra configuration fragment:

```shell
west build -b <board> app -- -DEXTRA_CONF_FILE=debug.conf
```

Run Twister tests:

```shell
west twister -T tests --integration
```

---

## Troubleshooting

### `MalformedConfig: manifest file ... not found`

`.west/config` references a directory that does not exist, typically after the
application repository was renamed. Correct it with
`west config manifest.path <app-repo>`.

### `ModuleNotFoundError` (`patoolib`, `jsonschema`, others)

Either the virtual environment is inactive, or Zephyr's Python requirements are
missing. Check the shell prompt, then run `west packages pip --install`.

The `AttributeError: 'NoneType' object has no attribute 'err'` that follows is
a defect in west's own error handler. Ignore it and address the underlying
error reported above it.

### `Zephyr SDK setup requires '7z' to be installed and available in the PATH`

The download and extraction succeeded; only the final setup step failed. Add
the 7-Zip installation directory to `PATH`, reopen the terminal, reactivate the
virtual environment, and re-run `west sdk install`.

### `FATAL ERROR: Unknown module(s): {'hal_espressif'}`

The module is absent from the workspace. Verify with `west list`, correct the
manifest `name-allowlist` (setup step 2), then `west update`.

### `esptool>=X.Y.Z not found in PATH`

Run `west packages pip --install` with the virtual environment active. Confirm
with `esptool version` — the executable is installed into the environment's
script directory, which is only on `PATH` while the environment is active.

### `'__device_dts_ord_DT_N_NODELABEL_<name>_ORD' undeclared`

The referenced devicetree node does not exist for the selected board. The
example-application template's `main.c` uses demo nodes (`example_sensor`,
`blink_led`) defined only in the overlays under `app/boards/`, which cover a
limited set of boards. Either replace the template source with application
code, or add `app/boards/<board>.overlay` defining the required nodes.

### `boot:0x3 (DOWNLOAD_BOOT)` — repeating "waiting for download"

GPIO0 was low at reset, so the ROM entered the serial bootloader and never
started the flashed image. Causes, in order of likelihood:

1. The terminal emulator is asserting DTR/RTS, which are wired to GPIO0 and EN
   on most development boards. Set flow control to **None**, or use
   `west espressif monitor`.
2. The BOOT button is held or mechanically stuck. Release it and reset.
3. Stale state — power-cycle with no buttons held.

A normal boot reports `boot:0x13 (SPI_FAST_FLASH_BOOT)`.

### Build succeeds but nothing appears on serial

Confirm `CONFIG_LOG`, `CONFIG_SERIAL`, and `CONFIG_UART_CONSOLE` are enabled,
and that the terminal baud rate is 115200.

---

## Reference: healthy boot output

```
ets <date>
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
mode:DIO, clock div:2
I (soc_init): ESP Simple boot
I (soc_init): chip revision: <rev>
I (flash_init): SPI Flash Size : 4MB
I (boot): libc heap size <N> kB.
I (spi_flash): detected chip: generic
*** Booting Zephyr OS build <version> ***
```

A `Unicore bootloader` warning is expected on dual-core parts: Zephyr starts
only the PRO CPU unless SMP is enabled. This does not affect Wi-Fi operation.

---

## License

<!-- Specify the project license here. -->
