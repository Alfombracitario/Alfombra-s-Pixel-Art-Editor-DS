# A-Pix DS
<img width="75" height="75" alt="apixdsicon" src="https://github.com/user-attachments/assets/42b970ba-c9fe-478c-bf4c-873eacf6d411" />

This is a pixel art editor developed specifically for the **Nintendo DS**.


The application is designed and tested to run on **original Nintendo DS hardware**. Emulator compatibility is **not guaranteed**, and emulator-specific issues are not a priority for this project.

This document describes the editor controls and the custom image format used by the application.

---

## 1. Controls

### General Controls
- **A** — Zoom in  
- **B** — Zoom out  
- **Y** or **L** — Enable / disable grid  
- **START** — Return to bitmap mode and **confirm changes**  
- **SELECT** — Return to bitmap mode and **cancel changes**

### Navigation
- **R** or **X** + **D-Pad** — Move canvas  
- **D-Pad** — Change active palette entry  

---

## 2. ACS File Format

**`.acs` (Alfombra Compression System)** is a custom image format created to efficiently compress pixel art and allow **very fast loading times** on Nintendo DS hardware (and any other retro hardware!)

This format is currently **experimental**. While it is optimized for performance and memory usage, it may still contain edge cases or bugs. Use with caution, especially for important or production assets.

---

## 3. Why is .gif not supported?

.gif is not supported yet because this version is just a prototype and adding it would take a lot of time.

<img width="256" height="384" alt="A-PixDS screenshot" src="https://github.com/user-attachments/assets/67042a94-b95d-416d-bcdb-9fb9232637dc" />
