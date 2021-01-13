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

#ifndef WIFI_H
#define WIFI_H

#include <cstdint>

class Core;

class Wifi
{
    public:
        Wifi(Core *core);

        uint16_t readWModeWep()          { return wModeWep;        }
        uint16_t readWIrf()              { return wIrf;            }
        uint16_t readWIe()               { return wIe;             }
        uint16_t readWMacaddr(int index) { return wMacaddr[index]; }
        uint16_t readWBssid(int index)   { return wBssid[index];   }
        uint16_t readWAidFull()          { return wAidFull;        }
        uint16_t readWPowerstate()       { return wPowerstate;     }
        uint16_t readWPowerforce()       { return wPowerforce;     }
        uint16_t readWRxbufBegin()       { return wRxbufBegin;     }
        uint16_t readWRxbufEnd()         { return wRxbufEnd;       }
        uint16_t readWRxbufWrAddr()      { return wRxbufWrAddr;    }
        uint16_t readWRxbufRdAddr()      { return wRxbufRdAddr;    }
        uint16_t readWRxbufReadcsr()     { return wRxbufReadcsr;   }
        uint16_t readWRxbufGap()         { return wRxbufGap;       }
        uint16_t readWRxbufGapdisp()     { return wRxbufGapdisp;   }
        uint16_t readWRxbufCount()       { return wRxbufCount;     }
        uint16_t readWTxbufWrAddr()      { return wTxbufWrAddr;    }
        uint16_t readWTxbufCount()       { return wTxbufCount;     }
        uint16_t readWTxbufGap()         { return wTxbufGap;       }
        uint16_t readWTxbufGapdisp()     { return wTxbufGapdisp;   }
        uint16_t readWConfig(int index)  { return wConfig[index];  }
        uint16_t readWBeaconcount2()     { return wBeaconcount2;   }
        uint16_t readWBbRead()           { return wBbRead;         }
        uint16_t readWRxbufRdData();

        void writeWModeWep(uint16_t mask, uint16_t value);
        void writeWIrf(uint16_t mask, uint16_t value);
        void writeWIe(uint16_t mask, uint16_t value);
        void writeWMacaddr(int index, uint16_t mask, uint16_t value);
        void writeWBssid(int index, uint16_t mask, uint16_t value);
        void writeWAidFull(uint16_t mask, uint16_t value);
        void writeWPowerstate(uint16_t mask, uint16_t value);
        void writeWPowerforce(uint16_t mask, uint16_t value);
        void writeWRxbufBegin(uint16_t mask, uint16_t value);
        void writeWRxbufEnd(uint16_t mask, uint16_t value);
        void writeWRxbufWrAddr(uint16_t mask, uint16_t value);
        void writeWRxbufRdAddr(uint16_t mask, uint16_t value);
        void writeWRxbufReadcsr(uint16_t mask, uint16_t value);
        void writeWRxbufGap(uint16_t mask, uint16_t value);
        void writeWRxbufGapdisp(uint16_t mask, uint16_t value);
        void writeWRxbufCount(uint16_t mask, uint16_t value);
        void writeWTxbufWrAddr(uint16_t mask, uint16_t value);
        void writeWTxbufCount(uint16_t mask, uint16_t value);
        void writeWTxbufWrData(uint16_t mask, uint16_t value);
        void writeWTxbufGap(uint16_t mask, uint16_t value);
        void writeWTxbufGapdisp(uint16_t mask, uint16_t value);
        void writeWConfig(int index, uint16_t mask, uint16_t value);
        void writeWBeaconcount2(uint16_t mask, uint16_t value);
        void writeWBbCnt(uint16_t mask, uint16_t value);
        void writeWBbWrite(uint16_t mask, uint16_t value);
        void writeWIrfSet(uint16_t mask, uint16_t value);

    private:
        Core *core;

        uint8_t bbRegisters[0x100] = {};

        uint16_t wModeWep = 0;
        uint16_t wIrf = 0;
        uint16_t wIe = 0;
        uint16_t wMacaddr[3] = {};
        uint16_t wBssid[3] = {};
        uint16_t wAidFull = 0;
        uint16_t wPowerstate = 0x0200;
        uint16_t wPowerforce = 0;
        uint16_t wRxbufBegin = 0;
        uint16_t wRxbufEnd = 0;
        uint16_t wRxbufWrAddr = 0;
        uint16_t wRxbufRdAddr = 0;
        uint16_t wRxbufReadcsr = 0;
        uint16_t wRxbufGap = 0;
        uint16_t wRxbufGapdisp = 0;
        uint16_t wRxbufCount = 0;
        uint16_t wTxbufWrAddr = 0;
        uint16_t wTxbufCount = 0;
        uint16_t wTxbufGap = 0;
        uint16_t wTxbufGapdisp = 0;
        uint16_t wBeaconcount2 = 0;
        uint16_t wBbWrite = 0;
        uint16_t wBbRead = 0;

        uint16_t wConfig[15] =
        {
            0x0048, 0x4840, 0x0000, 0x0000, 0x0142,
            0x8064, 0x0000, 0x2443, 0x0042, 0x0016,
            0x0016, 0x0016, 0x162C, 0x0204, 0x0058
        };

        void sendInterrupt(int bit);
};

#endif // WIFI_H
