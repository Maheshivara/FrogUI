# FrogUI Development Guide

This guide covers building, compiling, and developing FrogUI for SF3000/R36SX handhelds.

---

## Table of Contents

- [Building from Source](#building-from-source)
- [Technical Details](#technical-details)
- [Development Guidelines](#development-guidelines)
- [Directory Structure](#directory-structure)
- [Debugging](#debugging)

---

## Building from Source

### Prerequisites

- SF3000 cross-toolchain (MIPS32r2, little-endian, hard-float) at
  `~/sf3000-work/sf3000toolchain/`. Download:
  [game-de-it/sf3000 · sf3000_toolchain_v0.1](https://github.com/game-de-it/sf3000/releases/tag/sf3000_toolchain_v0.1)
- Make

### Build Commands

TreeFrogUI builds FrogUI as a **libretro core** (`frogui_libretro.so`) that runs
inside picoarch. Use the libretro build script:

```bash
./build_libretro.sh
```

Or the Makefile with:

```bash
make all
```

### Build Outputs

This will create:

- `OUT_DIR/app/*.lo` - The intermediary objects
- `OUT_DIR/harfbuzz` - The harfbuzz external lib objects
- `OUT_DIR/sheenbidi` - The sheenbidi external lib objects
- `OUT_DIR/frogui_libretro.so` - The FrogUI libretro core
- `OUT_DIR/frogui_libretro.harfbuzz.LICENSE` - The harfbuzz license copy
- `OUT_DIR/frogui_libretro.sheenbidi.LICENSE` - The sheenbidi license copy

---

## Technical Details

### Screen Resolution

- Runtime panel geometry, detected by picoarch:
  - R36SX: 640x480 pixels (4:3)
  - SF3000/SF3500/SF3100/HD: 854x480 pixels (16:9)
- RGB565 color format

### Input Handling

- Supports RETRO_DEVICE_JOYPAD on port 0
- Button mapping:
  - Up/Down: Navigate menu
  - L/R: Jump by 7 entries
  - A: Select/Enter
  - B: Back
  - Select: Settings menu
  - X: Theme settings
  - Y: Recent games

### File System

- Uses custom `dirent.h` implementation for SF2000
- Scans `/mnt/sdcard/ROMS` for directories and files
- No limit on entries per directory (dynamic allocation)
- Supports hidden folders (starting with `.` or named `save`)

### Core Integration

- No ROM loading required (supports `RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME`)
- Runs at 60 FPS
- No audio output
- Integrates with multicore save state system

### Typography

- Font: Multiples (See [fonts](../fonts) dir)
- Font size: 18 <-> 26px

### Thumbnail System

- Format: Raw RGB565 (.rgb565 extension), PNG or JPG files
- Location: `.res` subdirectories alongside ROMs
- Supported dimensions: 64x64, 128x128, 160x160, 200x200, 250x200, 200x250

---

## Development Guidelines

### Code Style

- Follow existing code conventions and patterns
- Use descriptive variable names
- Add logging with `dbg()` for debugging (See [debug.c](../src/app/debug.c))
- Prefer static buffers over dynamic allocation

### Git Commit Guidelines

- All commit messages must be single line only
- Never mention Claude, AI assistance, or automated generation
- Use present tense, imperative mood (e.g., "Add feature", "Fix bug")
- Keep messages concise and descriptive

### Testing

- Test on actual SF3000/R36SX hardware when possible
- Verify memory usage (avoid malloc/free in critical paths)
- Test with various ROM collections and filename lengths
- Check navigation edge cases (empty folders, long names, etc.)

---

## Directory Structure

### On Device

```
/mnt/sdcard/
├── roms/
│   ├── gba/          <- FrogUI shows this folder
│   │   ├── game.gba
│   │   └── .res/
│   │       └── game.rgb565
│   ├── nes/
│   ├── snes/
│   └── ...
├── cubegm/
│   └── cores/
│       └── frogui_libretro.so
```

## Debugging

### Debug Logs

On the device, check `log.txt` on the SD card root for runtime debugging information.

### Common Issues

**Build fails:**

- Ensure multicore toolchain is properly installed
- Check that you're building from the correct directory
- Verify all dependencies are available

**Core doesn't load:**

- Check that the core file is at `sd:/cubegm/cores/frogui_libretro.so`
- Check device compatibility

**Navigation issues:**

- Check log files for filesystem errors
- Verify directory permissions
- Test with simpler directory structures first

### Adding Features

When adding new features:

1. Follow existing patterns in the codebase
2. Add debug logging for new functionality
3. Test thoroughly on hardware
4. Update documentation in README.md
5. Consider memory constraints of the device

---

## Contributing

Contributions are welcome! Please:

1. Follow the development guidelines above
2. Test changes on actual hardware when possible
3. Write clear, single-line commit messages
4. Document new features in appropriate README sections

---

## License

See [LICENSE](LICENSE) for licensing information.

FrogUI is licensed under CC BY-NC-SA 4.0. For commercial licensing inquiries, contact the project maintainers.
