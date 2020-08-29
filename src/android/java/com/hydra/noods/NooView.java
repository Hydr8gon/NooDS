package com.hydra.noods;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

public class NooView extends View
{
    private Bitmap bitmap;
    private Paint paint;
    private boolean started, gbaMode;

    public NooView(Context context)
    {
        super(context);

        paint = new Paint();
        started = false;

        // Handle screen touches
        setOnTouchListener(new View.OnTouchListener()
        {
            @Override
            public boolean onTouch(View view, MotionEvent event)
            {
                switch (event.getAction())
                {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_MOVE:
                    {
                        // Send the touch coordinates to the core
                        pressScreen((int)event.getX(), (int)event.getY());
                        break;
                    }

                    case MotionEvent.ACTION_UP:
                    {
                        // Send a touch release to the core
                        releaseScreen();
                        break;
                    }
                }

                return true;
            }
        });
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        // Check the current GBA mode status
        boolean gba = (isGbaMode() && getGbaCrop() != 0);

        // Update the layout if just starting or if GBA mode changed
        if (!started || gbaMode != gba)
        {
            started = true;
            gbaMode = gba;
            updateLayout(getWidth(), getHeight());
            bitmap = Bitmap.createBitmap((gbaMode ? 240 : 256), (gbaMode ? 160 : (192 * 2)), Bitmap.Config.ARGB_8888);
        }

        // Clear the display
        canvas.drawColor(Color.BLACK);

        // Copy the current frame to the bitmap
        copyFramebuffer(bitmap, gbaMode);

        // Rotate the screen(s) if necessary
        Matrix matrix = new Matrix();
        switch (getScreenRotation())
        {
            case 1: matrix.postRotate(90);  break; // Clockwise
            case 2: matrix.postRotate(270); break; // Counter-clockwise
        }

        if (gbaMode)
        {
            // Draw the GBA screen
            matrix.postScale((float)getTopWidth() / ((getScreenRotation() == 0) ? 240 : 160), (float)getTopHeight() / ((getScreenRotation() == 0) ? 160 : 240));
            canvas.drawBitmap(Bitmap.createBitmap(bitmap, 0, 0, 240, 160, matrix, getScreenFilter() != 0), getTopX(), getTopY(), paint);
        }
        else
        {
            // Draw the DS top screen
            Matrix top = new Matrix(matrix);
            top.postScale((float)getTopWidth() / ((getScreenRotation() == 0) ? 256 : 192), (float)getTopHeight() / ((getScreenRotation() == 0) ? 192 : 256));
            canvas.drawBitmap(Bitmap.createBitmap(bitmap, 0, 0, 256, 192, top, getScreenFilter() != 0), getTopX(), getTopY(), paint);

            // Draw the DS bottom screen
            Matrix bot = new Matrix(matrix);
            bot.postScale((float)getBotWidth() / ((getScreenRotation() == 0) ? 256 : 192), (float)getBotHeight() / ((getScreenRotation() == 0) ? 192 : 256));
            canvas.drawBitmap(Bitmap.createBitmap(bitmap, 0, 192, 256, 192, bot, getScreenFilter() != 0), getBotX(), getBotY(), paint);
        }

        // Request more
        invalidate();
    }

    public native void copyFramebuffer(Bitmap bitmap, boolean gbaCrop);
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
    public native void pressScreen(int x, int y);
    public native void releaseScreen();
}
