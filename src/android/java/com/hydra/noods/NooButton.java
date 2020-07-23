package com.hydra.noods;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;

public class NooButton extends Button
{
    private int id;

    public NooButton(Context context, AttributeSet attrs)
    {
        super(context, attrs);

        // The button ID is set by the tag, and determines which input to send to the core
        id = Integer.parseInt(getTag().toString());

        setOnTouchListener(new Button.OnTouchListener()
        {
            @Override
            public boolean onTouch(View view, MotionEvent event)
            {
                // Send a button press or release to the core
                switch (event.getAction())
                {
                    case MotionEvent.ACTION_DOWN: pressKey(id); break;
                    case MotionEvent.ACTION_UP: releaseKey(id); break;
                }

                return true;
            }
        });
    }

    public native void pressKey(int key);
    public native void releaseKey(int key);
}
