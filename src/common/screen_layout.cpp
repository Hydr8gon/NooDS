/*
    Copyright 2019-2021 Hydr8gon

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

#include "screen_layout.h"
#include "../settings.h"

int ScreenLayout::screenRotation    = 0;
int ScreenLayout::screenArrangement = 0;
int ScreenLayout::screenSizing      = 0;
int ScreenLayout::screenGap         = 0;
int ScreenLayout::integerScale      = 0;
int ScreenLayout::gbaCrop           = 1;

void ScreenLayout::addSettings()
{
    // Define the layout settings
    std::vector<Setting> layoutSettings =
    {
        Setting("screenRotation",    &screenRotation,    false),
        Setting("screenArrangement", &screenArrangement, false),
        Setting("screenSizing",      &screenSizing,      false),
        Setting("screenGap",         &screenGap,         false),
        Setting("integerScale",      &integerScale,      false),
        Setting("gbaCrop",           &gbaCrop,           false)
    };

    // Add the layout settings
    Settings::add(layoutSettings);
}

void ScreenLayout::update(int winWidth, int winHeight, bool gbaMode)
{
    if (gbaMode && gbaCrop)
    {
        // Determine the screen dimensions based on the current rotation
        int width  = (screenRotation ? 160 : 240);
        int height = (screenRotation ? 240 : 160);

        // Set the minimum dimensions for the layout
        minWidth  = width;
        minHeight = height;
        if (winWidth  < minWidth)  winWidth  = minWidth;
        if (winHeight < minHeight) winHeight = minHeight;

        // Scale the screen to the size of the window
        float baseRatio = (float)width / height;
        float screenRatio = (float)winWidth / winHeight;
        float scale = ((baseRatio > screenRatio) ? ((float)winWidth / width) : ((float)winHeight / height));
        if (integerScale) scale = (int)scale;

        // Calculate the dimensions and position of the screen
        topWidth  = scale * width;
        topHeight = scale * height;
        topX = (winWidth - topWidth) / 2;
        topY = (winHeight - topHeight) / 2;
    }
    else // NDS mode
    {
        // Determine the screen arrangement based on the current settings
        // In automatic mode, the arrangement is horizontal if rotated and vertical otherwise
        bool vertical = (screenArrangement == 1 || (screenArrangement == 0 && screenRotation == 0));

        // Determine the screen dimensions based on the current rotation
        int width  = (screenRotation ? 192 : 256);
        int height = (screenRotation ? 256 : 192);

        // Determine the screen gap based on the current setting
        int gap = (screenGap ? (12 * (1 << screenGap)) : 0);
        if (gap > 96) gap = 96;

        float largeScale, smallScale;

        // Calculate the scale of each screen
        // When calculating scale, if the window is wider than the screen, the screen is scaled to the height of the window
        // If the window is taller than the screen, the screen is scaled to the width of the window
        // If gap is enabled, each screen is given half of the gap as extra weight for scaling
        // This results in a gap that is scaled with the screens, and averages if the screens are different scales
        if (vertical)
        {
            // Add the extra gap weight if enabled
            if (screenGap) height += gap / 2;

            // Set the minimum dimensions for the layout
            minWidth  = width;
            minHeight = height * 2;
            if (winWidth  < minWidth)  winWidth  = minWidth;
            if (winHeight < minHeight) winHeight = minHeight;

            if (screenSizing == 0) // Even
            {
                // Scale both screens to the size of the window
                float baseRatio = (float)width / (height * 2);
                float screenRatio = (float)winWidth / winHeight;
                largeScale = ((baseRatio > screenRatio) ? ((float)winWidth / width) : ((float)winHeight / (height * 2)));
                if (integerScale) largeScale = (int)largeScale;
                smallScale = largeScale;
            }
            else // Enlarge Top/Bottom
            {
                float baseRatio = (float)width / height;

                // Scale the large screen to the size of the window minus room for the smaller screen
                float largeRatio = (float)winWidth / (winHeight - height);
                largeScale = ((baseRatio > largeRatio) ? ((float)winWidth / width) : ((float)(winHeight - height) / height));
                if (integerScale) largeScale = (int)largeScale;

                // Scale the small screen to the remaining window space
                float smallRatio = (float)winWidth / (winHeight - largeScale * height);
                smallScale = ((baseRatio > smallRatio) ? ((float)winWidth / width) : ((float)(winHeight - largeScale * height) / height));
                if (integerScale) smallScale = (int)smallScale;
            }

            // Remove the extra gap weight for the next calculations
            if (screenGap) height -= gap / 2;
        }
        else // Horizontal
        {
            // Add the extra gap weight if enabled
            if (screenGap) width += gap / 2;

            // Set the minimum dimensions for the layout
            minWidth  = width * 2;
            minHeight = height;
            if (winWidth  < minWidth)  winWidth  = minWidth;
            if (winHeight < minHeight) winHeight = minHeight;

            if (screenSizing == 0) // Even
            {
                // Scale both screens to the size of the window
                float baseRatio = (float)(width * 2) / height;
                float screenRatio = (float)winWidth / winHeight;
                largeScale = ((baseRatio > screenRatio) ? ((float)winWidth / (width * 2)) : ((float)winHeight / height));
                if (integerScale) largeScale = (int)largeScale;
                smallScale = largeScale;
            }
            else // Enlarge Top/Enlarge Bottom
            {
                float baseRatio = (float)width / height;

                // Scale the large screen to the size of the window minus room for the smaller screen
                float largeRatio = (float)(winWidth - width) / winHeight;
                largeScale = ((baseRatio > largeRatio) ? ((float)(winWidth - width) / width) : ((float)winHeight / height));
                if (integerScale) largeScale = (int)largeScale;

                // Scale the small screen to the remaining window space
                float smallRatio = (float)(winWidth - largeScale * width) / winHeight;
                smallScale = ((baseRatio > smallRatio) ? ((float)(winWidth - largeScale * width) / width) : ((float)winHeight / height));
                if (integerScale) smallScale = (int)smallScale;
            }

            // Remove the extra gap weight for the next calculations
            if (screenGap) width -= gap / 2;
        }

        // Calculate the dimensions of each screen
        if (screenSizing == 1) // Enlarge Top
        {
            topWidth  = largeScale * width;
            botWidth  = smallScale * width;
            topHeight = largeScale * height;
            botHeight = smallScale * height;
        }
        else // Even/Enlarge Bottom
        {
            topWidth  = smallScale * width;
            botWidth  = largeScale * width;
            topHeight = smallScale * height;
            botHeight = largeScale * height;
        }

        // Calculate the positions of each screen
        // The screens are centered and placed next to each other either vertically or horizontally
        if (vertical)
        {
            topX = (winWidth - topWidth) / 2;
            botX = (winWidth - botWidth) / 2;

            // Swap the screens if rotated clockwise to keep the top above the bottom
            if (screenRotation == 1) // Clockwise
            {
                botY = (winHeight - botHeight - topHeight) / 2;
                topY = botY + botHeight;

                // Add the gap between the screens if enabled
                if (screenGap)
                {
                    botY -= (largeScale * gap + smallScale * gap) / 4;
                    topY += (largeScale * gap + smallScale * gap) / 4;
                }
            }
            else // None/Counter-Clockwise
            {
                topY = (winHeight - topHeight - botHeight) / 2;
                botY = topY + topHeight;

                // Add the gap between the screens if enabled
                if (screenGap)
                {
                    topY -= (largeScale * gap + smallScale * gap) / 4;
                    botY += (largeScale * gap + smallScale * gap) / 4;
                }
            }
        }
        else // Horizontal
        {
            topY = (winHeight - topHeight) / 2;
            botY = (winHeight - botHeight) / 2;

            // Swap the screens if rotated clockwise to keep the top above the bottom
            if (screenRotation == 1) // Clockwise
            {
                botX = (winWidth - botWidth - topWidth) / 2;
                topX = botX + botWidth;

                // Add the gap between the screens if enabled
                if (screenGap)
                {
                    botX -= (largeScale * gap + smallScale * gap) / 4;
                    topX += (largeScale * gap + smallScale * gap) / 4;
                }
            }
            else // None/Counter-Clockwise
            {
                topX = (winWidth - topWidth - botWidth) / 2;
                botX = topX + topWidth;

                // Add the gap between the screens if enabled
                if (screenGap)
                {
                    topX -= (largeScale * gap + smallScale * gap) / 4;
                    botX += (largeScale * gap + smallScale * gap) / 4;
                }
            }
        }
    }
}

int ScreenLayout::getTouchX(int x, int y)
{
    // Map window coordinates to an X-coordinate on the touch screen
    switch (screenRotation)
    {
        case 0:  return       (x - botX) * 256 / botWidth;  // None
        case 1:  return       (y - botY) * 256 / botHeight; // Clockwise
        default: return 255 - (y - botY) * 256 / botHeight; // Counter-clockwise
    }
}

int ScreenLayout::getTouchY(int x, int y)
{
    // Map window coordinates to a Y-coordinate on the touch screen
    switch (screenRotation)
    {
        case 0:  return       (y - botY) * 192 / botHeight; // None
        case 1:  return 191 - (x - botX) * 192 / botWidth;  // Clockwise
        default: return       (x - botX) * 192 / botWidth;  // Counter-clockwise
    }
}
