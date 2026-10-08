# DOS Port Specification for Freeciv 1.14.1

## Purpose

Port Freeciv 1.14.1, sourced from the RH9 i386 source RPM, to DOS 6.22 as a playable, offline, single-user graphical game.

Preserve the game rules, AI, and reusable engine logic. Replace platform-dependent GUI, rendering, input, audio, and operating-system interfaces with DOS-compatible implementations.

This document defines the target and technical requirements. Implementation tasks belong in [checklist.txt](./checklist.txt), progress history in [progress.txt](./progress.txt), and the project VM boot procedure in [dos_boot_steps.txt](./dos_boot_steps.txt).

Superseded goals and playability checklists, historical build/link logs, and the old missing-symbol list are preserved in [old/](./old/) for reference only. They are not authoritative requirements or evidence of the current build/runtime state.

The public repository's scope and upstream credit are documented in
[README.md](./README.md). DOS disk/recovery images, raw RPMs and the separately
extracted Linux-package reference tree are local-only and excluded from Git;
historical paths to them do not imply redistribution. The upstream source/data,
licensed runtime supply, port changes and text/image validation records remain
available. A clone requires its own licensed DOS environment and locally built
executables for image-based acceptance.

Development follows one explicitly authorized Phase at a time. Phase closeout
requires verified outcomes, current checklist/cleanup/specification/boot records
and a progress-history entry. Replace the README's current-status section with
the latest verified state; do not accumulate obsolete phase-status summaries.
Commit and push verified changes to the GitHub repository after each Phase,
confirm synchronization, and stop for review before starting another Phase.
Unverified outcomes or blocked publication must be reported explicitly.

## Scope

### Supported experience

- Offline single-player games with AI opponents.
- New-game setup, game loading, saving, turn progression, and game completion.
- Graphical map, readable HUD, menus, dialogs, reports, and gameplay controls.
- Keyboard operation and mouse interaction when a DOS mouse driver is available.
- DOS-native sound and music where practical, with a usable no-audio fallback.

### Intentional exclusions

- Networked multiplayer, external server connections, and server discovery.
- A 16-bit real-mode rewrite of the game.
- Linux/X11/GTK or Win32 GUI dependencies in the DOS executable.
- Porting SDL, ESD, or Linux audio stacks to DOS.
- Mandatory support for 1024x768, every legacy sound device, or decorative animations.

Upstream networking code may remain in the source tree for other targets, but the DOS game must not depend on external sockets or a network server.

## Target environment

| Component | Requirement |
| --- | --- |
| Operating system | DOS 6.22 |
| Minimum CPU | Pentium 100 |
| Minimum RAM | 16 MB |
| Execution model | 32-bit protected mode with a DOS extender |
| Graphics | VESA 2.0+ compatible hardware |
| Minimum graphics mode | 640x480, 16 bits per pixel |
| Preferred graphics mode | 800x600, 16 bits per pixel, when supported |
| Optional graphics mode | 1024x768, 16 bits per pixel, on stronger hardware |

The game must be a high-resolution graphical client, not a text or ASCII console build. The supported map size, AI configuration, and resolution must fit within the minimum memory and CPU profile.

## Execution model and build

DJGPP with a DPMI protected-mode runtime is the preferred toolchain and runtime. A generic DPMI-compatible extender is an alternative; a DOS/4GW-style path is a fallback only if DJGPP is impractical. Supporting all extender families is not required.

DOS hosts a 32-bit flat-memory process. BIOS calls, video memory, keyboard, mouse, and sound access must use the selected extender's supported interfaces and DOS drivers/APIs.

Build requirements:

- Integrate the DOS GUI target through the autotools build system using `--enable-client=dos-vbe`.
- Use a consistent DOS-targeted compiler, linker, archiver, and runtime.
- Produce a reproducible protected-mode DOS executable from source.
- Link real implementations of required interfaces, not ABI placeholders that only satisfy the linker.
- Keep DOS-specific compatibility changes isolated so supported non-DOS builds retain their behavior.
- Document the required DPMI host, DOS memory configuration, runtime files, and build dependencies.

### Reproducible cross-build procedure

`configure.ac` and the `Makefile.am` files are authoritative. `configure.in` is retained only as legacy reference and is not a DOS regeneration input. Use [bootstrap.sh](./freeciv-1.14.1/bootstrap.sh) for regeneration; [autogen.sh](./freeciv-1.14.1/autogen.sh) delegates to it and accepts `NOCONFIGURE=1` to skip configuration. The old `--disable-autoconf2.52` path is explicitly rejected rather than silently discarding DOS support.

