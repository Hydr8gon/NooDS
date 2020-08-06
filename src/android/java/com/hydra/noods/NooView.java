package com.hydra.noods;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

public class NooView extends View
{
    private Bitmap bitmap;
    private int scale, x;

    public NooView(Context context)
    {
        super(context);
        bitmap = Bitmap.createBitmap(256, 192 * 2, Bitmap.Config.ARGB_8888);
        scale = -1;

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
                        // Determine the touch position relative to the emulated touch screen
                        int touchX = ((int)event.getX() - x) / scale;
                        int touchY = (int)event.getY() / scale - 192;

                        // Send the touch coordinates to the core
                        pressScreen(touchX, touchY);
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
        // Calculate the screen dimensions
        if (scale == -1)
        {
            scale = getWidth() / 256;
            x = (getWidth() - 256 * scale) / 2;
        }

        // Clear the screen
        canvas.drawColor(Color.BLACK);

        // Draw the framebuffer
        copyFramebuffer(bitmap);
        canvas.drawBitmap(Bitmap.createScaledBitmap(bitmap, 256 * scale, 192 * 2 * scale, false), x, 0, new Paint());

        // Request more
        invalidate();
    }

    public native void copyFramebuffer(Bitmap bitmap);
    public native void pressScreen(int x, int y);
    public native void releaseScreen();
}
