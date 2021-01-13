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

#ifndef NDS_ICON_H
#define NDS_ICON_H

#include <cstdint>
#include <string>

class NdsIcon
{
    public:
        NdsIcon(std::string romPath);

        uint32_t *getIcon() { return icon; }

    private:
        uint32_t icon[32 * 32];
};

#endif // NDS_ICON_H