Required host tools are Autoconf/Autoheader, Automake/Aclocal, GNU Make, GNU tar, gettext's `config.rpath`, and ordinary shell/file/hash utilities. The reference toolchain uses Autoconf 2.71, Automake 1.16.5, DJGPP GCC 12.2.0, and DJGPP Binutils 2.30. If gettext support files are installed elsewhere, set `GETTEXT_DATADIR` to the directory containing `config.rpath`.

From the project root, with the DJGPP cross tools on `PATH`, run:

```sh
PATH=/path/to/djgpp/bin:$PATH \
sh freeciv-1.14.1/build-dos.sh /absolute/new/freeciv-dos-build
```

The destination must not exist, its parent must exist, and its canonical path must be outside the source tree without whitespace or colons. The configured source checkout may live in a path with spaces: the driver first creates a fresh source snapshot in the destination, strips only generated build artifacts, regenerates build templates there, and builds out of tree. This avoids relying on the checkout's existing configuration, archives, or executable. It preserves platform source headers such as `amiga/config.h`.

The driver selects:

```text
--host=i586-pc-msdosdjgpp --enable-client=dos-vbe --disable-server
--disable-nls --without-readline --without-zlib
--disable-esd --disable-sdl-mixer --disable-winmm
--disable-make-data --disable-cvs-deps
```

`--disable-server` disables the separate server executable, not the embedded engine. Configuration requires a real DJGPP compiler for `dos-vbe`, defines both `FC_NO_SOCKET_API` and `FC_LOCAL_ENGINE`, excludes host GUI/audio probing, and does not advertise nonblocking sockets. Default build flags are `-O2 -g` plus `-Wall -std=gnu89 -fcommon`; GNU89 and common-symbol compatibility retain this upstream version's C/global conventions. `CC`, `AR`, `RANLIB`, `LD`, `NM`, `OBJCOPY`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `JOBS`, and `DJGPP_PREFIX` may be supplied through the environment.

The normal client build recursively builds the real DOS GUI archive and the offline broker/isolated engine. The engine uses [build-engine.sh](./freeciv-1.14.1/client/offline/build-engine.sh), shared with the component harness, for symbol isolation. Client agent timer dependencies resolve to `common/timing.c`, not diagnostic timer substitutes. Generic-interface compile errors are fatal for the DOS backend.

Outputs are under `<destination>/build/`, with the executable at `client/civclient.exe`. The destination also retains bootstrap/configure/make logs, archive membership, executable symbols/identity, tool versions, source-snapshot hashes, and executable/configuration hashes. The driver checks every real DOS backend module is present, the diagnostic `dos_client_stubs.c` is absent from linkage, and engine/AI/real timer symbols are present. Reproducibility means a fresh source build with recorded inputs; debug paths/toolchain timestamps can change binary hashes across destinations.

Build evidence and a reference candidate are retained in [builds/phase2-dos/](./builds/phase2-dos/), alongside a regenerated source-distribution archive. These are build artifacts, not a DOS installation or asset package. Do not use the checkout's older in-place executable/archive as the result of this procedure.

Incomplete GUI implementations must not be exercised as though they were functional. Keep DOS GUI startup gated with an explicit unavailable diagnostic and failure exit before graphics initialization or scripted actions until production resource loading, persistent input/event servicing, and display integration are verified together. A separately validated display or correctly linked candidate is not playable-game acceptance; replace the gate only alongside verified required implementations.

## Offline engine architecture

Freeciv separates its client from the authoritative server engine. The client alone cannot generate a game, run AI, validate actions, or advance turns.

The DOS product uses one executable containing the client and an isolated embedded authoritative engine, connected by in-memory packet delivery. The engine reuses the server, AI, and common sources without requiring external networking.

Architecture requirements:

- Preserve authoritative action validation, AI processing, ruleset loading, map generation, and save/load behavior.
- Maintain clear ownership and lifecycle boundaries between authoritative state and client-visible state.
- Preserve fog of war and the information available to the player.
- Resolve client/server globals, entry points, and mode assumptions rather than simply linking both executables together.
- Preserve ordered requests, responses, and state updates through local packet delivery or an equally complete mapped interface.
- Integrate engine work with the GUI event loop without socket-driven blocking waits or unresponsive input.
- Support initialization, game-over handling, cleanup, and repeated new-game/load operations.

### State isolation and transport contract

