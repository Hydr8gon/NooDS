package com.hydra.noods;

import androidx.appcompat.app.AppCompatActivity;

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

        setContentView(R.layout.noo_activity);

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
    }

    @Override
    protected void onResume()
    {
        super.onResume();

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
    protected void onDestroy()
    {
        super.onDestroy();

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

        // Close the core to ensure the save gets written
        closeRom();
    }

    @Override
    public void onBackPressed()
    {
        // Do nothing
    }

    public native void runFrame();
    public native void fillAudioBuffer(short[] buffer);
    public native void closeRom();
}
