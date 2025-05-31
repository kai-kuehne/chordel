# chordel

**Chordel** is a minimalist musical training and discovery application for the Ableton Push 2. It transforms the Push 2 into an interactive visual tool for learning and practicing chords, scales, and musical structures.

Chordel runs on macOS and Linux, and is written in portable C using SDL2, with zero dependencies on the C standard library. It is designed for low-latency interaction, full control over rendering, and a clear separation between logic and rendering backends.

For a developer write-up on how I build this, see: [https://kaikuehne.net/chordel](https://kaikuehne.net/chordel).

## Demos

[Training Mode](./progress/training-mode/chordel_training_with_ff_arrow.mp4)

[Desktop UI](./progress/chords.mp4)

## Features

- High-performance MIDI input and Push 2 display control
- Grid-based chord visualization and training modes
- Real-time feedback and hinting system
- Native vector rendering using Cairo on the Push 2 display
- Clean C codebase with strict compile flags and minimal dependencies
- Reproducible build setup via Nix flakes

## Requirements

- Ableton Push 2 (you don't need Ableton Live)
- SDL2 and SDL2_ttf
- Cairo
- libusb
- PortMidi
- C compiler (Clang recommended)
- Nix (optional but recommended for development)

## Building

Chordel can be built using either Nix or your system toolchain.

### Option 1: Nix (recommended)

```sh
nix develop
make DESKTOP_UI=1 # or 0 if you don't need the GUI
```

### Option 2: Manual setup

Make sure the following libraries are installed and available via `pkg-config`:

- `sdl2`
- `SDL2_ttf`
- `cairo`
- `libusb-1.0`
- `physfs`
- `portmidi`

Then build with:

```sh
make DESKTOP_UI=1 # or 0 if you don't need the GUI
```

## Usage

Once built, run the application with your Push 2 connected via USB. You just need to supply the path to a SoundFont file:

```sh
./chordel soundfont.sf2
```

If you don't have one handy, [TinySoundFont provides one as part of their examples](https://github.com/schellingb/TinySoundFont/tree/main/examples).

Chordel will initialize the device and display a chord training interface on the Push 2 screen. Follow prompts, play chords, and get feedback in real time. Additional training modes and visualization options are accessible via key bindings.

## Project status

This project is a personal proof-of-concept and a learning experience. I'm not a low-level systems developer by trade, and Chordel is my best effort at working close to the metal. While I’ve tried to make it clean and useful, it’s not a finished product or a general-purpose tool.

I welcome feedback, bug reports, and discussion — but I’m not planning to implement feature requests or provide long-term support.

## Third-party assets and libraries

### Installed via Nix flakes

The libraries listed below are installed via Nix flakes into a development environment. See [flake.nix](flake.nix) for configuration details and https://search.nixos.org/packages for used versions. Channel used: **24.11**.

| Project  | License |
|----------|---------|
| SDL2     | [zlib](https://raw.githubusercontent.com/libsdl-org/SDL/refs/tags/release-2.30.6/LICENSE.txt) |
| SDL2_ttf | [zlib](https://raw.githubusercontent.com/libsdl-org/SDL_ttf/refs/tags/release-2.22.0/LICENSE.txt) |
| cairo    | [LGPL 2.1 or MPL 1.1](https://raw.githubusercontent.com/msteinert/cairo/refs/heads/directfb/COPYING) |
| libusb1  | [LGPL 2.1](https://raw.githubusercontent.com/libusb/libusb/refs/tags/v1.0.27/COPYING) |
| physfs   | [zlib](https://raw.githubusercontent.com/icculus/physfs/refs/tags/release-3.2.0/LICENSE.txt) |
| portmidi | [MIT](https://raw.githubusercontent.com/PortMidi/portmidi/refs/tags/v2.0.4/license.txt) |

### Vendored

This table lists assets and libraries that are directly included within this project.

| Project | License | Location |
|---------|---------|----------|
| https://github.com/rsms/inter | [SIL Open Font License 1.1](assets/ttf/inter/LICENSE.txt) | [assets/ttf/inter](assets/ttf/inter) |
| https://github.com/JetBrains/JetBrainsMono | [SIL Open Font License 1.1](assets/ttf/JetBrainsMono/OFL.txt) | [assets/ttf/JetBrainsMono](assets/ttf/JetBrainsMono) |
| https://github.com/schellingb/TinySoundFont | [MIT](vendor/TinySoundFont/LICENSE) | [vendor/TinySoundFont](vendor/TinySoundFont) |
| https://github.com/troydhanson/uthash | [BSD 2-Clause](vendor/uthash/LICENSE) | [vendor/uthash](vendor/uthash) |
| https://github.com/lemire/cbitset | [Apache License 2.0](vendor/cbitset/LICENSE) | [vendor/cbitset](vendor/cbitset) |
| https://github.com/icculus/physfs/tree/main/extras | [Public Domain](vendor/physfsrwops/physfsrwops.h) | [vendor/physfsrwops](vendor/physfsrwops) |