- Compile the engine's common/server/AI sources separately from the client's common sources. Exclude the server executable entry point and do not invoke its blocking `srv_main()` loop.
- Combine the engine objects with a relocatable link, then rename every engine-defined external symbol, including its internal references, using the `fc_engine_` namespace. Preserve the target ABI's leading underscore when present. Only the six `fc_offline_engine_*` APIs in `client/offline/engine.h` remain unprefixed.
- Keep engine game/map/ruleset, mode, connection, RNG, and module state distinct from the client-visible cache. Do not exchange player, city, unit, map, or connection pointers across the boundary. CRT services may be shared.
- Compile participating client/common/engine code with consistent `FC_LOCAL_ENGINE` connection layouts and `FC_NO_SOCKET_API`. External connection, lookup, discovery, and autoconnect paths are unsupported.
- Preserve the existing wire encoders/decoders, capability negotiation, authoritative request handlers, request IDs, processing-start/finish notifications, and player-specific visibility filtering.
- The client broker owns separate bounded request/response byte queues. Each queue has `MAX_LEN_BUFFER` capacity; connection receive/send buffers retain the same limit. Copy bytes rather than retaining caller buffers. Queue objects and session objects must be zero-initialized before first use.
- A write callback returns copied bytes, zero for backpressure, or a negative value for failure. It must not synchronously run client handlers or reenter engine processing. Keep pending bytes on backpressure; log failures and terminate the failed session explicitly. Deliver queued rejection replies before reporting engine failure.
- The engine owns authoritative initialization and teardown. The broker owns its queues and the client transport buffers; closing it must not free the client-visible game cache. Release active-turn AI allocations before freeing authoritative players/maps and reset local lifecycle/RNG state for a fresh session.
- Starting a game is explicit and requires an established human connection. Cooperative polling accepts a positive packet budget no larger than `INT_MAX` and advances at most one startup, turn-begin, turn-end, or game-over phase per call. It returns while waiting for player input and does not enter socket sniffing loops.
- A lifecycle phase can still perform substantial map generation or AI work. Packet budgeting is not a wall-clock responsiveness guarantee; minimum-hardware timing and event-loop integration must meet the reliability/performance requirements separately.

### Component verification

Run the isolated engine/transport harness from the project root using an absolute build directory that does not already exist:

```sh
sh freeciv-1.14.1/client/offline/check.sh /absolute/new/native-build
```

The script uses the configured source tree and shipped data, compiles separate client/engine common instances, checks the state-symbol renaming, and runs the native harness outside the source tree. It keeps assertions enabled even if caller flags define `NDEBUG`. Compiler/linker tools and flags can be supplied through `CC`, `LD`, `NM`, `OBJCOPY`, `CFLAGS`, and `LDFLAGS`.

For memory/ownership validation:

```sh
ASAN_OPTIONS='detect_leaks=1:halt_on_error=1' \
CFLAGS='-O0 -g -std=gnu89 -fsanitize=address -fno-omit-frame-pointer' \
LDFLAGS='-fsanitize=address' \
sh freeciv-1.14.1/client/offline/check.sh /absolute/new/asan-build
```

For target compilation, place the DJGPP tools on `PATH` and use:

```sh
CC=i586-pc-msdosdjgpp-gcc LD=i586-pc-msdosdjgpp-ld \
NM=i586-pc-msdosdjgpp-nm OBJCOPY=i586-pc-msdosdjgpp-objcopy \
CROSS_COMPILE=1 CFLAGS='-O0 -g -std=gnu89' \
sh freeciv-1.14.1/client/offline/check.sh /absolute/new/dos-build
```

The harness checks joins, packet ordering/fragmentation, bounded polling/buffering, backpressure, rejection/error delivery, reentrancy guards, state isolation, cleanup/restart, and real map/AI initialization, rates changes, and turn advancement. Its packet observer is not the graphical client; decoded ruleset payloads are released rather than installed in a UI cache. Test-only linkage helpers are confined to this diagnostic executable.

These commands verify the architecture component, not the production autotools linkage, graphical event loop, save/load UI, full-game completion, DOS runtime, 16 MB fit, or VESA display. They do not regenerate or replace `client/civclient.exe`. Actual DOS and user-driven gameplay acceptance remain mandatory.

## DOS GUI backend

The DOS backend belongs in `client/gui-dos-vbe` and must satisfy the generic Freeciv GUI contracts, including:

- `client/include/gui_main_g.h`
- `client/include/graphics_g.h`
- `client/include/mapview_g.h`
- The related generic dialog, menu, report, and control interfaces.

`client/gui-stub` may be used as an interface reference, not as a functional implementation.

The GUI shell must implement `ui_init()`, `ui_main(int argc, char *argv[])`, `sound_bell()`, unit-icon hooks, and the remaining required generic entry points with their correct signatures. Network-input hooks must be safely handled for the offline target without introducing an external socket dependency.

The main loop must persist until the user exits and service real input, local engine updates, timers, audio, and display presentation. Required operations must not silently disappear behind no-op callbacks.

