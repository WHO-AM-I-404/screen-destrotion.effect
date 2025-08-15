# Distortion Desktop Effect

A dynamic real-time distortion effect that manipulates the Windows desktop image around the mouse cursor, creating a **ripple-like warp** animation.

---

## &#x20;Description

**Distortion Desktop Effect** is a small Windows application written in C that applies a circular distortion effect to your desktop. It repeatedly captures the screen, then uses mathematical transformations to shift pixels in concentric patterns around the cursor, producing a real-time visual ripple/wave effect.

---

## &#x20;How It Works (Technical Overview)

1. **Screen Capture**

   * Uses WinAPI's `GetDC()` and `BitBlt()` to capture the entire virtual screen.
   * Stores the captured image in a memory device context (DC) using a compatible bitmap.

2. **Cursor Tracking**

   * Retrieves the mouse cursor position with `GetCursorPos()`.
   * Adjusts coordinates to account for multi-monitor setups.

3. **Distortion Calculation**

   * Loops through angles from `0` to `2π` using a fixed iteration step (`ITERATIONS`).
   * For each angle, calculates a normalized direction vector (`sin`, `cos`).
   * Moves outward from the cursor in small steps (`DISTANCE`), repeating for `ITERATIONSM` segments.

4. **Pixel Manipulation**

   * For each step, pixels are copied from a source offset to a target offset along the current direction.
   * This creates a shifted, concentric distortion pattern radiating from the cursor.
   * The distortion area size is defined by `SIZE` (pixel square size).

5. **Rendering**

   * Writes the manipulated image back to the desktop using `BitBlt()`.
   * Updates every 50ms for smooth animation (\~20 FPS).

---

## &#x20;Features

* **Real-Time Distortion:** Animated ripple effect centered on the mouse pointer.
* **Circular Wave Pattern:** Uses trigonometric functions for smooth circular motion.
* **Fast Refresh:** Updates every 50ms for fluid animation.
* **Non-Intrusive:** Runs as an overlay directly modifying the desktop buffer.
* **Portable:** Single executable, no installation required.
* **Optimized:** Uses raw WinAPI calls for speed and efficiency.

---

## &#x20;System Requirements

* **OS:** Windows 7, 8, 10, 11
* **Architecture:** 32-bit or 64-bit
* **RAM:** Minimum 2GB
* **CPU:** Any modern CPU (single-core capable)
* **GPU:** Not required, but hardware acceleration improves performance

---

## &#x20;How to Use

1. Compile the source code:

   ```bash
   gcc distortion.c -o distortion.exe -lgdi32
   ```

   *(Requires MinGW or MSVC on Windows)*

2. Run `distortion.exe`.

3. Move your mouse around to see the distortion ripple follow your cursor.

4. Close the console or terminate the process to stop the effect.

---

## &#x20;Notes

* The effect modifies your screen buffer in real-time. While it is safe and does not alter system files, it may interfere with other screen capture tools.
* CPU usage is minimal on modern systems, but may vary depending on screen resolution.

---

## &#x20;License

This project is licensed under the **MIT License** – free to use, modify, and distribute.

> *"Even the desktop can have waves."* – Distortion Effect Philosophy
