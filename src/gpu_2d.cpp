/*
    Copyright 2019-2025 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstring>

#include "gpu_2d.h"
#include "core.h"
#include "settings.h"

Gpu2D::Gpu2D(Core *core, bool engine): core(core), engine(engine) {
    // Set up 2D GPU engine A or B
    bgVramAddr = 0x6000000 + engine * 0x200000;
    objVramAddr = 0x6400000 + engine * 0x200000;
    palette = core->memory.palette + engine * 0x400;
    oam = core->memory.oam + engine * 0x400;
    extPalettes = engine ? core->memory.engBExtPal : core->memory.engAExtPal;
}

void Gpu2D::saveState(FILE *file) {
    // Write state data to the file
    fwrite(winHFlip, sizeof(bool), sizeof(winHFlip) / sizeof(bool), file);
    fwrite(winVFlag, sizeof(bool), sizeof(winVFlag) / sizeof(bool), file);
    fwrite(&dispCnt, sizeof(dispCnt), 1, file);
    fwrite(bgCnt, 2, sizeof(bgCnt) / 2, file);
    fwrite(bgHOfs, 2, sizeof(bgHOfs) / 2, file);
    fwrite(bgVOfs, 2, sizeof(bgVOfs) / 2, file);
    fwrite(bgPA, 2, sizeof(bgPA) / 2, file);
    fwrite(bgPB, 2, sizeof(bgPB) / 2, file);
    fwrite(bgPC, 2, sizeof(bgPC) / 2, file);
    fwrite(bgPD, 2, sizeof(bgPD) / 2, file);
    fwrite(bgX, 4, sizeof(bgX) / 4, file);
    fwrite(bgY, 4, sizeof(bgY) / 4, file);
    fwrite(winX1, 2, sizeof(winX1) / 2, file);
    fwrite(winX2, 2, sizeof(winX2) / 2, file);
    fwrite(winY1, 2, sizeof(winY1) / 2, file);
    fwrite(winY2, 2, sizeof(winY2) / 2, file);
    fwrite(&winIn, sizeof(winIn), 1, file);
    fwrite(&winOut, sizeof(winOut), 1, file);
    fwrite(&bldCnt, sizeof(bldCnt), 1, file);
    fwrite(&mosaic, sizeof(mosaic), 1, file);
    fwrite(&bldAlpha, sizeof(bldAlpha), 1, file);
    fwrite(&bldY, sizeof(bldY), 1, file);
    fwrite(&masterBright, sizeof(masterBright), 1, file);
}

void Gpu2D::loadState(FILE *file) {
    // Read state data from the file
    fread(winHFlip, sizeof(bool), sizeof(winHFlip) / sizeof(bool), file);
    fread(winVFlag, sizeof(bool), sizeof(winVFlag) / sizeof(bool), file);
    fread(&dispCnt, sizeof(dispCnt), 1, file);
    fread(bgCnt, 2, sizeof(bgCnt) / 2, file);
    fread(bgHOfs, 2, sizeof(bgHOfs) / 2, file);
    fread(bgVOfs, 2, sizeof(bgVOfs) / 2, file);
    fread(bgPA, 2, sizeof(bgPA) / 2, file);
    fread(bgPB, 2, sizeof(bgPB) / 2, file);
    fread(bgPC, 2, sizeof(bgPC) / 2, file);
    fread(bgPD, 2, sizeof(bgPD) / 2, file);
    fread(bgX, 4, sizeof(bgX) / 4, file);
    fread(bgY, 4, sizeof(bgY) / 4, file);
    fread(winX1, 2, sizeof(winX1) / 2, file);
    fread(winX2, 2, sizeof(winX2) / 2, file);
    fread(winY1, 2, sizeof(winY1) / 2, file);
    fread(winY2, 2, sizeof(winY2) / 2, file);
    fread(&winIn, sizeof(winIn), 1, file);
    fread(&winOut, sizeof(winOut), 1, file);
    fread(&bldCnt, sizeof(bldCnt), 1, file);
    fread(&mosaic, sizeof(mosaic), 1, file);
    fread(&bldAlpha, sizeof(bldAlpha), 1, file);
    fread(&bldY, sizeof(bldY), 1, file);
    fread(&masterBright, sizeof(masterBright), 1, file);
}

uint32_t Gpu2D::rgb5ToRgb6(uint32_t color) {
    // Convert an RGB5 value to an RGB6 value (the way the 2D engine does it)
    // Also keep the extra bits because some of them are used to keep track of stuff
    uint8_t r = ((color << 1) & 0x3E);
    uint8_t g = ((color >> 4) & 0x3E);
    uint8_t b = ((color >> 9) & 0x3E);
    return (color & 0xFFFC0000) | (b << 12) | (g << 6) | r;
}

void Gpu2D::reloadRegisters() {
    // Reload internal registers at the start of a frame
    internalX[0] = bgX[0];
    internalX[1] = bgX[1];
    internalY[0] = bgY[0];
    internalY[1] = bgY[1];
}

void Gpu2D::updateWindows(int line) {
    // Enable or disable vertical window areas at the set scanlines
    for (int i = 0; i < 2; i++) {
        if (line == winY2[i])
            winVFlag[i] = false;
        else if (line == winY1[i])
            winVFlag[i] = true;
    }
}

void Gpu2D::drawGbaScanline(int line) {
    // Clear layers with the backdrop (first palette index)
    uint32_t backdrop = U8TO16(palette, 0) & ~BIT(15);
    for (int i = 0; i < 240; i++) layers[0][i] = backdrop;
    memcpy(layers[1], layers[0], 240 * sizeof(uint32_t));
    memset(priorities, 4, sizeof(priorities));
    memset(blendBits, 5, sizeof(blendBits));

    // Draw the objects, object window first if enabled
    if (dispCnt & BIT(12)) {
        if (dispCnt & BIT(15)) drawObjects<1>(line, true);
        drawObjects<1>(line, false);
    }

    // Draw the background layers depending on the BG mode
    switch (uint8_t mode = (dispCnt & 0x7)) {
        case 0:
            if (dispCnt & BIT(11)) drawText<1>(3, line);
            if (dispCnt & BIT(10)) drawText<1>(2, line);
            if (dispCnt & BIT(9)) drawText<1>(1, line);
            if (dispCnt & BIT(8)) drawText<1>(0, line);
            break;

        case 1:
            if (dispCnt & BIT(10)) drawAffine<1>(2, line);
            if (dispCnt & BIT(9)) drawText<1>(1, line);
            if (dispCnt & BIT(8)) drawText<1>(0, line);
            break;

        case 2:
            if (dispCnt & BIT(11)) drawAffine<1>(3, line);
            if (dispCnt & BIT(10)) drawAffine<1>(2, line);
            break;

        case 3: case 4: case 5:
            if (dispCnt & BIT(10)) drawExtendedGba(2, line);
            break;

        default:
            LOG_CRIT("Unknown GBA BG mode: %d\n", mode);
            break;
    }

    // Blend the layers to form the final image
    for (int mode = (bldCnt >> 6) & 0x3, i = 0; i < 240; i++) {
        // Check if blending can/should be performed
        if (layers[0][i] & BIT(25)) { // Semi-transparent pixel
            // Force alpha blending if possible, otherwise allow brightness blending or do nothing
            if (bldCnt & BIT(8 + blendBits[1][i])) // Below pixel is blendable
                goto alpha;
            else if (mode < 2 || !(bldCnt & BIT(blendBits[0][i])))
                continue;
        }
        else if (mode == 0 || !(bldCnt & BIT(blendBits[0][i])) || (mode == 1 && !(bldCnt & BIT(8 + blendBits[1][i])))) {
            // Do nothing if blending is disabled or not possible
            continue;
        }

        // Skip blending if the pixel is in the bounds of a window that has it disabled
        if (dispCnt & 0xE000) { // Windows enabled
            uint8_t enabled;
            if ((dispCnt & BIT(13)) && winVFlag[0] && (i >= winX1[0] && i < winX2[0]) != winHFlip[0])
                enabled = winIn >> 0; // Window 0
            else if ((dispCnt & BIT(14)) && winVFlag[1] && (i >= winX1[1] && i < winX2[1]) != winHFlip[1])
                enabled = winIn >> 8; // Window 1
            else if ((dispCnt & BIT(15)) && (framebuffer[line * 256 + i] & BIT(24)))
                enabled = winOut >> 8; // Object window
            else
                enabled = winOut >> 0; // Outside of windows
            if (!(enabled & BIT(5))) continue;
        }

        // Apply blending, using 15-bit colors in GBA mode
        switch (mode) {
            case 1: // Alpha blending
            alpha: {
                uint8_t eva = std::min((bldAlpha >> 0) & 0x1F, 16);
                uint8_t evb = std::min((bldAlpha >> 8) & 0x1F, 16);
                uint8_t r = std::min((((layers[0][i] >> 0) & 0x1F) * eva + ((layers[1][i] >> 0) & 0x1F) * evb) / 16, 31U);
                uint8_t g = std::min((((layers[0][i] >> 5) & 0x1F) * eva + ((layers[1][i] >> 5) & 0x1F) * evb) / 16, 31U);
                uint8_t b = std::min((((layers[0][i] >> 10) & 0x1F) * eva + ((layers[1][i] >> 10) & 0x1F) * evb) / 16, 31U);
                layers[0][i] = (b << 10) | (g << 5) | r;
                continue;
            }

            case 2: // Brightness increase
                if (bldY) {
                    uint8_t r = (layers[0][i] >> 0) & 0x1F; r += (31 - r) * bldY / 16;
                    uint8_t g = (layers[0][i] >> 5) & 0x1F; g += (31 - g) * bldY / 16;
                    uint8_t b = (layers[0][i] >> 10) & 0x1F; b += (31 - b) * bldY / 16;
                    layers[0][i] = (b << 10) | (g << 5) | r;
                }
                continue;

            case 3: // Brightness decrease
                if (bldY) {
                    uint8_t r = (layers[0][i] >> 0) & 0x1F; r -= r * bldY / 16;
                    uint8_t g = (layers[0][i] >> 5) & 0x1F; g -= g * bldY / 16;
                    uint8_t b = (layers[0][i] >> 10) & 0x1F; b -= b * bldY / 16;
                    layers[0][i] = (b << 10) | (g << 5) | r;
                }
                continue;
        }
    }

    // Copy the final image to the framebuffer
    memcpy(&framebuffer[line * 256], layers[0], 240 * sizeof(uint32_t));
}

void Gpu2D::drawScanline(int line) {
    // Clear layers with the backdrop (first palette index)
    uint32_t backdrop = U8TO16(palette, 0) & ~BIT(15);
    for (int i = 0; i < 256; i++) layers[0][i] = backdrop;
    memcpy(layers[1], layers[0], sizeof(layers[0]));
    memset(priorities, 4, sizeof(priorities));
    memset(blendBits, 5, sizeof(blendBits));

    // Draw the objects, object window first if enabled
    if (dispCnt & BIT(12)) {
        if (dispCnt & BIT(15)) drawObjects<0>(line, true);
        drawObjects<0>(line, false);
    }

    // Draw the background layers depending on the BG mode
    switch (uint8_t mode = (dispCnt & 0x7)) {
        case 0:
            if (dispCnt & BIT(11)) drawText<0>(3, line);
            if (dispCnt & BIT(10)) drawText<0>(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 1:
            if (dispCnt & BIT(11)) drawAffine<0>(3, line);
            if (dispCnt & BIT(10)) drawText<0>(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 2:
            if (dispCnt & BIT(11)) drawAffine<0>(3, line);
            if (dispCnt & BIT(10)) drawAffine<0>(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 3:
            if (dispCnt & BIT(11)) drawExtended(3, line);
            if (dispCnt & BIT(10)) drawText<0>(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 4:
            if (dispCnt & BIT(11)) drawExtended(3, line);
            if (dispCnt & BIT(10)) drawAffine<0>(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 5:
            if (dispCnt & BIT(11)) drawExtended(3, line);
            if (dispCnt & BIT(10)) drawExtended(2, line);
            if (dispCnt & BIT(9)) drawText<0>(1, line);
            if (dispCnt & BIT(8)) drawText<0>(0, line);
            break;

        case 6:
            if (dispCnt & BIT(10)) drawLarge(2, line);
            break;

        default:
            LOG_CRIT("Unknown engine %c BG mode: %d\n", (engine == 0) ? 'A' : 'B', mode);
            break;
    }

    // Blend the layers to form the final image
    for (int mode = (bldCnt >> 6) & 0x3, i = 0; i < 256; i++) {
        // Check if blending can/should be performed
        if (layers[0][i] & BIT(26)) { // 3D pixel
            if (bldCnt & BIT(8 + blendBits[1][i])) { // Below pixel is blendable
                // Override the default blending rules and apply special alpha blending
                // If 3D alpha is max, skip blending; high-res 3D is transposed on these pixels
                uint8_t eva = ((layers[0][i] >> 18) & 0x3F) + 1;
                if (eva == 64) continue;
                uint32_t blend = rgb5ToRgb6(layers[1][i]);
                uint8_t evb = 64 - eva;
                uint8_t r = std::min((((layers[0][i] >> 0) & 0x3F) * eva + ((blend >> 0) & 0x3F) * evb) / 64, 63U);
                uint8_t g = std::min((((layers[0][i] >> 6) & 0x3F) * eva + ((blend >> 6) & 0x3F) * evb) / 64, 63U);
                uint8_t b = std::min((((layers[0][i] >> 12) & 0x3F) * eva + ((blend >> 12) & 0x3F) * evb) / 64, 63U);
                layers[0][i] = (b << 12) | (g << 6) | r;
                continue;
            }
            else if (mode < 2 || !(bldCnt & BIT(blendBits[0][i]))) {
                // Do nothing unless brightness blending is possible as a fallback
                continue;
            }
        }
        else {
            // Convert the pixel to 18-bit since it isn't an 18-bit 3D pixel
            layers[0][i] = rgb5ToRgb6(layers[0][i]);

            if (layers[0][i] & BIT(25)) { // Semi-transparent pixel
                // Force alpha blending if possible, otherwise allow brightness blending or do nothing
                if (bldCnt & BIT(8 + blendBits[1][i])) // Below pixel is blendable
                    goto alpha;
                else if (mode < 2 || !(bldCnt & BIT(blendBits[0][i])))
                    continue;
            }
            else if (mode == 0 || !(bldCnt & BIT(blendBits[0][i])) || (mode == 1 && !(bldCnt & BIT(8 + blendBits[1][i])))) {
                // Do nothing if blending is disabled or not possible
                continue;
            }
        }

        // Skip blending if the pixel is in the bounds of a window that has it disabled
        if (dispCnt & 0xE000) { // Windows enabled
            uint8_t enabled;
            if ((dispCnt & BIT(13)) && winVFlag[0] && (i >= winX1[0] && i < winX2[0]) != winHFlip[0])
                enabled = winIn >> 0; // Window 0
            else if ((dispCnt & BIT(14)) && winVFlag[1] && (i >= winX1[1] && i < winX2[1]) != winHFlip[1])
                enabled = winIn >> 8; // Window 1
            else if ((dispCnt & BIT(15)) && (framebuffer[line * 256 + i] & BIT(24)))
                enabled = winOut >> 8; // Object window
            else
                enabled = winOut >> 0; // Outside of windows
            if (!(enabled & BIT(5))) continue;
        }

        // Apply blending, using 18-bit colors in DS mode
        switch (mode) {
            case 1: // Alpha blending
            alpha: {
                uint32_t blend = (layers[1][i] & BIT(26)) ? layers[1][i] : rgb5ToRgb6(layers[1][i]);
                uint8_t eva = std::min((bldAlpha >> 0) & 0x1F, 16);
                uint8_t evb = std::min((bldAlpha >> 8) & 0x1F, 16);
                uint8_t r = std::min((((layers[0][i] >> 0) & 0x3F) * eva + ((blend >> 0) & 0x3F) * evb) / 16, 63U);
                uint8_t g = std::min((((layers[0][i] >> 6) & 0x3F) * eva + ((blend >> 6) & 0x3F) * evb) / 16, 63U);
                uint8_t b = std::min((((layers[0][i] >> 12) & 0x3F) * eva + ((blend >> 12) & 0x3F) * evb) / 16, 63U);
                layers[0][i] = (b << 12) | (g << 6) | r;
                continue;
            }

            case 2: // Brightness increase
                if (bldY) {
                    uint8_t r = (layers[0][i] >> 0) & 0x3F; r += (63 - r) * bldY / 16;
                    uint8_t g = (layers[0][i] >> 6) & 0x3F; g += (63 - g) * bldY / 16;
                    uint8_t b = (layers[0][i] >> 12) & 0x3F; b += (63 - b) * bldY / 16;
                    layers[0][i] = (b << 12) | (g << 6) | r;
                }
                continue;

            case 3: // Brightness decrease
                if (bldY) {
                    uint8_t r = (layers[0][i] >> 0) & 0x3F; r -= r * bldY / 16;
                    uint8_t g = (layers[0][i] >> 6) & 0x3F; g -= g * bldY / 16;
                    uint8_t b = (layers[0][i] >> 12) & 0x3F; b -= b * bldY / 16;
                    layers[0][i] = (b << 12) | (g << 6) | r;
                }
                continue;
        }
    }

    // Copy the final image to the framebuffer
    switch ((dispCnt >> 16) & 0x3) { // Display mode
        case 0: // Display off
            // Fill the display with white
            memset(&framebuffer[line * 256], 0xFF, 256 * sizeof(uint32_t));
            break;

        case 1: // Layers
            memcpy(&framebuffer[line * 256], layers[0], 256 * sizeof(uint32_t));
            break;

        case 2: // VRAM display
            // Draw raw bitmap data from a VRAM block
            for (uint32_t addr = 0x6800000 + ((dispCnt >> 1) & 0x60000) + line * 256 * 2, i = 0; i < 256; i++)
                framebuffer[line * 256 + i] = rgb5ToRgb6(core->memory.read<uint16_t>(0, addr + i * 2));
            break;

        case 3: // Main memory display
            LOG_CRIT("Unimplemented engine %c display mode: display FIFO\n", (engine == 0) ? 'A' : 'B');
            break;
    }

    // Apply master brightness (DS-only, 18-bit)
    switch ((masterBright >> 14) & 0x3) { // Mode
        case 1: // Increase
            if (uint8_t factor = std::min(masterBright & 0x1F, 16)) {
                for (int i = 0; i < 256; i++) {
                    uint32_t *pixel = &framebuffer[line * 256 + i];
                    uint8_t r = (*pixel >> 0) & 0x3F; r += (63 - r) * factor / 16;
                    uint8_t g = (*pixel >> 6) & 0x3F; g += (63 - g) * factor / 16;
                    uint8_t b = (*pixel >> 12) & 0x3F; b += (63 - b) * factor / 16;
                    *pixel = (b << 12) | (g << 6) | r;
                }
            }
            break;

        case 2: // Decrease
            if (uint8_t factor = std::min(masterBright & 0x1F, 16)) {
                for (int i = 0; i < 256; i++) {
                    uint32_t *pixel = &framebuffer[line * 256 + i];
                    uint8_t r = (*pixel >> 0) & 0x3F; r -= r * factor / 16;
                    uint8_t g = (*pixel >> 6) & 0x3F; g -= g * factor / 16;
                    uint8_t b = (*pixel >> 12) & 0x3F; b -= b * factor / 16;
                    *pixel = (b << 12) | (g << 6) | r;
                }
            }
            break;
    }
}

void Gpu2D::drawBgPixel(int bg, int line, int x, uint32_t pixel) {
    // Skip the pixel if it's in the bounds of a window that has its layer disabled
    if (dispCnt & 0xE000) { // Windows enabled
        uint8_t enabled;
        if ((dispCnt & BIT(13)) && winVFlag[0] && (x >= winX1[0] && x < winX2[0]) != winHFlip[0])
            enabled = winIn >> 0; // Window 0
        else if ((dispCnt & BIT(14)) && winVFlag[1] && (x >= winX1[1] && x < winX2[1]) != winHFlip[1])
            enabled = winIn >> 8; // Window 1
        else if ((dispCnt & BIT(15)) && (framebuffer[line * 256 + x] & BIT(24)))
            enabled = winOut >> 8; // Object window
        else
            enabled = winOut >> 0; // Outside of windows
        if (!(enabled & BIT(bg))) return;
    }

    // Draw the pixel to one of 2 layers, depending on priority, for later blending
    if ((bgCnt[bg] & 0x3) <= priorities[0][x]) { // Higher than topmost
        // Move the topmost pixel to the second topmost
        layers[1][x] = layers[0][x];
        priorities[1][x] = priorities[0][x];
        blendBits[1][x] = blendBits[0][x];

        // Update the topmost pixel
        layers[0][x] = pixel;
        priorities[0][x] = bgCnt[bg] & 0x3;
        blendBits[0][x] = bg;
    }
    else if ((bgCnt[bg] & 0x3) <= priorities[1][x]) { // Higher than second topmost
        // Update the second topmost pixel
        layers[1][x] = pixel;
        priorities[1][x] = bgCnt[bg] & 0x3;
        blendBits[1][x] = bg;
    }
}

void Gpu2D::drawObjPixel(int line, int x, uint32_t pixel, int8_t priority) {
    // Skip the pixel if it's in the bounds of a window that has objects disabled
    if (dispCnt & 0xE000) { // Windows enabled
        uint8_t enabled;
        if ((dispCnt & BIT(13)) && winVFlag[0] && (x >= winX1[0] && x < winX2[0]) != winHFlip[0])
            enabled = winIn >> 0; // Window 0
        else if ((dispCnt & BIT(14)) && winVFlag[1] && (x >= winX1[1] && x < winX2[1]) != winHFlip[1])
            enabled = winIn >> 8; // Window 1
        else if ((dispCnt & BIT(15)) && (framebuffer[line * 256 + x] & BIT(24)))
            enabled = winOut >> 8; // Object window
        else
            enabled = winOut >> 0; // Outside of windows
        if (!(enabled & BIT(4))) return;
    }

    // Draw a pixel to the top layer if the old one is transparent or lower priority
    // Objects are drawn first, and are treated as one layer, so they don't push pixels down
    if (!(layers[0][x] & BIT(15)) || priority < priorities[0][x]) {
        layers[0][x] = pixel;
        priorities[0][x] = priority;
        blendBits[0][x] = 4;
    }
}

template <bool gbaMode> void Gpu2D::drawText(int bg, int line) {
    // If 3D is enabled, override BG0 in text mode
    if (!gbaMode && bg == 0 && (dispCnt & BIT(3))) {
        // In high-res 3D mode, skip every other pixel
        uint32_t *data = core->gpu3DRenderer.getLine(line);
        bool resShift = Settings::highRes3D;

        // Draw a scanline of 3D pixels
        for (int i = 0; i < 256; i++)
            if (data[i << resShift] & 0xFC0000)
                drawBgPixel(bg, line, i, data[i << resShift]);
        return;
    }

    // Get the base data addresses
    uint32_t tileBase = bgVramAddr + ((dispCnt >> 11) & 0x70000) + ((bgCnt[bg] << 3) & 0x0F800);
    uint32_t indexBase = bgVramAddr + ((dispCnt >> 8) & 0x70000) + ((bgCnt[bg] << 12) & 0x3C000);

    // Adjust the Y-coordinate within the background based on vertical mosaic
    int yOffset = (((bgCnt[bg] & BIT(6)) ? (line - (line % (((mosaic >> 4) & 0xF) + 1))) : line) + bgVOfs[bg]) & 0x1FF;
    tileBase += (yOffset & 0xF8) << 3;

    // If the Y-offset exceeds 256 and the background is 512 pixels tall, move to the next 256x256 section
    // If the background is 512 pixels wide, move 2 sections to skip the second X section
    if (yOffset >= 256 && (bgCnt[bg] & BIT(15)))
        tileBase += (bgCnt[bg] & BIT(14)) ? 0x1000 : 0x800;

    // Draw a line
    int width = (gbaMode ? 240 : 256) + (bgHOfs[bg] & 0x7);
    if (bgCnt[bg] & BIT(7)) { // 8-bit
        // Set the extended palette slot if enabled; BG 0/1 can optionally use slot 2/3
        uint8_t *extPal = (dispCnt & BIT(30)) ? extPalettes[bg + (bg < 2 && (bgCnt[bg] & BIT(13))) * 2] : nullptr;
        for (int i = 0; i < width; i += 8) {
            // Move the tile address to the current tile
            int xOffset = (i + bgHOfs[bg]) & 0x1FF;
            uint32_t tileAddr = tileBase + ((xOffset & 0xF8) >> 2);

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if (xOffset >= 256 && (bgCnt[bg] & BIT(14)))
                tileAddr += 0x800;

            // Get the current tile and its palette
            uint16_t tile = core->memory.read<uint16_t>(gbaMode, tileAddr);
            uint8_t *pal = extPal ? &extPal[(tile >> 3) & 0x1E00] : palette;

            // Get the palette indices for the current line of the tile, flipped vertically if enabled
            uint32_t indexAddr = indexBase + (tile & 0x3FF) * 64 + ((tile & BIT(11)) ? (7 - (yOffset & 7)) : (yOffset & 7)) * 8;
            uint64_t indices = core->memory.read<uint32_t>(gbaMode, indexAddr) |
                ((uint64_t)core->memory.read<uint32_t>(gbaMode, indexAddr + 4) << 32);

            // Draw the current line of the tile, flipped horizontally if enabled
            for (int x = i - (xOffset & 7) + ((tile & BIT(10)) ? 7 : 0); indices != 0; ((tile & BIT(10)) ? x-- : x++), indices >>= 8) {
                // Draw a pixel if it's visible
                if (x >= 0 && x < (gbaMode ? 240 : 256) && (indices & 0xFF))
                    drawBgPixel(bg, line, x, U8TO16(pal, (indices & 0xFF) * 2) | BIT(15));
            }
        }
    }
    else { // 4-bit
        for (int i = 0; i < width; i += 8) {
            // Move the tile address to the current tile
            int xOffset = (i + bgHOfs[bg]) & 0x1FF;
            uint32_t tileAddr = tileBase + ((xOffset & 0xF8) >> 2);

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if (xOffset >= 256 && (bgCnt[bg] & BIT(14)))
                tileAddr += 0x800;

            // Get the current tile
            uint16_t tile = core->memory.read<uint16_t>(gbaMode, tileAddr);

            // Get the tile's palette
            // In 4-bit mode, the tile can select from multiple 16-color palettes
            uint8_t *pal = &palette[(tile & 0xF000) >> 7];

            // Get the palette indices for the current line of the tile, flipped vertically if enabled
            uint32_t indexAddr = indexBase + (tile & 0x3FF) * 32 + ((tile & BIT(11)) ? (7 - (yOffset & 7)) : (yOffset & 7)) * 4;
            uint32_t indices = core->memory.read<uint32_t>(gbaMode, indexAddr);

            // Draw the current line of the tile, flipped horizontally if enabled
            for (int x = i - (xOffset & 7) + ((tile & BIT(10)) ? 7 : 0); indices != 0; ((tile & BIT(10)) ? x-- : x++), indices >>= 4) {
                // Draw a pixel if it's visible
                if (x >= 0 && x < (gbaMode ? 240 : 256) && (indices & 0xF))
                    drawBgPixel(bg, line, x, U8TO16(pal, (indices & 0xF) * 2) | BIT(15));
            }
        }
    }
}

template <bool gbaMode> void Gpu2D::drawAffine(int bg, int line) {
    // Calculate the base data addresses
    uint32_t tileBase = bgVramAddr + ((bgCnt[bg] << 3) & 0x0F800) + ((dispCnt >> 11) & 0x70000);
    uint32_t indexBase = bgVramAddr + ((bgCnt[bg] << 12) & 0x3C000) + ((dispCnt >> 8) & 0x70000);

    // Set the initial rotscale coordinates
    int rotscaleX = internalX[bg - 2] - bgPA[bg - 2];
    int rotscaleY = internalY[bg - 2] - bgPC[bg - 2];

    // Get the background size
    int size = 128 << ((bgCnt[bg] & 0xC000) >> 14);

    // Draw a line
    for (int i = 0; i < (gbaMode ? 240 : 256); i++) {
        // Increment the rotscaled coordinates and remove the fraction
        int x = (rotscaleX += bgPA[bg - 2]) >> 8;
        int y = (rotscaleY += bgPC[bg - 2]) >> 8;

        // Handle display area overflow
        if (bg < 2 || (bgCnt[bg] & BIT(13))) { // Wraparound
            x &= size - 1;
            y &= size - 1;
        }
        else if (x < 0 || x >= size || y < 0 || y >= size) { // Transparent
            continue;
        }

        // Read the current tile
        uint32_t tileAddr = tileBase + (y / 8) * (size / 8) + (x / 8);
        uint8_t tile = core->memory.read<uint8_t>(gbaMode, tileAddr);

        // Read the palette index for the current pixel of the tile
        uint32_t indexAddr = indexBase + tile * 64 + (y & 7) * 8 + (x & 7);
        uint8_t index = core->memory.read<uint8_t>(gbaMode, indexAddr);

        // Draw a pixel if it isn't transparent
        if (index)
            drawBgPixel(bg, line, i, U8TO16(palette, index * 2) | BIT(15));
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawExtended(int bg, int line) {
    // Set the initial rotscale coordinates
    int rotscaleX = internalX[bg - 2] - bgPA[bg - 2];
    int rotscaleY = internalY[bg - 2] - bgPC[bg - 2];

    if (bgCnt[bg] & BIT(7)) { // Bitmap
        // Calculate the base data address
        uint32_t dataBase = bgVramAddr + ((bgCnt[bg] << 6) & 0x7C000);

        // Get the bitmap size
        static const uint16_t sizes[] = { 128, 128, 256, 256, 512, 256, 512, 512 };
        const uint16_t *size = &sizes[((bgCnt[bg] >> 13) & 0x6)];
        const uint16_t sizeX = size[0];
        const uint16_t sizeY = size[1];

        if (bgCnt[bg] & BIT(2)) { // Direct color bitmap
            // Draw a line
            for (int i = 0; i < 256; i++) {
                // Increment the rotscaled coordinates and remove the fraction
                int x = (rotscaleX += bgPA[bg - 2]) >> 8;
                int y = (rotscaleY += bgPC[bg - 2]) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) { // Wraparound
                    x &= sizeX - 1;
                    y &= sizeY - 1;
                }
                else if (x < 0 || x >= sizeX || y < 0 || y >= sizeY) { // Transparent
                    continue;
                }

                // Draw a pixel if it isn't transparent
                uint16_t pixel = core->memory.read<uint16_t>(0, dataBase + (y * sizeX + x) * 2);
                if (pixel & BIT(15))
                    drawBgPixel(bg, line, i, pixel);
            }
        }
        else { // 256 color bitmap
            // Draw a line
            for (int i = 0; i < 256; i++) {
                // Increment the rotscaled coordinates and remove the fraction
                int x = (rotscaleX += bgPA[bg - 2]) >> 8;
                int y = (rotscaleY += bgPC[bg - 2]) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) { // Wraparound
                    x &= sizeX - 1;
                    y &= sizeY - 1;
                }
                else if (x < 0 || x >= sizeX || y < 0 || y >= sizeY) { // Transparent
                    continue;
                }

                // Read the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(0, dataBase + y * sizeX + x);

                // Draw a pixel if it isn't transparent
                if (index)
                    drawBgPixel(bg, line, i, U8TO16(palette, index * 2) | BIT(15));
            }
        }
    }
    else { // Extended affine
        // Calculate the base data addresses
        uint32_t tileBase = bgVramAddr + ((bgCnt[bg] << 3) & 0x0F800) + ((dispCnt >> 11) & 0x70000);
        uint32_t indexBase = bgVramAddr + ((bgCnt[bg] << 12) & 0x3C000) + ((dispCnt >> 8) & 0x70000);

        // Get the bitmap size
        size_t size = 128 << ((bgCnt[bg] >> 14) & 0x3);

        // Use the standard palette by default
        uint8_t *pal = palette;

        // Draw a line of the layer
        for (int i = 0; i < 256; i++) {
            // Increment the rotscaled coordinates and remove the fraction
            int x = (rotscaleX += bgPA[bg - 2]) >> 8;
            int y = (rotscaleY += bgPC[bg - 2]) >> 8;

            // Handle display area overflow
            if (bgCnt[bg] & BIT(13)) { // Wraparound
                x &= size - 1;
                y &= size - 1;
            }
            else if (x < 0 || x >= size || y < 0 || y >= size) { // Transparent
                continue;
            }

            // Read the current tile
            uint32_t tileAddr = tileBase + ((y / 8) * (size / 8) + (x / 8)) * 2;
            uint16_t tile = core->memory.read<uint16_t>(0, tileAddr);

            // Switch to an extended palette selected by the tile if enabled
            if (dispCnt & BIT(30)) {
                if (!extPalettes[bg]) continue;
                pal = &extPalettes[bg][(tile >> 3) & 0x1E00];
            }

            // Read the palette index for the current pixel
            uint32_t indexAddr = indexBase + (tile & 0x3FF) * 64 + // Tile offset
                (((tile & BIT(11)) ? (7 - y) : y) & 7) * 8 + // Vertical offset, flipped if enabled
                (((tile & BIT(10)) ? (7 - x) : x) & 7); // Horizontal offset, flipped if enabled
            uint8_t index = core->memory.read<uint8_t>(0, indexAddr);

            // Draw the pixel if it isn't transparent
            if (index)
                drawBgPixel(bg, line, i, U8TO16(pal, index * 2) | BIT(15));
        }
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawExtendedGba(int bg, int line) {
    // Calculate the base data address
    // Modes 4 and 5 have two bitmaps that can be selected with DISPCNT bit 4
    uint8_t mode = dispCnt & 0x7;
    uint32_t dataBase = bgVramAddr + ((bgCnt[bg] << 3) & 0xF800) +
        ((mode > 3 && (dispCnt & BIT(4))) ? 0xA000 : 0);

    // Set the initial rotscale coordinates
    int rotscaleX = internalX[bg - 2] - bgPA[bg - 2];
    int rotscaleY = internalY[bg - 2] - bgPC[bg - 2];

    if (mode == 4) { // 256 color bitmap
        // Draw a line of the layer
        for (int i = 0; i < 240; i++) {
            // Increment the rotscaled coordinates and remove the fraction
            int x = (rotscaleX += bgPA[bg - 2]) >> 8;
            int y = (rotscaleY += bgPC[bg - 2]) >> 8;

            // Don't draw anything on display area overflow
            if (x < 0 || x >= 240 || y < 0 || y >= 160)
                continue;

            // Draw a pixel if it has a non-transparent palette index
            if (uint8_t index = core->memory.read<uint8_t>(1, dataBase + y * 240 + x))
                drawBgPixel(bg, line, i, U8TO16(palette, index * 2) | BIT(15));
        }
    }
    else { // Direct color bitmap
        // Get the bitmap size
        uint8_t sizeX = (mode == 5) ? 160 : 240;
        uint8_t sizeY = (mode == 5) ? 128 : 160;

        // Draw a line of the layer
        for (int i = 0; i < 240; i++) {
            // Increment the rotscaled coordinates and remove the fraction
            int x = (rotscaleX += bgPA[bg - 2]) >> 8;
            int y = (rotscaleY += bgPC[bg - 2]) >> 8;

            // Don't draw anything on display area overflow
            if (x < 0 || x >= sizeX || y < 0 || y >= sizeY)
                continue;

            // Draw a pixel, ignoring transparency
            uint16_t pixel = core->memory.read<uint16_t>(1, dataBase + (y * sizeX + x) * 2);
            drawBgPixel(bg, line, i, pixel | BIT(15));
        }
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawLarge(int bg, int line) {
    // Set the initial rotscale coordinates
    int rotscaleX = internalX[bg - 2] - bgPA[bg - 2];
    int rotscaleY = internalY[bg - 2] - bgPC[bg - 2];

    // Get the bitmap size
    int sizeX = ((bgCnt[bg] >> 14) & 0x3) ? 1024 : 512;
    int sizeY = ((bgCnt[bg] >> 14) & 0x3) ? 512 : 1024;

    // Draw a line of the layer
    for (int i = 0; i < 256; i++) {
        // Increment the rotscaled coordinates and remove the fraction
        int x = (rotscaleX += bgPA[bg - 2]) >> 8;
        int y = (rotscaleY += bgPC[bg - 2]) >> 8;

        // Handle display area overflow
        if (bgCnt[bg] & BIT(13)) { // Wraparound
            x &= sizeX - 1;
            y &= sizeY - 1;
        }
        else if (x < 0 || x >= sizeX || y < 0 || y >= sizeY) { // Transparent
            continue;
        }

        // A full large bitmap requires 512KB of VRAM, but engine B can only use 128KB
        // For engine B, wrap the 128KB bitmap 4 times to cover the full area
        if (engine == 1)
            y &= (sizeY / 4) - 1;

        // Read the palette index for the current pixel
        uint8_t index = core->memory.read<uint8_t>(0, bgVramAddr + y * sizeX + x);

        // Draw a pixel if it isn't transparent
        if (index)
            drawBgPixel(bg, line, i, U8TO16(palette, index * 2) | BIT(15));
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

template <bool gbaMode> void Gpu2D::drawObjects(int line, bool window) {
    // Loop through and draw the 128 sprites in OAM
    for (int i = 0; i < 128; i++) {
        // Skip objects that are disabled or are/aren't window type
        uint8_t byte = oam[i * 8 + 1];
        uint8_t type = (byte >> 2) & 0x3;
        if ((byte & 0x3) == 2 || (type == 2) != window)
            continue;

        // Get an object's 6 attribute bytes (the unused 2 bytes are for rotscale)
        uint16_t object[3] = { U8TO16(oam, i * 8 + 0), U8TO16(oam, i * 8 + 2), U8TO16(oam, i * 8 + 4) };

        // Get the object's dimensions based on its shape and size
        static const uint8_t sizes[] = {
            8, 8, 16, 16, 32, 32, 64, 64, // Square
            16, 8, 32, 8, 32, 16, 64, 32, // Horizontal
            8, 16, 8, 32, 16, 32, 32, 64, // Vertical
            0, 0, 0, 0, 0, 0, 0, 0 // Prohibited
        };
        const uint8_t *size = &sizes[((object[0] >> 11) & 0x18) | ((object[1] >> 13) & 0x6)];
        const uint8_t width = size[0];
        const uint8_t height = size[1];

        // Double the object bounds for rotscale objects with the double size bit set
        bool shift = ((object[0] & 0x300) == 0x300);
        uint8_t width2 = width << shift;
        uint8_t height2 = height << shift;

        // Get the Y coordinate and wrap it around if it exceeds the screen bounds
        int y = object[0] & 0xFF;
        if (y >= 192) y -= 256;

        // Adjust the Y-coordinate within the sprite based on vertical mosaic
        int spriteY = ((object[0] & BIT(12)) ? (line - (line % (((mosaic >> 12) & 0xF) + 1))) : line) - y;
        if (spriteY < 0 || spriteY >= height2)
            continue;

        // Get the X coordinate and wrap it around if it exceeds the screen bounds
        int x = object[1] & 0x1FF;
        if (x >= 256) x -= 512;

        // Objects are higher priority than background layers, so the priority is given a little boost
        int8_t priority = ((object[2] >> 10) & 0x3) - 1;

        // Draw bitmap objects
        if (!gbaMode && type == 3) {
            uint32_t dataBase;
            int bitmapWidth;

            // Determine the address and width of the bitmap
            if (dispCnt & BIT(6)) { // 1D mapping
                dataBase = objVramAddr + (object[2] & 0x3FF) * ((dispCnt & BIT(22)) ? 256 : 128);
                bitmapWidth = width;
            }
            else { // 2D mapping
                uint8_t xMask = (dispCnt & BIT(5)) ? 0x1F : 0x0F;
                dataBase = objVramAddr + (object[2] & xMask) * 0x10 + (object[2] & 0x3FF & ~xMask) * 0x80;
                bitmapWidth = (dispCnt & BIT(5)) ? 256 : 128;
            }

            if (object[0] & BIT(8)) { // Rotscale
                // Get the rotscale parameters
                int16_t params[4] = {
                    (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x06),
                    (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x0E),
                    (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x16),
                    (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x1E)
                };

                // Draw a line of the object
                for (int j = 0; j < width2; j++) {
                    int offset = x + j;
                    if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Draw a pixel
                    uint16_t pixel = core->memory.read<uint16_t>(0, dataBase + (rotscaleY * bitmapWidth + rotscaleX) * 2);
                    if (pixel & BIT(15))
                        drawObjPixel(line, offset, pixel, priority);
                }
            }
            else {
                // Adjust the bitmap offset based on whether or not the sprite is vertically flipped
                dataBase += ((object[1] & BIT(13)) ? (height - spriteY - 1) : spriteY) * bitmapWidth * 2;

                // Draw a line of the object
                for (int j = 0; j < width; j++) {
                    // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                    int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);
                    if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                    // Draw a pixel
                    uint16_t pixel = core->memory.read<uint16_t>(0, dataBase + j * 2);
                    if (pixel & BIT(15))
                        drawObjPixel(line, offset, pixel, priority);
                }
            }
            continue;
        }

        // Get the base tile address
        uint32_t tileBase;
        if (gbaMode) {
            tileBase = 0x6010000 + (object[2] & 0x3FF) * 32;
        }
        else {
            // On the DS, the boundary between tiles can be 32, 64, 128, or 256 bytes for 1D tile mapping
            uint16_t bound = (dispCnt & BIT(4)) ? (32 << ((dispCnt >> 20) & 0x3)) : 32;
            tileBase = objVramAddr + (object[2] & 0x3FF) * bound;
        }

        if (object[0] & BIT(8)) { // Rotscale
            // Get the rotscale parameters
            int16_t params[4] = {
                (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x06),
                (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x0E),
                (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x16),
                (int16_t)U8TO16(oam, ((object[1] >> 9) & 0x1F) * 0x20 + 0x1E)
            };

            if (object[0] & BIT(13)) { // 8-bit
                int mapWidth = (dispCnt & BIT(gbaMode ? 6 : 4)) ? width : 128;

                // Get the object's palette
                uint8_t *pal;
                if (dispCnt & BIT(31)) { // Extended palette
                    // In extended palette mode, the object can select from multiple 256-color palettes
                    if (!extPalettes[4]) continue;
                    pal = &extPalettes[4][(object[2] & 0xF000) >> 3];
                }
                else { // Standard palette
                    pal = &palette[0x200];
                }

                // Draw a line of the object
                for (int j = 0; j < width2; j++) {
                    int offset = x + j;
                    if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the palette index for the current pixel
                    uint8_t index = core->memory.read<uint8_t>(gbaMode, tileBase +
                        ((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 8 + (rotscaleX / 8) * 64 + rotscaleX % 8);

                    if (index && type == 2) { // Object window
                        // Mark object window pixels with an extra bit, and don't draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else {
                        // Draw a pixel, marking semi-transparent pixels with an extra bit
                        if (index)
                            drawObjPixel(line, offset, ((type == 1) << 25) | BIT(15) | U8TO16(pal, index * 2), priority);

                        // Update the priority even if the pixel is transparent
                        // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                        if (gbaMode && priority < priorities[0][offset] && blendBits[0][offset] == 4)
                            priorities[0][offset] = priority;
                    }
                }
            }
            else { // 4-bit
                int mapWidth = (dispCnt & BIT(gbaMode ? 6 : 4)) ? width : 256;

                // Get the object's palette
                // In 4-bit mode, the object can select from multiple 16-color palettes
                uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

                // Draw a line of the object
                for (int j = 0; j < width2; j++) {
                    int offset = x + j;
                    if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the palette index for the current pixel
                    uint8_t index = core->memory.read<uint8_t>(gbaMode, tileBase +
                        ((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 4 + (rotscaleX / 8) * 32 + (rotscaleX % 8) / 2);
                    index = (rotscaleX & 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                    if (index && type == 2) { // Object window
                        // Mark object window pixels with an extra bit, and don't draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else {
                        // Draw a pixel, marking semi-transparent pixels with an extra bit
                        if (index)
                            drawObjPixel(line, offset, ((type == 1) << 25) | BIT(15) | U8TO16(pal, index * 2), priority);

                        // Update the priority even if the pixel is transparent
                        // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                        if (gbaMode && priority < priorities[0][offset] && blendBits[0][offset] == 4)
                            priorities[0][offset] = priority;
                    }
                }
            }
        }
        else if (object[0] & BIT(13)) { // 8-bit
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(gbaMode ? 6 : 4)) ? width : 128;
            if (object[1] & BIT(13)) // Vertical flip
                tileBase += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 8;
            else
                tileBase += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 8;

            // Get the object's palette
            uint8_t *pal;
            if (dispCnt & BIT(31)) { // Extended palette
                // In extended palette mode, the object can select from multiple 256-color palettes
                if (!extPalettes[4]) continue;
                pal = &extPalettes[4][(object[2] & 0xF000) >> 3];
            }
            else { // Standard palette
                pal = &palette[0x200];
            }

            // Draw a line of the object
            for (int j = 0; j < width; j++) {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);
                if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                // Get the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(gbaMode, tileBase + (j / 8) * 64 + j % 8);

                if (index && type == 2) { // Object window
                    // Mark object window pixels with an extra bit, and don't draw anything
                    framebuffer[line * 256 + offset] |= BIT(24);
                }
                else {
                    // Draw a pixel, marking semi-transparent pixels with an extra bit
                    if (index)
                        drawObjPixel(line, offset, ((type == 1) << 25) | BIT(15) | U8TO16(pal, index * 2), priority);

                    // Update the priority even if the pixel is transparent
                    // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                    if (gbaMode && priority < priorities[0][offset] && blendBits[0][offset] == 4)
                        priorities[0][offset] = priority;
                }
            }
        }
        else { // 4-bit
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(gbaMode ? 6 : 4)) ? width : 256;
            if (object[1] & BIT(13)) // Vertical flip
                tileBase += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 4;
            else
                tileBase += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 4;

            // Get the palette of the object
            // In 4-bit mode, the object can select from multiple 16-color palettes
            uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

            // Draw a line of the object
            for (int j = 0; j < width; j++) {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);
                if (offset < 0 || offset >= (gbaMode ? 240 : 256)) continue;

                // Get the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(gbaMode, tileBase + (j / 8) * 32 + (j % 8) / 2);
                index = (j & 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                if (index && type == 2) { // Object window
                    // Mark object window pixels with an extra bit, and don't draw anything
                    framebuffer[line * 256 + offset] |= BIT(24);
                }
                else {
                    // Draw a pixel, marking semi-transparent pixels with an extra bit
                    if (index)
                        drawObjPixel(line, offset, ((type == 1) << 25) | BIT(15) | U8TO16(pal, index * 2), priority);

                    // Update the priority even if the pixel is transparent
                    // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                    if (gbaMode && priority < priorities[0][offset] && blendBits[0][offset] == 4)
                        priorities[0][offset] = priority;
                }
            }
        }
    }
}

void Gpu2D::writeDispCnt(uint32_t mask, uint32_t value) {
    // Write to the DISPCNT register
    mask &= ((engine == 0) ? 0xFFFFFFFF : 0xC0B1FFF7);
    dispCnt = (dispCnt & ~mask) | (value & mask);
    if (core->gbaMode) dispCnt &= 0xFFFF;
}

void Gpu2D::writeBgCnt(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGCNT registers
    if (core->gbaMode && bg < 2) mask &= 0xDFFF;
    bgCnt[bg] = (bgCnt[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgHOfs(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGHOFS registers
    mask &= 0x01FF;
    bgHOfs[bg] = (bgHOfs[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgVOfs(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGVOFS registers
    mask &= 0x01FF;
    bgVOfs[bg] = (bgVOfs[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPA(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGPA registers
    bgPA[bg - 2] = (bgPA[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPB(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGPB registers
    bgPB[bg - 2] = (bgPB[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPC(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGPC registers
    bgPC[bg - 2] = (bgPC[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPD(int bg, uint16_t mask, uint16_t value) {
    // Write to one of the BGPD registers
    bgPD[bg - 2] = (bgPD[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgX(int bg, uint32_t mask, uint32_t value) {
    // Write to one of the BGX registers
    mask &= 0x0FFFFFFF;
    bgX[bg - 2] = (bgX[bg - 2] & ~mask) | (value & mask);

    // Extend the sign to 32 bits
    if (bgX[bg - 2] & BIT(27)) bgX[bg - 2] |= 0xF0000000; else bgX[bg - 2] &= ~0xF0000000;

    // Reload the internal register
    internalX[bg - 2] = bgX[bg - 2];
}

void Gpu2D::writeBgY(int bg, uint32_t mask, uint32_t value) {
    // Write to one of the BGY registers
    mask &= 0x0FFFFFFF;
    bgY[bg - 2] = (bgY[bg - 2] & ~mask) | (value & mask);

    // Extend the sign to 32 bits
    if (bgY[bg - 2] & BIT(27)) bgY[bg - 2] |= 0xF0000000; else bgY[bg - 2] &= ~0xF0000000;

    // Reload the internal register
    internalY[bg - 2] = bgY[bg - 2];
}


void Gpu2D::writeWinH(int win, uint16_t mask, uint16_t value) {
    // Write to one of the WINH registers
    if (mask & 0x00FF) winX2[win] = (value & 0x00FF) >> 0;
    if (mask & 0xFF00) winX1[win] = (value & 0xFF00) >> 8;

    // Invert the window if X1 exceeds X2
    if (winHFlip[win] = (winX1[win] > winX2[win]))
        SWAP(winX1[win], winX2[win]);
}

void Gpu2D::writeWinV(int win, uint16_t mask, uint16_t value) {
    // Write to one of the WINV registers
    if (mask & 0x00FF) winY2[win] = (value & 0x00FF) >> 0;
    if (mask & 0xFF00) winY1[win] = (value & 0xFF00) >> 8;
}

void Gpu2D::writeWinIn(uint16_t mask, uint16_t value) {
    // Write to the WININ register
    mask &= 0x3F3F;
    winIn = (winIn & ~mask) | (value & mask);
}

void Gpu2D::writeWinOut(uint16_t mask, uint16_t value) {
    // Write to the WINOUT register
    mask &= 0x3F3F;
    winOut = (winOut & ~mask) | (value & mask);
}

void Gpu2D::writeMosaic(uint16_t mask, uint16_t value) {
    // Write to the MOSAIC register
    mosaic = (mosaic & ~mask) | (value & mask);
}

void Gpu2D::writeBldCnt(uint16_t mask, uint16_t value) {
    // Write to the BLDCNT register
    mask &= 0x3FFF;
    bldCnt = (bldCnt & ~mask) | (value & mask);
}

void Gpu2D::writeBldAlpha(uint16_t mask, uint16_t value) {
    // Write to the BLDALPHA register
    mask &= 0x1F1F;
    bldAlpha = (bldAlpha & ~mask) | (value & mask);
}

void Gpu2D::writeBldY(uint8_t value) {
    // Write to the BLDY register
    bldY = value & 0x1F;
    if (bldY > 16) bldY = 16;
}

void Gpu2D::writeMasterBright(uint16_t mask, uint16_t value) {
    // Write to the MASTER_BRIGHT register
    mask &= 0xC01F;
    masterBright = (masterBright & ~mask) | (value & mask);
}
