# Micropolis (SimCity Classic) — Tiny Engine Port

An authentic, modern port of **Micropolis** (the open-source release of Will Wright's classic **SimCity**, originally created by Maxis and released under GPL v3 by Electronic Arts / Don Hopkins) to the **Tiny Engine**.

This port brings the classic 1989 city builder to both **Desktop (Linux, macOS, Windows via SDL2)** and the **Panic Playdate (400x240 1-bit monochrome handheld console)**.

---

## Table of Contents

- [About the Game](#about-the-game)
- [About the Port](#about-the-port)
- [Features](#features)
- [Controls](#controls)
  - [Desktop (SDL2 / Keyboard & Mouse)](#desktop-sdl2--keyboard--mouse)
  - [Playdate Handheld Console](#playdate-handheld-console)
- [Building & Running](#building--running)
- [Attribution & Credits](#attribution--credits)
- [License](#license)

---

## About the Game

Originally released in 1989 by Maxis, *SimCity* defined the city-building simulation genre. Players act as mayor and urban planner, founding a city on procedurally generated terrain, zoning land, providing electricity and transit networks, managing municipal budgets, and responding to disasters.

Key simulation mechanics preserved in full:
- **Cellular Automata Simulation**: Dynamic grid simulation modeling growth, decay, power grids, traffic flows, pollution propagation, crime, and land value.
- **Zoning**: Residential (green), Commercial (blue), and Industrial (yellow) development responding to R/C/I market demand valves.
- **Infrastructure & Utilities**: Coal and Nuclear power plants, electrical power lines, transit roads, sea ports, airports, and sports stadiums.
- **Municipal Budgeting**: Setting property tax rates and allocating funding percentages for Road Maintenance, Police Departments, and Fire Protection.
- **Public Opinion & Evaluation**: Annual citizen evaluations, mayor approval ratings, city classification (*Village* to *Megalopolis*), and top problem surveys.
- **Disasters**: Real-time emergencies including Fires, Floods, Earthquakes, Tornados, Monster attacks, and Nuclear Power Plant meltdowns.
- **Historical Scenarios**: 8 classic campaign scenarios:
  1. *Dullsville (1900)* — Cure economic stagnation.
  2. *San Francisco (1906)* — Recover from the historic earthquake.
  3. *Hamburg (1944)* — Rebuild after devastating wartime firestorms.
  4. *Bern (1965)* — Overhaul the transit grid to resolve city-wide gridlock.
  5. *Tokyo (1957)* — Reconstruct the metropolis following a monster attack.
  6. *Detroit (1972)* — Deploy law enforcement to curb runaway crime.
  7. *Boston (2010)* — Contain a catastrophic nuclear power meltdown.
  8. *Rio de Janeiro (2047)* — Fortify the coast against raging flash floods.

---

## About the Port

This port adapts the open-source Micropolis simulation core into **Tiny Engine**, a modular, lightweight C game engine engineered for high-performance 2D rendering across desktop computers and embedded handhelds.

This port starts from works done in https://github.com/tenox7/vtcity.

### Architecture Highlights
- **Engine Decoupling**: Core cellular automata simulation routines run at a fixed 20 Hz tick, cleanly decoupled from the 60 FPS graphical rendering and input processing loop.
- **C99 Standard Modernization**: The entire codebase has been refactored from legacy K&R / GNU C89 to clean, strict standard C99 (`-std=c99`), compiling warning-free on modern compilers (`gcc`, `clang`, and `arm-none-eabi-gcc`).
- **Texture-Based Graphical Pipeline**: Replaced legacy DEC VT terminal escape sequences and X11/Tk dependencies with a modern 16x16 tile blitter and master sprite sheet (`assets/gfx/tiles_16.png` and `assets/gfx/tileset.json`).
- **Dynamic Sprite Subsystem**: Smooth rendering of animated entities including passenger trains, police/traffic helicopters, commercial airplanes, cargo ships, monsters, tornados, and multi-stage explosions.
- **Playdate 1-Bit Monochrome Optimization**:
  - Solid black UI panels (`rgba_black()`) with crisp white double borders (`rgba_white()`) to eliminate dithering speckles on the Playdate LCD.
  - Pixel-perfect building placement aligning 3x3, 4x4, and 6x6 structures directly with the on-screen cursor outline.
  - Native integration with the Playdate OS system menu.
- **Audio Subsystem**: 27 classic sound effects converted from raw PCM to 16-bit mono 44.1 kHz WAV assets.

---

## Controls

The game provides full input support for both keyboard/mouse on Desktop and the D-pad/crank/buttons on the Playdate handheld.

### Desktop (SDL2 / Keyboard & Mouse)

#### Navigation & Map Interaction
| Input | Action |
| :--- | :--- |
| **Arrow Keys** / **Numpad (8, 2, 4, 6)** | Move map cursor (with edge scrolling) |
| **Left Mouse Click** | Place active tool / select UI item |
| **Left Mouse Click + Drag** | Continuously pave roads, wires, or bulldoze |
| **Right Mouse Click** | Inspect / query tile under mouse pointer |
| **Mouse Wheel** | Cycle through construction tools |
| **Spacebar** / **Enter** / **Z** | Place active tool (Primary Action) |
| **X** / **Backspace** | Inspect zone query / Cancel (Secondary Action) |

#### Direct Tool Shortcuts
| Key | Tool | Size | Cost |
| :---: | :--- | :---: | :---: |
| <kbd>R</kbd> | Road | 1x1 | $10 |
| <kbd>W</kbd> | Power Wire | 1x1 | $5 |
| <kbd>B</kbd> | Bulldozer | 1x1 | $1 |
| <kbd>Z</kbd> | Residential Zone | 3x3 | $100 |
| <kbd>C</kbd> | Commercial Zone | 3x3 | $100 |
| <kbd>I</kbd> | Industrial Zone | 3x3 | $100 |
| <kbd>F</kbd> | Fire Station | 3x3 | $500 |
| <kbd>O</kbd> | Police Department | 3x3 | $500 |
| <kbd>S</kbd> | Sports Stadium | 4x4 | $3,000 |
| <kbd>P</kbd> | Public Park | 1x1 | $10 |
| <kbd>T</kbd> | Seaport | 4x4 | $5,000 |
| <kbd>L</kbd> | Coal Power Plant | 4x4 | $3,000 |
| <kbd>N</kbd> | Nuclear Power Plant | 4x4 | $5,000 |
| <kbd>A</kbd> | Airport | 6x6 | $10,000 |
| <kbd>Q</kbd> | Query / Inspect Tool | 1x1 | Free |

#### Modals & Windows
| Key | Window / Function |
| :---: | :--- |
| <kbd>U</kbd> | Open City Budget & Tax Rates dialog |
| <kbd>V</kbd> | Open City Evaluation & Public Opinion dialog |
| <kbd>X</kbd> | Open Disasters trigger menu |
| <kbd>G</kbd> | Open Scenario campaign menu |
| <kbd>Tab</kbd> | Cycle Map Overlays (*Normal*, *Power*, *Traffic*, *Pollution*, *Crime*, *Land Value*) |
| <kbd>M</kbd> | Toggle Minimap Radar ON / OFF |
| <kbd>Escape</kbd> | Open In-Game System Menu / Return |

#### Simulation Speed
| Key | Pacing |
| :---: | :--- |
| <kbd>1</kbd> | Pause Simulation |
| <kbd>2</kbd> | Slow Speed |
| <kbd>3</kbd> | Normal Speed |
| <kbd>4</kbd> | Fast Speed |

#### Interactive GUI Elements
- **Bottom Tool Tray**: Click on any tool icon to select it immediately.
- **Minimap Radar**: Click anywhere inside the 60x50 radar to jump the camera directly to that map region.
- **Status Bar**: Click on the Funds display to open the Budget dialog; click on the Overlay text to cycle layers; click on the Speed indicator to toggle simulation speed.

---

### Playdate Handheld Console

| Control | Function |
| :--- | :--- |
| **D-Pad** | Navigate map cursor (features initial delay and fast hold-acceleration; camera smoothly tracks edges) |
| **The Crank** | Rotate clockwise / counter-clockwise to cycle through construction tools |
| **Bumper (L / R)** | Cycle through construction tools |
| **Button (A)** | Build active tool / confirm selection / hold while moving D-pad to drag-build |
| **Button (B)** | Query tile / close active modal dialog / return to game |
| **System Menu Button** | Opens Playdate OS native menu with items: **"City Budget"**, **"Evaluation"**, and **"System Menu"** |

---

## Building & Running

### 1. Desktop (Meson & Ninja)

Requirements: C99 compiler (`gcc` or `clang`), `meson`, `ninja`, and `SDL2` development libraries.

```bash
# Configure the build directory
meson setup build

# Compile the game executable
meson compile -C build

# Run unit tests
meson test -C build

# Launch Micropolis
./build/micropolis
```

### 2. Playdate Simulator

Requirements: [Panic Playdate SDK](https://play.date/dev/) with `PLAYDATE_SDK_PATH` configured.

```bash
# Build the simulator shared library and PDX bundle
make -C build-pd

# Run in Playdate Simulator
$PLAYDATE_SDK_PATH/bin/PlaydateSimulator micropolis.pdx
```

### 3. Playdate Hardware Device (ARM Cortex-M7)

Requirements: `arm-none-eabi-gcc` cross-compiler and Playdate SDK.

```bash
# Build the stripped ARM ELF binary and package the device PDX bundle
make -C build-pd-device

# Install on Playdate over USB
$PLAYDATE_SDK_PATH/bin/pdutil /dev/ttyACM0 install micropolis_DEVICE.pdx
```

---

## Attribution & Credits

- **Will Wright**: Original *SimCity* concept, game design, cellular automata simulation models, and tile graphics.
- **Maxis**: Original game development and commercial release (1989).
- **Don Hopkins & Electronic Arts**: Releasing the SimCity source code as **Micropolis** under the GNU General Public License v3 (GPL v3) for the One Laptop per Child (OLPC) project.
- **vtcity Project**: Developing the Unix baseline that decoupled simulation routines from legacy X11/Tk libraries.
- **Tiny Engine**: High-performance 2D engine framework and multi-platform abstraction layer.

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3) to align with the original upstream release of Micropolis. See the `LICENSE` file for full terms and conditions.
