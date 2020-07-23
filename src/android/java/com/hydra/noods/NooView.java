package com.hydra.noods;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

public class NooView extends View
{
    private Bitmap bitmap;

    public NooView(Context context, AttributeSet attrs)
    {
        super(context, attrs);

        bitmap = Bitmap.createBitmap(256, 192 * 2, Bitmap.Config.ARGB_8888);
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        // Clear the screen
        canvas.drawColor(Color.BLACK);

        // Draw the framebuffer
        copyFramebuffer(bitmap);
        int scale = getWidth() / 256;
        int x = (getWidth() - 256 * scale) / 2;
        canvas.drawBitmap(Bitmap.createScaledBitmap(bitmap, 256 * scale, 192 * 2 * scale, false), x, 0, new Paint());

        // Request more
        invalidate();
    }

    public native void copyFramebuffer(Bitmap bitmap);
}
