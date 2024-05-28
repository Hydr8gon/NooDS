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

package com.hydra.noods;

import android.content.Context;
import android.os.Vibrator;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;

public class NooButton extends Button
{
    private Vibrator vibrator;
    private int[] resIds;
    private int[] btnIds;
    private int state = 0;
    private int lastState = 0;

    public static int[] resAbxy =
    {
        R.drawable.abxy,          R.drawable.abxy_pressed1, R.drawable.abxy_pressed5, R.drawable.abxy,
        R.drawable.abxy_pressed7, R.drawable.abxy_pressed8, R.drawable.abxy_pressed6, R.drawable.abxy,
        R.drawable.abxy_pressed3, R.drawable.abxy_pressed2, R.drawable.abxy_pressed4, R.drawable.abxy,
        R.drawable.abxy,          R.drawable.abxy,          R.drawable.abxy,          R.drawable.abxy
    };

    public static int[] resDpad =
    {
        R.drawable.dpad,          R.drawable.dpad_pressed1, R.drawable.dpad_pressed5, R.drawable.dpad,
        R.drawable.dpad_pressed7, R.drawable.dpad_pressed8, R.drawable.dpad_pressed6, R.drawable.dpad,
        R.drawable.dpad_pressed3, R.drawable.dpad_pressed2, R.drawable.dpad_pressed4, R.drawable.dpad,
        R.drawable.dpad,          R.drawable.dpad,          R.drawable.dpad,          R.drawable.dpad
    };

    public NooButton(Context context, int[] resIds, int[] btnIds, int x, int y, int width, int height)
    {
        super(context);
        vibrator = (Vibrator)context.getSystemService(Context.VIBRATOR_SERVICE);
        this.resIds = resIds;
        this.btnIds = btnIds;

        // Set the button parameters
        setBackgroundResource(resIds[0]);
        setX(x);
        setY(y);
        setLayoutParams(new LayoutParams(width, height));
        setAlpha(0.5f);

        // Handle button touches
        if (btnIds.length >= 4) // D-pad
        {
            setOnTouchListener(new Button.OnTouchListener()
            {
                @Override
                public boolean onTouch(View view, MotionEvent event)
                {
                    switch (event.getAction())
                    {
                        case MotionEvent.ACTION_DOWN:
                        case MotionEvent.ACTION_MOVE:
                            // Press the right key if in range, otherwise release
                            if (event.getX() > view.getWidth() * 2 / 3)
                                pressDKey(0);
                            else
                                releaseKey(btnIds[0]);

                            // Press the left key if in range, otherwise release
                            if (event.getX() < view.getWidth() * 1 / 3)
                                pressDKey(1);
                            else
                                releaseKey(btnIds[1]);

                            // Press the up key if in range, otherwise release
                            if (event.getY() < view.getHeight() * 1 / 3)
                                pressDKey(2);
                            else
                                releaseKey(btnIds[2]);

                            // Press the down key if in range, otherwise release
                            if (event.getY() > view.getHeight() * 2 / 3)
                                pressDKey(3);
                            else
                                releaseKey(btnIds[3]);

                            // Update the image and vibrate
                            if (state != lastState)
                                setBackgroundResource(resIds[state]);
                            if ((state & ~lastState) != 0)
                                vibrator.vibrate(SettingsMenu.getVibrateStrength() * 4);
                            lastState = state;
                            state = 0;
                            break;

                        case MotionEvent.ACTION_UP:
                            // Release all directions and update the image
                            for (int i = 0; i < 4; i++)
                                releaseKey(btnIds[i]);
                            setBackgroundResource(resIds[0]);
                            lastState = state = 0;
                            break;
                    }
                    return true;
                }
            });
        }
        else
        {
            setOnTouchListener(new Button.OnTouchListener()
            {
                @Override
                public boolean onTouch(View view, MotionEvent event)
                {
                    switch (event.getAction())
                    {
                        case MotionEvent.ACTION_DOWN:
                            // Press a key, update the image, and vibrate
                            pressKey(btnIds[0]);
                            setBackgroundResource(resIds[1]);
                            vibrator.vibrate(SettingsMenu.getVibrateStrength() * 4);
                            break;

                        case MotionEvent.ACTION_UP:
                            // Release a key and update the image
                            releaseKey(btnIds[0]);
                            setBackgroundResource(resIds[0]);
                            break;
                    }
                    return true;
                }
            });
        }
    }

    private void pressDKey(int key)
    {
        // Press a D-pad key and track the state
        pressKey(btnIds[key]);
        state |= 1 << key;
    }

    public static native void pressKey(int key);
    public static native void releaseKey(int key);
}
