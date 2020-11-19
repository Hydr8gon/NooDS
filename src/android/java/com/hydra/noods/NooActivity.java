package com.hydra.noods;

import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;

import android.content.Intent;
import android.graphics.Color;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.widget.TextView;

public class NooActivity extends AppCompatActivity
{
    private boolean running;
    private Thread core, audio, fps;
    private AudioTrack track;

    private ConstraintLayout layout;
    private GLSurfaceView view;
    private NooRenderer renderer;
    private NooButton buttons[];
    private TextView fpsCounter;
    private boolean showingButtons, showingFps;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        layout = new ConstraintLayout(this);
        view = new GLSurfaceView(this);
        renderer = new NooRenderer();
        buttons = new NooButton[9];
        fpsCounter = new TextView(this);

        // Prepare the GL renderer
        view.setEGLContextClientVersion(2);
        view.setRenderer(renderer);

        // Handle non-button screen touches
        view.setOnTouchListener(new View.OnTouchListener()
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

        // Add the view to the layout
        layout.addView(view);

        // Get the display density and dimensions
        final float d = getResources().getDisplayMetrics().density;
        final int   w = getResources().getDisplayMetrics().widthPixels;
        final int   h = getResources().getDisplayMetrics().heightPixels;

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

        // Add the buttons to the layout
        for (int i = 0; i < 9; i++)
            layout.addView(buttons[i]);
        showingButtons = true;

        // Create the FPS counter
        fpsCounter.setTextSize(24);
        fpsCounter.setTextColor(Color.WHITE);

        // Add the FPS counter to the layout if enabled
        if (showingFps = (getShowFpsCounter() != 0))
            layout.addView(fpsCounter);

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
        view.onPause();

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
        view.onResume();

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

        if (getShowFpsCounter() != 0)
        {
            // Add the FPS counter to the layout if enabled
            if (!showingFps)
            {
                layout.addView(fpsCounter);
                showingFps = true;
            }

            // Prepare the FPS counter thread if enabled
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
        else if (showingFps)
        {
            // Remove the FPS counter from the layout if disabled
            layout.removeView(fpsCounter);
            showingFps = false;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.noo_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        switch (item.getItemId())
        {
            case R.id.controls_action:
            {
                // Toggle hiding the on-screen buttons
                if (showingButtons = !showingButtons)
                {
                    for (int i = 0; i < 9; i++)
                        layout.addView(buttons[i]);
                }
                else
                {
                    for (int i = 0; i < 9; i++)
                        layout.removeView(buttons[i]);
                }
                return true;
            }

            case R.id.settings_action:
            {
                // Open the settings menu
                startActivity(new Intent(this, SettingsMenu.class));
                return true;
            }

            case R.id.browser_action:
            {
                // Go back to the file browser
                startActivity(new Intent(this, FileBrowser.class));
                finish();
                return true;
            }
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onOptionsMenuClosed(Menu menu)
    {
        super.onOptionsMenuClosed(menu);

        // Rehide the status bar
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onBackPressed()
    {
        // Simulate a menu button press to open the menu
        BaseInputConnection inputConnection = new BaseInputConnection(view, true);
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU));
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MENU));
    }

    public native void fillAudioBuffer(short[] buffer);
    public native int getShowFpsCounter();
    public native int getFps();
    public native void runFrame();
    public native void writeSave();
    public native void pressScreen(int x, int y);
    public native void releaseScreen();
}