## Display and rendering

### VESA interface

- Query the real VBE controller and supported modes through BIOS services.
- Validate controller version, BIOS return codes, mode attributes, dimensions, depth, pitch, and color masks.
- Select supported 16-bit modes using verified BIOS metadata rather than assuming a mode number implies a pixel format.
- Set graphics mode and provide actual access to video memory through the protected-mode runtime.
- Use a mapped linear framebuffer where available. Otherwise provide tested banked access or document the required hardware capability and fail explicitly.
- Fall back to another supported graphical mode when possible. A RAM-only buffer is not a successful display fallback.
- Restore the original display state and release video/runtime resources on exit or failed initialization.

#### Display implementation contract

- The supplied backend requires VESA 2.0+ with a usable RGB565 linear framebuffer and BIOS `4F04h` state size/save/restore support. Banked-only, non-RGB565, or incomplete-state-service adapters fail explicitly without entering an invisible software-only display. These are declared hardware restrictions, not emulated support.
- `vbe_init_display()` prefers a validated 800x600x16 mode and falls back to validated 640x480x16. `vbe_set_mode()` accepts the 640x480, 800x600, and optional 1024x768 profiles, tries advertised matching modes, and on allocation/mapping/BIOS failure tries other validated candidates and lower profiles. Optional 1024x768 is not a minimum-hardware certification.
- Standard 16-bit candidate constants are `0x111`, `0x114`, and `0x117`; no selection depends on those constants alone. Controller signature/version/memory, the bounded real-mode mode list, mode attributes/planes/depth/model, pitch, masks and physical address must be checked. Reject RGB555 and other layouts rather than feeding them RGB565 renderer data.
- Controller and mode wire buffers are exactly 512 and 256 bytes. Decode fixed-width little-endian fields from byte arrays, with compile-time size checks, rather than casting BIOS memory to an unchecked C structure. Use VBE 3 linear pitch/mask fields for VBE 3 controllers, and VBE 2 fields otherwise.
- `vbe_hw.c` owns actual DJGPP/DPMI services. Reserve a managed linear block using `0501h`, map device pages through the `0508h` extension, and access it through a bounded LDT selector. Release the selector and block (`0502h`) on teardown. CWSDPMI r7 supports `0508h` but not `0801h`; do not silently ignore an unsupported physical-unmap call or depend on process exit to hide repeated-display leaks. Hosts lacking the required extension fail explicitly.
- The historically named `dos_vbe_front_buffer` is an offscreen RGB565 allocation with the BIOS pitch. `dos_vbe_present()` copies completed dirty updates into mapped video memory; no backbuffer allocation alone constitutes display readiness. Initialization verifies mapped readback, and presentation/readback reject inconsistent dimensions, pitch, allocation size or format.
- Save the original mode and BIOS hardware/BDA/DAC/VBE state before mode changes. Restoration must explicitly select the original mode before restoring the state block; a successful state call alone does not guarantee an adapter left LFB graphics mode. Preserve legacy text video pages when returning to a DOS text session. Original graphical framebuffer contents are not backed up.
- Make display initialization/shutdown idempotent. Register exit cleanup, unwind failed attempts before fallback, release conventional BIOS buffers and mappings, and retain failed-release/restoration handles for an explicit retry rather than losing ownership. Report cleanup failures.
- Keep `tests/vbe_check.c` / `VBECHECK.EXE` as a separate real-hardware diagnostic; it displays RGB bars/checkerboard, reads back the full pitch-by-height mapping, waits for Q, and restores DOS text mode. It does not implement game graphics, fonts, GUI input or gameplay.

From the source directory, run the host behavioral suites with:

```sh
sh client/gui-dos-vbe/vbe_check.sh
sh client/gui-dos-vbe/dpmi_check.sh
sh client/gui-dos-vbe/framebuffer_check.sh
sh client/gui-dos-vbe/mapview_check.sh
```

They use a host C compiler and ASan/UBSan to exercise BIOS metadata, presentation,
fallback and failure lifecycles, and actual mapper code with fake DPMI services.
The rendering suites exercise actual primitives/colors/mapview with pitched
buffers, allocation failures, extreme signed coordinates, clipping, overlap,
dirty behavior, and fake map/control/display services. These and the
source-presence smoke check are wired into the backend's `make check`.
Host simulations do not replace the DOS visible-display procedure in
[dos_boot_steps.txt](dos_boot_steps.txt).

Build the separate target diagnostic into a nonexistent absolute directory:

```sh
CC=i586-pc-msdosdjgpp-gcc \
sh client/gui-dos-vbe/build-vbe-check.sh /tmp/freeciv-vbe-new
```

