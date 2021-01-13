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

#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

class ScreenLayout
{
    public:
        static void addSettings();

        void update(int winWidth, int winHeight, bool gbaMode);

        int getTouchX(int x, int y);
        int getTouchY(int x, int y);

        int getMinWidth()  { return minWidth;  }
        int getMinHeight() { return minHeight; }
        int getTopX()      { return topX;      }
        int getBotX()      { return botX;      }
        int getTopY()      { return topY;      }
        int getBotY()      { return botY;      }
        int getTopWidth()  { return topWidth;  }
        int getBotWidth()  { return botWidth;  }
        int getTopHeight() { return topHeight; }
        int getBotHeight() { return botHeight; }

        static int getScreenRotation()    { return screenRotation;    }
        static int getScreenArrangement() { return screenArrangement; }
        static int getScreenSizing()      { return screenSizing;      }
        static int getScreenGap()         { return screenGap;         }
        static int getIntegerScale()      { return integerScale;      }
        static int getGbaCrop()           { return gbaCrop;           }

        static void setScreenRotation(int value)    { screenRotation    = value; }
        static void setScreenArrangement(int value) { screenArrangement = value; }
        static void setScreenSizing(int value)      { screenSizing      = value; }
        static void setScreenGap(int value)         { screenGap         = value; }
        static void setIntegerScale(int value)      { integerScale      = value; }
        static void setGbaCrop(int value)           { gbaCrop           = value; }

    private:
        int minWidth = 0, minHeight = 0;
        int topX = 0, botX = 0;
        int topY = 0, botY = 0;
        int topWidth = 0, botWidth = 0;
        int topHeight = 0, botHeight = 0;

        static int screenRotation;
        static int screenArrangement;
        static int screenSizing;
        static int screenGap;
        static int integerScale;
        static int gbaCrop;
};

#endif // SCREEN_LAYOUT_H
