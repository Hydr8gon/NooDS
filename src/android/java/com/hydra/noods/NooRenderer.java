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
    private int program;
    private int textures[];
    private Bitmap bitmap;
    private boolean gbaMode;
    private int width, height;

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
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, (getScreenFilter() == 1) ? GLES20.GL_LINEAR : GLES20.GL_NEAREST);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, (getScreenFilter() == 1) ? GLES20.GL_LINEAR : GLES20.GL_NEAREST);

        bitmap = Bitmap.createBitmap(256, 192 * 2, Bitmap.Config.ARGB_8888);
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

        // Update the layout
        GLES20.glViewport(0, 0, width, height);
        updateLayout(width, height);
        this.width = width;
        this.height = height;
    }

    @Override
    public void onDrawFrame(GL10 unused)
    {
        // Update the layout if GBA mode changed
        if (gbaMode != (isGbaMode() && getGbaCrop() != 0))
        {
            gbaMode = !gbaMode;
            updateLayout(width, height);
            bitmap = Bitmap.createBitmap((gbaMode ? 240 : 256), (gbaMode ? 160 : (192 * 2)), Bitmap.Config.ARGB_8888);
        }

        // Clear the display
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        // Wait until a new frame is ready to prevent stuttering
        // This sucks, but buffers are automatically swapped, so returning would cause even worse stutter
        while (!copyFramebuffer(bitmap, gbaMode));
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);

        if (gbaMode)
        {
            // Draw the GBA screen
            drawScreen(getTopX(), getTopY(), getTopWidth(), getTopHeight(), 0.0f, 0.0f, 1.0f, 1.0f);
        }
        else
        {
            // Draw the DS top and bottom screens
            drawScreen(getTopX(), getTopY(), getTopWidth(), getTopHeight(), 0.0f, 0.0f, 1.0f, 0.5f);
            drawScreen(getBotX(), getBotY(), getBotWidth(), getBotHeight(), 0.0f, 0.5f, 1.0f, 1.0f);
        }
    }

    private void drawScreen(float x, float y, float w, float h, float s1, float t1, float s2, float t2)
    {
        // Define the vertices
        final float[] vertices =
        {
            x,     y,     s1, t1,
            x,     y + h, s1, t2,
            x + w, y,     s2, t1,
            x + w, y + h, s2, t2
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

    public native boolean copyFramebuffer(Bitmap bitmap, boolean gbaCrop);
    public native int getScreenRotation();
    public native int getGbaCrop();
    public native int getScreenFilter();
    public native void updateLayout(int width, int height);
    public native int getTopX();
    public native int getBotX();
    public native int getTopY();
    public native int getBotY();
    public native int getTopWidth();
    public native int getBotWidth();
    public native int getTopHeight();
    public native int getBotHeight();
    public native boolean isGbaMode();
}