The normal production build links real VBE modules but keeps its readiness gate.

### Software renderer

- Draw into an offscreen backbuffer and present updates to video memory.
- Honor the selected mode's pitch and pixel layout; use RGB565 only where the reported masks support it.
- Use clipped drawing primitives, checked buffer sizes, and explicit allocation-error handling.
- Use dirty regions and coalesced updates to reduce CPU cost and flicker.
- Render readable bitmap text for the HUD, menus, dialogs, and reports.
- Manage graphics/font/icon resources and their ownership consistently with the generic client interfaces.

Rendering primitives must operate on validated, allocated RGB565 buffers, clip
signed coordinates before computing byte offsets, and leave row padding intact.
Invalid dimensions/formats or allocation failures must report an error without
discarding the previous usable buffer. Fully offscreen drawing is a legitimate
clipped no-op, not failed initialization.

Track changed pixels in a coalesced dirty region. Synchronize the complete
pitch-sized mapping once when a display is created; afterward copy only dirty
visible row spans and clear dirty state after successful presentation. Repeated
presentation without new changes must perform no video writes. Keep drawing and
presentation distinct so batched `write_to_screen=FALSE` updates remain offscreen
until an explicit flush.

Coalescing uses one bounding rectangle, not an exact sparse list: unchanged
pixels between separate changes may be copied, but unaffected rows/padding are
not included. `dos_vbe_last_present_bytes()` reports actual bytes written by the
last presentation, including zero for a clean buffer. Drawing final composed
tile pixels avoids erase/redraw dirtiness when terrain and overlays are unchanged.

Map updates use the requested world-tile rectangle and clip copied damage to the
viewport, including label footprints that cross tile boundaries. Compositors
may rebuild a bounded scratch viewport to preserve shared layer ordering, but
must not clear unrelated visible map pixels or copy intermediate HUD erasures.
They must not reset
selection during redraw, or interpret negative dimensions as unsigned loop
bounds. Unit/city/selection overlays must use the same viewport-relative
coordinates as terrain. HUD drawing must remain clipped even if the drawing
buffer is narrower or shorter than the normal HUD layout. Rendering primitives
and a readable bitmap font do not by themselves implement real tileset resources
or a complete game HUD.

`framebuffer.h` provides checked/clipped fill, line, blit, icon and text
operations. Source/destination clipping stays aligned; self-blits have snapshot
semantics, including keyed transparency. Buffers own their storage and must not
be shallow-copied or have allocation metadata edited by callers.

`bitmap_font.c` contains an original glyph resource authored for this DOS VBE
port, incorporating no third-party font data, distributed under GNU GPL version
2 or any later version. Its printable-ASCII 5x7 glyphs occupy 6x8 cells; scale 2
provides readable 10x14 glyphs in 12x16 cells at both required resolutions.
Newlines advance rows, and unsupported bytes use a question-mark glyph. Static
font storage needs no heap lifecycle. This does not resolve tileset X11 font
names, localization beyond ASCII, or implement the remaining dialog/report UI.

Build the separate primitive/font/dirty-presentation diagnostic with:

```sh
CC=i586-pc-msdosdjgpp-gcc \
sh client/gui-dos-vbe/build-render-check.sh /tmp/freeciv-render-new
```

`RNDCHECK.EXE` accepts `640` or `800`, renders the deterministic test pattern,
checks mapped readback and bounded/zero-byte dirty updates, waits for Q, and
restores DOS text mode. It is not the production GUI. Map/HUD composition uses independent offscreen map/UI storage; compare final
pixels during front-buffer blits so unchanged repeats remain clean. Destroy
that storage with `dos_vbe_mapview_free()` before display teardown when ending
a session. Display reinitialization/resizing must safely replace owned storage.

A simplified tile renderer and icon cache are acceptable. A full upstream sprite pipeline is not mandatory, but required resource-loading contracts must remain functional.

### Selected tileset resource contract

The supported initial tileset is overhead Trident, using the shared client
tilespec/tag cache and `fill_tile_sprite_array()` compositor. Isometric drawing
is not supported by this backend. The XPM3 loader accepts one-/two-character
pixel codes, required X11/hex palette forms and transparency; unsupported XPM2,
extensions and malformed data fail explicitly. Sprites own RGB565 pixels and
separate opacity, so transparent pixels cannot collide with a visible color.
Crops have independent lifetime; origins must be inside the atlas, with
right/bottom overhang padded transparently for upstream explosion rectangles.
Tilespec, not the backend loader, owns cached tags and alias reference counts.

