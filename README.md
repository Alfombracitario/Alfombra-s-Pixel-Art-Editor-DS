<img width="1485" height="384" alt="banner" src="https://github.com/user-attachments/assets/0de200cb-ea3a-4f0a-9268-c7748b70b55e" />

A pixel art editor developed specifically for the **Nintendo DS**, inspired by YY-CHR.  
Designed and tested to run on **original Nintendo DS hardware**. Emulator compatibility is **not guaranteed**, and emulator-specific issues are not a priority for this project.

*Tested on DSi. DS testers welcome.*

*If you still want to use an emulator, use MelonDS!*
---

## Features

- Pixel art editor optimized for NDS hardware
- Multiple color depth support (2bpp to direct color)
- Retro console–oriented export formats
- Custom compact image format (`.acs`)
- Undo / Redo support
- Semi-transparent grid, zoom and scroll tools
- 4 brush types and 4 brush sizes
- Shift canvas in all directions (with wrap-around)
- Palette editor with per-channel precision editing and copy/paste
- Color bucket — replace a palette index, an entire color across the image or just a zone
- File browser with preview and smooth scrolling
- Animation support *(experimental — see below)*
- A few easter eggs hidden somewhere in the app

---

## Controls

### General

| Button | Action |
|--------|--------|
| **A** | Zoom in |
| **B** | Zoom out |
| **Y** / **R** | Toggle grid |
| **START** | Return to bitmap mode and **confirm changes** |
| **SELECT** | Return to bitmap mode and **cancel changes** |

### Navigation

| Button | Action |
|--------|--------|
| **L** / **X** + D-Pad | Scroll canvas |
| **D-Pad** | Change active palette entry |

### Palette Editor

| Button | Action |
|--------|--------|
| **SELECT** + D-Pad | Edit palette color with per-channel precision |
---

## Supported Formats

### Image Formats

| Format | Notes |
|--------|-------|
| ACS | Custom format (see below) |
| PNG | 8bpp, direct color |
| BMP | 4bpp, 8bpp, 24bpp |
| PCX | 8bpp |

### Console / Retro Formats

| Platform | Color Depth |
|----------|-------------|
| NES | 2bpp |
| Game Boy / GBC | 2bpp |
| GBA | 4bpp |
| SNES | 4bpp, 8bpp |

### Palette Formats

| Format | Notes |
|--------|-------|
| YY-CHR `.pal` | Import / Export |
| ARGB 1555 `.pal` | Import / Export |

---

## What is `.acs`?

**Alfombra Compression System (ACS)** is a custom image format designed to store pixel art efficiently, with fast read times suited for constrained hardware like the Nintendo DS.

It supports multiple color depths and compression commands tailored for pixel art patterns.

---

## Animation *(Experimental)*

A-Pix DS includes basic animation support: add frames, delete frames, change playback speed, and more.

<img width="260" height="35" alt="animation bar" src="https://github.com/user-attachments/assets/721bb411-0aa3-49f5-b847-38cdc4d5fdfb" />

Each frame must currently be exported individually — `.gif` export is not yet available.  
The app may behave unexpectedly in some animation scenarios.

---

## Current Limitations

- Maximum image size: **128×128 pixels**
- Animation export is frame-by-frame only (no GIF)

---

<img width="256" height="384" alt="githubiconapixds" src="https://github.com/user-attachments/assets/7670c65a-591c-4be7-95f6-85ec90b98b3d" />


---

**Discord:** https://discord.gg/smJcPKfBBW
