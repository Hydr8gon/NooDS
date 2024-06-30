/*
    Copyright 2019-2024 Hydr8gon

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

#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

class ScreenLayout
{
    public:
        static int screenPosition;
        static int screenRotation;
        static int screenArrangement;
        static int screenSizing;
        static int screenGap;
        static int integerScale;
        static int gbaCrop;

        int winWidth = 0, winHeight = 0;
        int minWidth = 0, minHeight = 0;
        int topX = 0, botX = 0;
        int topY = 0, botY = 0;
        int topWidth = 0, botWidth = 0;
        int topHeight = 0, botHeight = 0;

        static void addSettings();

        void update(int winWidth, int winHeight, bool gbaMode, bool splitScreens = false);
        int getTouchX(int x, int y);
        int getTouchY(int x, int y);
};

#endif // SCREEN_LAYOUT_H