`tools/stage_resources.py` creates a separate flat DOS 8.3 package. It rewrites
tilespec/spec references into `.TSP`/`.SPC` names, preserves original XPM bytes
and notices, and records aliases/provenance in `RESMAP.TXT`/`PROVEN.TXT`.
Do not rename or rewrite the source or extracted RPM reference assets in place.
DOS tileset discovery is case-insensitive and accepts `.TSP`; manifest lookup
must validate safe aliases, malformed mappings and duplicates. Host tests must
also resolve the actual uppercase `.XPM` package, without renamed substitutes.

The separate `build-map-check.sh` links `MAPCHECK.EXE` against a completed
production build while using an explicitly initialized actual common/client
state fixture. `tests/stage_phase6_fixture.py` copies four byte-identical default
rulesets into fixture-only `.RUL` aliases. These inputs supply selected terrain,
unit, government and research fields; they are not a complete production ruleset
package or evidence of a joined playable session. Required acceptance includes
real DOS visible output, readback, clean repeated presentation, Q/text
restoration and unchanged authoritative state during rendering.

### Map and HUD

The map display must provide:

- Terrain, relevant terrain improvements/resources, and visibility/fog-of-war distinctions.
- Distinguishable units, ownership, stacks, focus, and relevant unit status.
- City markers, ownership, names, sizes, and relevant city state.
- Selection and action-target overlays, including routes and city-worker displays.
- Correct scrolling, centering, map wrapping, and map-to-screen coordinate conversion.
- An overview/minimap with refresh and viewport indication.
- Updates driven by authoritative game-state changes.

The HUD must show readable turn/year, player, economy, research, government, selected-tile, unit, and city information. Simplified visuals must still convey the information needed to play the supported ruleset.

Viewport/selection helpers must normalize longitude, clamp polar centering and
preserve selection and unit focus independently of scrolling/redraw. Overview
sampling must distinguish unknown/explored/visible tiles and show wrapped view
edges and selection. Route overlays retain shared counted-segment semantics.
Movement/combat hooks may show endpoint states without timed interpolation;
client packet handlers, not rendering helpers, own authoritative HP updates.

Map/HUD display may fold supported Latin-1 and UTF-8 accented names to readable
ASCII while keeping stored names unchanged. This is a display fallback, not
full localization or a substitute for later dialog/report encoding/layout.

## Input and gameplay UI

The input layer must:

- Acquire real keyboard input through polling or appropriate DOS interrupt interfaces.
- Distinguish character input from extended keys and support movement, selection, cancellation, end-turn, and menu/dialog navigation.
- Keep map scrolling and cursor movement distinct from unit orders.
- Detect and use a DOS mouse driver when available, with correct screen-to-tile/UI hit testing.
- Provide keyboard access to required operations when no mouse driver is installed.
- Route actions through client controls to the local authoritative engine, with ownership and game-state checks.
- Handle queued input and key repeat predictably without silently dropping important commands.

The graphical UI must expose the supported ruleset's required operations:

- New-game setup, nation/player selection, AI configuration, load, save, and quit.
- Unit movement, selection, activities, founding cities, transport, combat, and special-unit actions.
- City management, worker/specialist assignment, production, worklists, and purchasing.
- Research, economic rates, government changes, and relevant reports.
- AI diplomacy, treaties, and player/intelligence information.
- Applicable victory controls and game-over information, including space-race controls when supported.
- Notifications, message history, errors, help, options, and confirmations.

External multiplayer chat is not required, but game messages and command feedback must remain visible in the graphical UI.

## Sound and music

Preserve sound and music capability as much as practical while keeping device handling separate from game logic and the GUI renderer.

Use a DOS-native plugin matching the contract in `client/audio.c`. Preferred device approaches are Sound Blaster-compatible audio, AdLib/FM synthesis, General MIDI, or an appropriate DOS mixer/device interface. Implementing every device family is not required.

The audio layer must:

- Detect and initialize supported hardware honestly.
- Support the chosen packaged sound/music formats and playback controls.
- Handle music repetition, stop/wait, volume where supported, and shutdown.
- Manage device, interrupt, and DMA resources safely where applicable.
- Report unavailable hardware or playback failures rather than claiming silent playback succeeded.
- Fall back cleanly to no audio without preventing gameplay.

Audio must not be a prerequisite for a playable game on hardware without a compatible sound device.

## Filesystem and packaging

### Asset source and reference layout

The supplied binary package, [freeciv-1.14.1-2.RH9.0.i386.rpm](./freeciv-1.14.1-2.RH9.0.i386.rpm), is preserved unchanged. Its metadata identifies Freeciv 1.14.1, release 2, i386, with source package `freeciv-1.14.1-2.src.rpm` and license label GPL. Its SHA-256 is:

