/*
    Copyright 2019 Hydr8gon

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

#include <cstdio>
#include <cstring>
#include <thread>
#include "GL/glut.h"

#include "../core.h"
#include "../gpu.h"

// Just do this instead of including glext.h
#ifdef _WIN32
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif

const char keyMap[] = { 'l', 'k', 'g', 'h', 'd', 'a', 'w', 's', 'p', 'q', 'o', 'i' };

void runCore()
{
    while (true)
        core::runDot();
}

void draw()
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 192 * 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, gpu::displayBuffer);
    glBegin(GL_QUADS);
    glTexCoord2i(1, 1); glVertex2f( 1, -1);
    glTexCoord2i(0, 1); glVertex2f(-1, -1);
    glTexCoord2i(0, 0); glVertex2f(-1,  1);
    glTexCoord2i(1, 0); glVertex2f( 1,  1);
    glEnd();
    glFlush();
    glutPostRedisplay();
}

void keyDown(unsigned char key, int x, int y)
{
    for (int i = 0; i < 10; i++)
    {
        if (key == keyMap[i])
            core::pressKey(i);
    }
}

void keyUp(unsigned char key, int x, int y)
{
    for (int i = 0; i < 10; i++)
    {
        if (key == keyMap[i])
            core::releaseKey(i);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Please specify a ROM to load.\n");
        return 0;
    }

    if (strcmp(argv[1], "-v") != 0)
    {
        fclose(stdout);
        if (!core::loadRom(argv[1]))
            return 0;
    }
    else if (!core::loadRom(argv[2]))
    {
        return 0;
    }

    glutInit(&argc, argv);
    glutInitWindowSize(256, 192 * 2);
    glutCreateWindow("NooDS");
    glEnable(GL_TEXTURE_2D);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glutDisplayFunc(draw);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);

    std::thread core(runCore);
    glutMainLoop();

    return 0;
}
