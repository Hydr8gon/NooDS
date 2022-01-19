/*
    Copyright 2019-2022 Hydr8gon

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
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;

public class NooButton extends Button
{
    private int id;

    public NooButton(Context context, int resId, int btnId, int x, int y, int width, int height)
    {
        super(context);

        // Set the button parameters
        setBackgroundResource(resId);
        id = btnId;
        setX(x);
        setY(y);
        setLayoutParams(new LayoutParams(width, height));
        setAlpha(0.5f);

        // Handle button touches
        if (id >= 4 && id <= 7) // D-pad
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
                        {
                            // Press the right key if in range, otherwise release
                            if (event.getX() > view.getWidth() * 2 / 3)
                                pressKey(4);
                            else
                                releaseKey(4);

                            // Press the left key if in range, otherwise release
                            if (event.getX() < view.getWidth() * 1 / 3)
                                pressKey(5);
                            else
                                releaseKey(5);

                            // Press the up key if in range, otherwise release
                            if (event.getY() < view.getHeight() * 1 / 3)
                                pressKey(6);
                            else
                                releaseKey(6);

                            // Press the down key if in range, otherwise release
                            if (event.getY() > view.getHeight() * 2 / 3)
                                pressKey(7);
                            else
                                releaseKey(7);

                            break;
                        }

                        case MotionEvent.ACTION_UP:
                        {
                            // Release all directional keys
                            releaseKey(4);
                            releaseKey(5);
                            releaseKey(6);
                            releaseKey(7);
                            break;
                        }
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
                    // Press or release the key specified by the ID
                    switch (event.getAction())
                    {
                        case MotionEvent.ACTION_DOWN: pressKey(id); break;
                        case MotionEvent.ACTION_UP: releaseKey(id); break;
                    }

                    return true;
                }
            });
        }
    }

    public native void pressKey(int key);
    public native void releaseKey(int key);
}