```text
f78a86adbc888e7599b320df8772d72641529370570df7b3ab3fea51465b77e5
```

The complete payload is extracted without installation or execution into [assets/freeciv-1.14.1-rh9/](./assets/freeciv-1.14.1-rh9/), preserving its original `etc/` and `usr/` layout. Treat this as an unchanged upstream reference, not a DOS installation directory or build output. It contains 196 regular files and one relative client symlink; the regular files total 15,506,344 bytes. Extracted file contents were verified against the decompressed RPM payload. This is content verification, not RPM signature authentication.

The game-data root is [usr/share/freeciv/](./assets/freeciv-1.14.1-rh9/usr/share/freeciv/) within that extraction:

| Resource | Supplied contents |
| --- | --- |
| Graphics | 21 XPM images, 20 sprite specification files, and three tileset specifications: Trident (30x30 tiles), Trident with shields (30x30), and MacroIsoTrident (64x32 isometric tiles) |
| Rules and nations | 90 `.ruleset` files across `default/`, `civ1/`, `civ2/`, and `nation/` |
| Scenarios | Six `.sav` scenario files and two `.serv` setup scripts |
| Help/configuration | `helpdata.txt`, `freeciv.rc`, and `freeciv.rc-2.0` |
| Audio | `stdsounds.soundspec` and 15 PCM WAV files in `stdsounds/`, totaling 1,425,666 audio-file bytes |

These 161 runtime-data files total 5,959,034 bytes on disk; this is not a runtime-memory estimate. Comparison with [freeciv-1.14.1/data/](./freeciv-1.14.1/data/) found 145 byte-identical files, no differing files, and 16 package-only files: the sound specification and its 15 WAV files. The source distribution already includes the packaged game graphics, rules, scenarios, and help; no asset overlay or source-data replacement is needed merely to obtain those resources.

The remaining package contents are Linux clients/server and launcher scripts, desktop/X11 configuration and an application icon, and 19 translation catalogs. Linux ELF executables and launcher scripts are reference material only; do not execute them as DOS tools or ship them as the port's executable/runtime. The extracted data path may be supplied explicitly for host-side resource inspection, but must not become a hard-coded DOS path.

### Asset compatibility requirements

- All three supplied tilesets' referenced sprite specs, XPM images, and intro/radar images are present. All 48 active sound-tag mappings resolve to the 15 supplied WAV files. Presence/reference checks do not establish rendering or playback.
- Preserve graphics tags, atlas grids, transparency, and reference ordering when implementing a DOS loader or converting assets. Supplied XPM images use both one- and two-character pixel codes and up to 256 colors; most have transparent pixels. Images are atlases, not already-cropped DOS sprites.
- Tilesets request X11 fonts such as `9x15bold` and `6x13`, but the package contains no standalone font files. Supply a licensed DOS bitmap-font resource or equivalent renderer; font-name strings do not provide font data.
- The WAVs are uncompressed PCM at 11,025, 22,050, or 44,100 Hz, with 8- or 16-bit samples. Fourteen are mono and one is stereo. Implement supported decoding/mixing or an explicitly documented conversion strategy for the selected DOS device.
- `music_start` maps to `stdsounds/amb18.wav`; the package supplies sampled intro music, not a MIDI/FM music bank. Many optional event mappings are commented out; do not claim a sound for every event.
- Plain DOS 8.3 compatibility is unresolved: 125 of the 161 runtime-data filenames are not literal 8.3 names, and `isotrident/` and `stdsounds/` are long directory names. Preserve the reference extraction and perform any later packaging conversions in a separate DOS staging tree, rewriting and validating every affected reference or providing a tested LFN solution. No case-insensitive full-path collisions were found in the supplied runtime-data tree.
- Preserve artists/attribution entries in the graphics and sound specifications and the corresponding source/license documentation. Verify applicable asset redistribution/conversion terms before release rather than assuming the RPM's overall license label supplies all attribution details.

### DOS distribution requirements

