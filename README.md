# Native NVCP Toggle

A fast, native C implementation for toggling NVIDIA display color settings. Near-instant execution with no runtime dependencies.

## Features

- **Digital Vibrance** - Color saturation control (50-100%)
- **Hue** - Color wheel rotation (0-359°)
- **Brightness / Contrast / Gamma** - Display calibration via Windows API
- **Color Temperature** - Warm/cool tint adjustment (-100 to +100)
- **Toggle behavior** - Run once to apply settings, run again to reset to defaults

## Download

Get the latest release from the [Releases](https://github.com/zifre/native_nvcp_toggle/releases) page.

Download both files:
- `native_nvcp_toggle.exe` - The executable
- `native_nvcp_config.ini` - Configuration file

Place them in the same folder.

## Usage

1. Edit `native_nvcp_config.ini` with your desired settings
2. Run `native_nvcp_toggle.exe` to apply your custom settings
3. Run again to reset to defaults
4. **Tip:** Pin to taskbar or create a keyboard shortcut for quick access

## Configuration

Edit `native_nvcp_config.ini` to customize your display settings:

```ini
# General
toggleAllDisplays=false    # true = all displays, false = primary only
keyPressToExit=false       # true = wait for keypress, false = exit immediately

# NVIDIA settings (requires NVIDIA GPU)
vibrance=60                # 50 (default) to 100 (max saturation)
hue=0                      # 0-359 degrees

# Windows gamma ramp (works with any GPU)
brightness=0.5             # 0.0 to 1.0 (default 0.5)
contrast=0.5               # 0.0 to 1.0 (default 0.5)
gamma=1.0                  # 0.5 to 3.0 (default 1.0)
temperature=0              # -100 (cool/blue) to +100 (warm/yellow)
```

## Requirements

- Windows 10/11
- NVIDIA GPU with drivers installed (for vibrance/hue control)

---

## Building from Source

If you want to build the project yourself:

### Prerequisites

- Visual Studio 2019+ with C++ build tools
- NVAPI SDK from [NVIDIA](https://developer.nvidia.com/nvapi)

### Setup

1. Clone this repository
2. Download the NVAPI SDK and extract it
3. Rename/move the SDK folder to `nvapi/` inside this project directory

Your folder structure should look like:
```
native_nvcp_toggle/
├── nvapi/
│   ├── nvapi.h
│   ├── x86/
│   │   └── nvapi.lib
│   └── amd64/
│       └── nvapi64.lib
├── native_nvcp_toggle.c
├── build.bat
└── ...
```

### Build

1. Open **Developer Command Prompt for VS** (x86 version)
2. Navigate to directory
3. Run:
   ```batch
   build.bat
   ```
4. Output: `native_nvcp_toggle.exe`

---

## Technical Notes

- Digital vibrance and hue use undocumented NVAPI functions (may break with future driver updates)
- Gamma ramp settings are applied via Windows GDI, not NVIDIA Control Panel
- The toggle detects state by comparing current values against defaults (vibrance=50%, hue=0, linear gamma)

## License

MIT License
