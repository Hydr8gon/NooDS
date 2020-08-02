package com.hydra.noods;

import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;

public class NooActivity extends AppCompatActivity
{
    private boolean running;
    private AudioTrack track;
    private Thread core, audio;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Get the display density and dimensions
        float d = getResources().getDisplayMetrics().density;
        int   w = getResources().getDisplayMetrics().widthPixels;
        int   h = getResources().getDisplayMetrics().heightPixels;

        ConstraintLayout layout = new ConstraintLayout(this);
        NooView view = new NooView(this);

        // Place the buttons based on the display information
        NooButton a      = new NooButton(this, R.drawable.a,       0, w     - (int)(d *  60), h - (int)(d * 135), (int)(d *  55), (int)(d *  55));
        NooButton b      = new NooButton(this, R.drawable.b,       1, w     - (int)(d * 115), h - (int)(d *  80), (int)(d *  55), (int)(d *  55));
        NooButton x      = new NooButton(this, R.drawable.x,      10, w     - (int)(d * 115), h - (int)(d * 190), (int)(d *  55), (int)(d *  55));
        NooButton y      = new NooButton(this, R.drawable.y,      11, w     - (int)(d * 170), h - (int)(d * 135), (int)(d *  55), (int)(d *  55));
        NooButton start  = new NooButton(this, R.drawable.start,   3, w / 2 + (int)(d *  16), h - (int)(d *  38), (int)(d *  33), (int)(d *  33));
        NooButton select = new NooButton(this, R.drawable.select,  2, w / 2 - (int)(d *  49), h - (int)(d *  38), (int)(d *  33), (int)(d *  33));
        NooButton l      = new NooButton(this, R.drawable.l,       9,         (int)(d *   5), h - (int)(d * 400), (int)(d * 110), (int)(d *  44));
        NooButton r      = new NooButton(this, R.drawable.r,       8, w     - (int)(d * 115), h - (int)(d * 400), (int)(d * 110), (int)(d *  44));
        NooButton dpad   = new NooButton(this, R.drawable.dpad,    4,         (int)(d *   5), h - (int)(d * 174), (int)(d * 132), (int)(d * 132));

        // Prepare the layout
        layout.addView(view);
        layout.addView(a);
        layout.addView(b);
        layout.addView(x);
        layout.addView(y);
        layout.addView(start);
        layout.addView(select);
        layout.addView(l);
        layout.addView(r);
        layout.addView(dpad);
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

        // Start the core thread
        core.setPriority(Thread.MAX_PRIORITY);
        core.start();

        // Start the audio thread
        audio.setPriority(Thread.MIN_PRIORITY);
        audio.start();
    }

    @Override
    public void onBackPressed()
    {
        // Do nothing
    }

    public native void runFrame();
    public native void fillAudioBuffer(short[] buffer);
    public native void writeSave();
}
