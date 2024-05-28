/*
    Copyright 2019-2023 Hydr8gon

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

package com.hydra.noods;

import android.graphics.Bitmap;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public class NooRenderer implements GLSurfaceView.Renderer
{
    public int width;
    public int height;

    private NooActivity activity;
    private int program;
    private int textures[];
    private Bitmap bitmap;
    private int highRes3D;
    private boolean gbaMode;

    private final String vertexShader =
        "precision mediump float;"                                      +
        "uniform mat4 uProjection;"                                     +
        "attribute vec2 aPosition;"                                     +
        "attribute vec2 aTexCoord;"                                     +
        "varying vec2 vTexCoord;"                                       +
        "void main()"                                                   +
        "{"                                                             +
        "    gl_Position = uProjection * vec4(aPosition.xy, 0.0, 1.0);" +
        "    vTexCoord = aTexCoord;"                                    +
        "}";

    private final String fragmentShader =
        "precision mediump float;"                           +
        "uniform sampler2D uTexture;"                        +
        "varying vec2 vTexCoord;"                            +
        "void main()"                                        +
        "{"                                                  +
        "    gl_FragColor = texture2D(uTexture, vTexCoord);" +
        "}";

    NooRenderer(NooActivity activity)
    {
        super();
        this.activity = activity;
    }

    @Override
    public void onSurfaceCreated(GL10 unused, EGLConfig config)
    {
        // Compile the vertex shader
        int vertShader = GLES20.glCreateShader(GLES20.GL_VERTEX_SHADER);
        GLES20.glShaderSource(vertShader, vertexShader);
        GLES20.glCompileShader(vertShader);

        // Compile the fragment shader
        int fragShader = GLES20.glCreateShader(GLES20.GL_FRAGMENT_SHADER);
        GLES20.glShaderSource(fragShader, fragmentShader);
        GLES20.glCompileShader(fragShader);

        // Attach the shaders to the program
        program = GLES20.glCreateProgram();
        GLES20.glAttachShader(program, vertShader);
        GLES20.glAttachShader(program, fragShader);
        GLES20.glLinkProgram(program);
        GLES20.glUseProgram(program);

        GLES20.glDeleteShader(vertShader);
        GLES20.glDeleteShader(fragShader);

        // Set up texturing
        textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textures[0]);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        int filter = (SettingsMenu.getScreenFilter() == 1) ? GLES20.GL_LINEAR : GLES20.GL_NEAREST;
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, filter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, filter);

        bitmap = Bitmap.createBitmap(256, 192 * 2, Bitmap.Config.ARGB_8888);
        highRes3D = 0;
        gbaMode = false;
    }

    @Override
    public void onSurfaceChanged(GL10 unused, int width, int height)
    {
        // Define the 2D projection matrix
        final float[] projection =
        {
             2.0f / width,  0.0f,          0.0f, 0.0f,
             0.0f,         -2.0f / height, 0.0f, 0.0f,
             0.0f,          0.0f,          0.0f, 0.0f,
            -1.0f,          1.0f,          0.0f, 1.0f
        };

        // Pass the data to the shaders
        FloatBuffer data = ByteBuffer.allocateDirect(projection.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        data.put(projection);
        data.position(0);
        GLES20.glUniformMatrix4fv(GLES20.glGetUniformLocation(program, "uProjection"), 1, false, data);

        // Update the screen layout
        this.width = width;
        this.height = height;
        GLES20.glViewport(0, 0, width, height);
        updateLayout(width, height);

        // Update the button layout
        activity.runOnUiThread(new Runnable()
        {
            @Override
            public void run()
            {
                activity.updateButtons();
            }
        });
    }

    @Override
    public void onDrawFrame(GL10 unused)
    {
        // Update the resolution if the high-res 3D setting changed
        if (highRes3D != SettingsMenu.getHighRes3D())
        {
            highRes3D = SettingsMenu.getHighRes3D();
            bitmap = Bitmap.createBitmap((gbaMode ? 240 : 256) << highRes3D,
                (gbaMode ? 160 : (192 * 2)) << highRes3D, Bitmap.Config.ARGB_8888);
        }

        // Update the layout if GBA mode changed
        if (gbaMode != (activity.isGbaMode() && SettingsMenu.getGbaCrop() != 0))
        {
            gbaMode = !gbaMode;
            updateLayout(width, height);
            bitmap = Bitmap.createBitmap((gbaMode ? 240 : 256) << highRes3D,
                (gbaMode ? 160 : (192 * 2)) << highRes3D, Bitmap.Config.ARGB_8888);
        }

        // Clear the display
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        // Wait until a new frame is ready to prevent stuttering
        // This sucks, but buffers are automatically swapped, so returning would cause even worse stutter
        while (!copyFramebuffer(bitmap, gbaMode) && activity.isRunning());
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);

        if (gbaMode)
        {
            // Draw the GBA screen
            drawScreen(getTopX(), getTopY(), getTopWidth(), getTopHeight(), 0.0f, 0.0f, 1.0f, 1.0f);
        }
        else
        {
            // Draw the DS top and bottom screens
            if (SettingsMenu.getScreenArrangement() != 3 || SettingsMenu.getScreenSizing() < 2)
                drawScreen(getTopX(), getTopY(), getTopWidth(), getTopHeight(), 0.0f, 0.0f, 1.0f, 0.5f);
            if (SettingsMenu.getScreenArrangement() != 3 || SettingsMenu.getScreenSizing() == 2)
                drawScreen(getBotX(), getBotY(), getBotWidth(), getBotHeight(), 0.0f, 0.5f, 1.0f, 1.0f);
        }
    }

    private void drawScreen(float x, float y, float w, float h, float s1, float t1, float s2, float t2)
    {
        final int rot = SettingsMenu.getScreenRotation();

        // Arrange the S coordinates for rotation
        final float s[][] =
        {
            { s1, s1, s2, s2 }, // None
            { s1, s2, s1, s2 }, // Clockwise
            { s2, s1, s2, s1 }  // Counter-Clockwise
        };

        // Arrange the T coordinates for rotation
        final float t[][] =
        {
            { t1, t2, t1, t2 }, // None
            { t2, t2, t1, t1 }, // Clockwise
            { t1, t1, t2, t2 }  // Counter-Clockwise
        };

        // Define the vertices
        final float[] vertices =
        {
            x,     y,     s[rot][0], t[rot][0],
            x,     y + h, s[rot][1], t[rot][1],
            x + w, y,     s[rot][2], t[rot][2],
            x + w, y + h, s[rot][3], t[rot][3]
        };

        FloatBuffer data = ByteBuffer.allocateDirect(vertices.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        data.put(vertices);

        // Pass the position data to the shaders
        data.position(0);
        final int position = GLES20.glGetAttribLocation(program, "aPosition");
        GLES20.glVertexAttribPointer(position, 2, GLES20.GL_FLOAT, false, 4 * 4, data);
        GLES20.glEnableVertexAttribArray(position);

        // Pass the texture coordinate data to the shaders
        data.position(2);
        final int texCoord = GLES20.glGetAttribLocation(program, "aTexCoord");
        GLES20.glVertexAttribPointer(texCoord, 2, GLES20.GL_FLOAT, false, 4 * 4, data);
        GLES20.glEnableVertexAttribArray(texCoord);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    public static native boolean copyFramebuffer(Bitmap bitmap, boolean gbaCrop);
    public static native void updateLayout(int width, int height);
    public static native int getTopX();
    public static native int getBotX();
    public static native int getTopY();
    public static native int getBotY();
    public static native int getTopWidth();
    public static native int getBotWidth();
    public static native int getTopHeight();
    public static native int getBotHeight();
}