- Keep the working DOS volume organized: boot/system and AUTOEXEC/CONFIG files in `C:\`, DOS utilities/support files in `C:\DOS`, and game executable/resources in `C:\FREECIV`. Use DOS-safe executable names such as `FREECIV.EXE`; keep diagnostic demos separate from normal startup.
- For the project dual-drive setup, boot `DOS622.img` as A: and attach the MBR-partitioned `dos-vm/DOS622_VHD.img` as C:. The active type-06 FAT16 partition starts at LBA 63 (byte offset 32256), has 524288 sectors, and uses BIOS geometry 521/16/63 without translation. Host mtools operations must include `@@32256`; an unpartitioned FAT16 image is not a substitute for a DOS-visible hard disk.
- Keep floppy-to-C: startup conditional on an accessible C: system volume, stop at a clean prompt without launching demos, and report an unavailable working drive explicitly. The supplied partition wrapper adds a partition table, not standalone MBR boot code; hard-drive-only boot is not supported by this setup.
- Package the executable, required DPMI/runtime files, rulesets, nation data, graphics/fonts, help, and configured audio resources.
- Resolve data paths correctly regardless of the launch working directory.
- Support DOS drive letters, path separators, case-insensitive lookup, and writable save/config/log locations.
- Make resource filenames and internal references compatible with DOS 6.22 FAT/8.3 names, or explicitly package and validate an LFN solution.
- Avoid dependencies on host installation paths, Unix home-directory layouts, or Linux file-permission assumptions.
- Handle missing/corrupt resources, unreadable saves, and disk-full/write failures explicitly.
- Use a text encoding and font strategy that keeps shipped labels, names, and user input readable.
- Include installation, controls, supported hardware/limits, troubleshooting, license, and corresponding-source/build information for distribution.

### DPMI runtime and diagnostic contract

- Use unchanged CWSDPMI r7 for the DJGPP runtime configuration documented in [dos_boot_steps.txt](dos_boot_steps.txt). Place `CWSDPMI.EXE` and its supplier documentation alongside `C:\FREECIV\FREECIV.EXE`; auto-loading must work independently of the current working directory.
- The baseline boots A: with its existing `COUNTRY=001` configuration, without HIMEM/EMM386. C: configuration is not loaded by calling its AUTOEXEC. Do not require untested memory-manager combinations.
- Use `CWSDPMI -s-` immediately before each application when verifying the 16 MB physical-memory requirement. This one-process host unloads when the application exits. If paging is intentionally enabled, explicitly select an application-local swap file with `-sC:\FREECIV\CWSDPMI.SWP`; auto-load defaults may otherwise use the root. Record paging mode with memory-fit evidence.
- Preserve the original binary/source archives and notices in `runtime/cwsdpmi-r7/`. Users have the right to receive source/binary updates; source is included as `csdpmi7s.zip` and available from [the DJGPP distributor](https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7s.zip). Redistribution must follow the supplied CWSDPMI terms.
- Keep `client/offline/runtime_check.c` and `build-runtime-check.sh` separate from normal GUI linkage. This diagnostic uses real DOS/DPMI services and isolated-engine join/reset tests; `RTCHECK.EXE` must identify its limited scope rather than report production GUI readiness.
- The diagnostic's executable-relative `DATA\RUNTIME.DAT` is an explicitly labeled test fixture, not packaged rulesets, graphics, or fonts. Its path tests do not satisfy actual game-resource acceptance.
- Diagnostic initialization must log failures, release acquired resources, and return nonzero without entering its keyboard wait on failure. Successful interactive operation waits for Q and returns to DOS. Preserve the production GUI failure gate until actual required video/resources/input implementations are verified.
- Preserve executable/build provenance and actual DOS logs separately from the normal candidate. Runtime component success does not certify video mapping, production startup, full gameplay, or the complete game's 16 MB fit.

## Reliability and performance

The supported configuration must run within 16 MB, accounting for the engine, client state, AI, resources, buffers, and DOS/runtime overhead.

Measure responsiveness for map scrolling/redraw, input, AI turns, and save/load on the target profile. Optimize resource caching and update presentation where needed without changing game rules.

Failures must produce actionable diagnostics. Initialization and cleanup must handle absent devices, unsupported video modes, insufficient memory, and file errors without leaving an invisible client, a hung graphics session, or leaked resources.

## Acceptance criteria

The port meets this specification when:

1. A clean, documented build produces a functional 32-bit DOS executable.
2. The packaged game starts under DOS 6.22 with the documented DPMI/runtime configuration.
3. A real VESA display shows a readable map, HUD, and usable controls at the minimum mode.
4. An offline game with AI can be created or loaded without an external network server.
5. Required unit, city, research, economy, government, diplomacy, and victory actions work through authoritative game logic.
6. Keyboard-only play works, and mouse interaction works with a supported driver.
7. Multiple turns, saving, exit/relaunch, loading, and game completion work without critical defects.
8. The supported minimum-hardware configuration fits within 16 MB and has measured usable responsiveness.
9. Audio functions on declared supported hardware, with an honest no-audio fallback elsewhere.
10. The game restores the DOS display and releases runtime/device resources on exit.

Validation must exercise the actual DOS executable and user-driven gameplay. Booting DOS, linking placeholders, allocating a software buffer, or passing source-presence checks alone does not demonstrate compliance.
