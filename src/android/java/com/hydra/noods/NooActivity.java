package com.hydra.noods;

import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;

import android.graphics.Color;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

public class NooActivity extends AppCompatActivity
{
    private boolean running;
    private Thread core, audio, fps;
    private AudioTrack track;

    private boolean hidden;
    private ConstraintLayout layout;
    private NooView view;
    private NooButton buttons[];
    private TextView fpsCounter;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Get the display density and dimensions
        float d = getResources().getDisplayMetrics().density;
        int   w = getResources().getDisplayMetrics().widthPixels;
        int   h = getResources().getDisplayMetrics().heightPixels;

        hidden = false;
        layout = new ConstraintLayout(this);
        view = new NooView(this);
        buttons = new NooButton[9];

        // Create the buttons and place them based on the display information
        buttons[0] = new NooButton(this, R.drawable.a,       0, w     - (int)(d *  60), h - (int)(d * 135), (int)(d *  55), (int)(d *  55));
        buttons[1] = new NooButton(this, R.drawable.b,       1, w     - (int)(d * 115), h - (int)(d *  80), (int)(d *  55), (int)(d *  55));
        buttons[2] = new NooButton(this, R.drawable.x,      10, w     - (int)(d * 115), h - (int)(d * 190), (int)(d *  55), (int)(d *  55));
        buttons[3] = new NooButton(this, R.drawable.y,      11, w     - (int)(d * 170), h - (int)(d * 135), (int)(d *  55), (int)(d *  55));
        buttons[4] = new NooButton(this, R.drawable.start,   3, w / 2 + (int)(d *  16), h - (int)(d *  38), (int)(d *  33), (int)(d *  33));
        buttons[5] = new NooButton(this, R.drawable.select,  2, w / 2 - (int)(d *  49), h - (int)(d *  38), (int)(d *  33), (int)(d *  33));
        buttons[6] = new NooButton(this, R.drawable.l,       9,         (int)(d *   5), h - (int)(d * 400), (int)(d * 110), (int)(d *  44));
        buttons[7] = new NooButton(this, R.drawable.r,       8, w     - (int)(d * 115), h - (int)(d * 400), (int)(d * 110), (int)(d *  44));
        buttons[8] = new NooButton(this, R.drawable.dpad,    4,         (int)(d *   5), h - (int)(d * 174), (int)(d * 132), (int)(d * 132));

        // Add the view and buttons to the layout
        layout.addView(view);
        for (int i = 0; i < 9; i++)
            layout.addView(buttons[i]);

        // Add the FPS counter to the layout if enabled
        if (getShowFpsCounter() != 0)
        {
            fpsCounter = new TextView(this);
            fpsCounter.setTextSize(24);
            fpsCounter.setTextColor(Color.WHITE);
            layout.addView(fpsCounter);
        }

        setContentView(layout);

        // Set up audio playback
        track = new AudioTrack(AudioManager.STREAM_MUSIC, 32768, AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT, 1024 * 2 * 2, AudioTrack.MODE_STREAM);
        track.play();
    }

    @Override
    protected void onPause()
    {
        super.onPause();

        running = false;

        // Wait for the emulator to stop
        try
        {
            core.join();
            audio.join();
            if (getShowFpsCounter() != 0)
                fps.join();
        }
        catch (Exception e)
        {
            // Oh well, I guess
        }

        // Write the save file
        // There's no reliable way to call something on program termination, so this has to be done here
        writeSave();
    }

    @Override
    protected void onResume()
    {
        super.onResume();

        // Hide the status bar
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

        running = true;

        // Prepare the core thread
        core = new Thread()
        {
            @Override
            public void run()
            {             
                while (running)
                    runFrame();
            }
        };

        core.setPriority(Thread.MAX_PRIORITY);
        core.start();

        // Prepare the audio thread
        audio = new Thread()
        {
            @Override
            public void run()
            {             
                while (running)
                {
                    short[] buffer = new short[1024 * 2];
                    fillAudioBuffer(buffer);
                    track.write(buffer, 0, 1024 * 2);
                }
            }
        };

        audio.setPriority(Thread.NORM_PRIORITY);
        audio.start();

        // Prepare the FPS counter thread if enabled
        if (getShowFpsCounter() != 0)
        {
            fps = new Thread()
            {
                @Override
                public void run()
                {             
                    while (running)
                    {
                        // Update the FPS counter text on the UI thread
                        runOnUiThread(new Runnable()
                        {
                            @Override
                            public void run()
                            {
                                fpsCounter.setText(Integer.toString(getFps()) + " FPS");
                            }
                        });

                        // Wait a second before updating the FPS counter again
                        try
                        {
                            Thread.sleep(1000);
                        }
                        catch (Exception e)
                        {
                            // Oh well, I guess
                        }
                    }
                }
            };

            fps.setPriority(Thread.MIN_PRIORITY);
            fps.start();
        }
    }

    @Override
    public void onBackPressed()
    {
        // Toggle hiding the buttons
        hidden = !hidden;

        // Add or remove the buttons from the layout accordingly
        if (hidden)
        {
            for (int i = 0; i < 9; i++)
                layout.removeView(buttons[i]);
        }
        else
        {
            for (int i = 0; i < 9; i++)
                layout.addView(buttons[i]);
        }
    }

    public native void fillAudioBuffer(short[] buffer);
    public native int getShowFpsCounter();
    public native int getFps();
    public native void runFrame();
    public native void writeSave();
}
