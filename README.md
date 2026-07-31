# Driver-Loader

A collection of tools for working with Windows kernel drivers — kernel-mode driver mapping, DSE bypass, and UEFI boot-level patching for research and educational purposes.

---

## Components

### Mapper (KDU-based)

A Windows kernel driver utility based on [KDU](https://github.com/hfiref0x/kdu) (Kernel Driver Utility). It provides the ability to:

- Explore Windows kernel components without a local debugger
- Bypass Driver Signature Enforcement (DSE)
- Load and execute kernel drivers using vulnerable "provider" drivers
- Manipulate kernel objects (process objects, protected processes, etc.)
- Dump virtual memory of target processes
- Launch programs as Protected Process Light (PPL)

The implementation is derived from hfiref0x/KDU with additional features and modifications.

### Compatibility-Module (EfiGuardDxe)

A UEFI bootkit based on [EfiGuard](https://github.com/hfiref0x/EfiGuard) that patches the Windows boot chain at boot time to disable PatchGuard and Driver Signature Enforcement. The module includes:

- **ScootwareCompatDxe** — a DXE driver that patches `bootmgfw.efi`, `bootmgr.efi`, and `ntoskrnl.exe` at boot
- **Loader.efi** — a UEFI shell-based bootloader that automatically finds and boots Windows after applying patches
- **EfiDSEFix.exe** — a userspace application (DSEFix-style) that toggles DSE state via the `SetVariable` EFI runtime service hook
- **VisualUefi** — a modified EDK II development environment (VisualUefi by ionescu007) with prebuilt libraries and Visual Studio project integration

---

## Project Structure

```
Driver-Loader/
├── Mapper/                    # KDU-based kernel driver mapper
│   ├── Mapper/                # KDU source code (main executable)
│   │   ├── main.cpp           # Entry point and CLI handling
│   │   ├── kduprov.cpp        # Provider driver management
│   │   ├── kduplist.h         # Provider list
│   │   ├── drvmap.cpp         # Driver mapping logic
│   │   ├── dsefix.cpp         # DSE override
│   │   ├── autopilot.cpp      # Automated mapping
│   │   ├── shellcode.cpp      # Shellcode injection variants
│   │   ├── victim.cpp         # Vulnerable driver handling
│   │   ├── diag.cpp           # System diagnostics
│   │   ├── ps.cpp             # Process manipulation
│   │   ├── pagewalk.cpp       # Physical memory page walking
│   │   ├── sup.cpp            # Support utilities
│   │   ├── sym.cpp            # Symbol resolution
│   │   ├── drvdb.cpp          # Driver database
│   │   ├── ipcsvc.cpp         # IPC service
│   │   ├── compress.cpp       # Compression utilities
│   │   ├── tests.cpp          # Test suite
│   │   └── ...
│   ├── Shared/                # Shared headers (kdu base, constants)
│   ├── Utils/                 # PCOMP encoder utility
│   ├── drv-DB/                # Driver database tool
│   ├── KDU.vcxproj            # Visual Studio project
│   ├── smap.sln               # Solution file
│   └── build.bat              # Build script
├── Compatibility-Module/      # UEFI bootkit / DSE bypass module
│   ├── EfiGuardDxe/           # DXE driver and application source
│   │   ├── ScootwareCompatDxe.c   # Main DXE driver
│   │   ├── PatchBootmgr.c         # Boot manager patching
│   │   ├── PatchWinload.c         # Winload.pkr patching
│   │   ├── PatchNtoskrnl.c        # Kernel patching
│   │   ├── SmbiosPatch.c          # SMBIOS patching
│   │   ├── ScootwareCompatDxe.h
│   │   ├── util.c / util.h        # Utility functions
│   │   ├── pe.c / pe.h            # PE parsing
│   │   ├── VisualUefi.c           # VisualUefi integration shim
│   │   └── ...
│   ├── VisualUefi/            # Modified VisualUefi (EDK II) submodule
│   ├── ScootwareCompatModule.sln   # Visual Studio solution
│   ├── ScootwareCompatModulePkg.dsc  # EDK II platform description
│   ├── ScootwareCompatDxe.vcxproj   # DXE driver VS project
│   ├── build.bat              # Unified build script
│   ├── build_edk2.bat         # EDK2 build script
│   └── build_edk2_nopause.bat # CI-friendly EDK2 build
├── LICENSE                    # AGPL v3 (see below)
└── README.md
```

---

## Prerequisites

### Software

| Requirement | Version | Purpose |
|---|---|---|
| Microsoft Visual Studio | 2019 or later (v143/v145 toolset) | Building all components |
| Windows Driver Kit (WDK) | 10 or later | Building kernel-mode driver components |
| EDK2 (via VisualUefi) | Latest commit | Building UEFI DXE driver and loader |
| NASM | 2.x | Required by EDK2 build for UEFI binaries |
| Python | 3.x | Required by EDK2 build system |
| Git | — | Cloning submodules (VisualUefi) |

### Environment

- Windows x64 host system
- Administrative privileges for testing (driver loading requires elevated rights)
- The `VisualUefi` submodule must be initialized (`git submodule update --init`) so that `Compatibility-Module/VisualUefi/edk2` is present

---

## Building

### Mapper (KDU)

1. Open `Mapper/smap.sln` in Visual Studio (2019+).
2. Ensure the Windows Driver Kit is installed and the correct target platform version is set (Windows 10.0 or later).
3. Build the `KDU` project for **Release | x64**.
4. The output produces `kdu.exe` and `drv64.dll` in the output directory. Both must reside in the same directory with read/write access for `kdu.exe` to function.

The `build.bat` script automates the full packed build pipeline (PCOMP encoding, resource embedding, linking).

### Compatibility-Module (EfiGuardDxe)

#### Using the Unified Build Script

```
Compatibility-Module> build.bat
```

This script builds the entire solution (`ScootwareCompatModule.sln`) in Release|x64, copies outputs to the `BIN` directory, and handles both the EDK2 DXE driver and the `EfiDSEFix.exe` application.

#### Using EDK2 Directly

1. Navigate to `Compatibility-Module/`.
2. Run `build_edk2.bat` — it will:
   - Detect your Visual Studio installation (VS2019 or VS2022)
   - Set up the EDK2 build environment from `VisualUefi/edk2`
   - Build `ScootwareCompatModulePkg.dsc` in RELEASE mode
   - Copy `ScootwareCompatDxe.efi` and `Loader.efi` to `BIN\`

#### Building EfiDSEFix.exe

`EfiDSEFix.exe` is a standalone Windows application that does not depend on the EDK2/VisualUefi toolchain. It is built as part of the `ScootwareCompatModule.sln` solution and placed in `BIN\` alongside the EFI binaries.

For Visual Studio development with debugging, open `ScootwareCompatModule.sln` and use the QEMU-based debugger configuration defined in `ScootwareCompatModule.props`.

---

## How the Components Work Together

The two components operate in different phases of the Windows boot/runtime lifecycle and complement each other:

1. **Compatibility-Module (boot-time)**: Before Windows boots, the EfiGuardDxe DXE driver intercepts the boot chain. It patches `bootmgfw.efi`, `bootmgr.efi`, `winload.efi`, and `ntoskrnl.exe` to disable PatchGuard and DSE at the firmware level. The `Loader.efi` application provides a convenient way to boot Windows with these patches applied from a USB stick or the EFI system partition. After Windows is running, `EfiDSEFix.exe` can toggle DSE on or off from a userspace command prompt.

2. **Mapper (runtime)**: Once Windows is running with DSE disabled, the Mapper utility can load unsigned or specially crafted kernel drivers into the kernel. It does this by leveraging vulnerable driver "providers" (legitimate signed drivers with exploitable IOCTL handlers) to gain kernel read/write primitives, then uses shellcode to map and execute arbitrary driver code without going through the standard kernel loader.

```
┌──────────────────────────────────────────────────┐
│              Boot Sequence                       │
│                                                  │
│  Firmware → EfiGuardDxe (patches boot chain)    │
│       → Loader.efi (boots Windows)              │
│       → Windows boots (PatchGuard & DSE off)    │
│                                                  │
│  After boot:                                    │
│  EfiDSEFix.exe (optional DSE toggle)            │
│  kdu.exe (load/map unsigned kernel drivers)     │
│                                                  │
│  KDU uses vulnerable provider drivers to        │
│  achieve kernel read/write, then maps           │
│  custom drivers via shellcode injection         │
└──────────────────────────────────────────────────┘
```

---

## Limitations and Known Issues

- **Mapper limitations**: Mapped drivers cannot use standard `DriverEntry` parameters; they must be designed to run "driverless." No SEH support on x64 (exceptions may cause BSOD). Mapped drivers cannot unload themselves. Only `ntoskrnl` imports are resolved automatically — all other module dependencies must be handled by the shellcode. PatchGuard may detect and crash the system if mapped code registers callbacks outside `PsLoadedModulesList`.

- **Compatibility-Module limitations**: Cannot disable Hypervisor-enforced Code Integrity (HVCI/HyperGuard). Checked kernels are not supported. The `SetVariable` DSE hook will cause a `SECURE_KERNEL_ERROR` bugcheck if used to write to `g_CiOptions` when HVCI is active. Some very old or noncompliant firmwares may not support UEFI driver installation (use the Loader application in those cases).

- **General**: Both components are development-grade tools. The `SetVariable` DSE bypass is targeted by some anti-cheat and anti-virus software, which may flag it as malicious. Use EfiGuard's boot-time patch method to avoid this. Always test in virtual machines first.

---

## Educational and Research Use Only

This software is provided for **educational and research purposes only**. Using these tools to:

- Load unauthorized or malicious kernel drivers
- Circumvent security mechanisms in production environments without authorization
- Develop or deploy rootkits or bootkits on systems you do not own

is **illegal** and may violate computer fraud and abuse laws in your jurisdiction. The authors accept no liability for misuse. By using this software, you acknowledge that you are responsible for complying with all applicable laws and regulations.

Always use these tools in controlled environments such as virtual machines or owned test hardware.

---

## Attribution

This project incorporates substantial code from the following projects:

- **KDU** — Copyright (c) hfiref0x, licensed under the [MIT License](https://github.com/hfiref0x/kdu/blob/master/LICENSE). This project uses KDU's architecture, provider framework, shellcode variants, and DSE override techniques.
- **EfiGuard** — Copyright (c) hfiref0x, licensed under GPLv3. The UEFI bootkit components in the Compatibility-Module are derived from EfiGuard's boot-chain patching approach and Zydis-based disassembly routines.
- **VisualUefi** — by ionescu007 (Alex Ionescu), used as the EDK II development environment with Visual Studio integration.
- **Zydis** — by zyantific, licensed under MIT. Used for x64 instruction decoding in the UEFI bootkit.
- **UPGDSED** — by hfiref0x and Fyyre, referenced for DSE bypass methodology.

---

## License

This project, including all original and modified content, is licensed under the **GNU Affero General Public License v3 (AGPL-3.0)**. See the `LICENSE` file for the full text.

The KDU-derived components retain their original MIT license as noted in the KDU source. The AGPL v3 applies to all original and modified content in this repository.

> This program is free software: you can redistribute it and/or modify
> it under the terms of the GNU Affero General Public License as published by
> the Free Software Foundation, either version 3 of the License, or
> (at your option) any later version.
>
> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
> GNU Affero General Public License for more details.
>
> You should have received a copy of the GNU Affero General Public License
> along with this program. If not, see <https://www.gnu.org/licenses/>.

---