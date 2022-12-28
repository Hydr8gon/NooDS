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

import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
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
    private Thread coreThread, saveThread, fpsThread;

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
        renderer = new NooRenderer(this);
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
    }

    @Override
    protected void onPause()
    {
        pauseCore();
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        resumeCore();
        super.onResume();

        // Hide the status bar
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
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

            case R.id.save_action:
                final boolean gba = isGbaMode();
                final String[] names = getResources().getStringArray(gba ? R.array.save_entries_gba : R.array.save_entries_nds);
                final int[] values = getResources().getIntArray(gba ? R.array.save_values_gba : R.array.save_values_nds);

                // Create the save type dialog
                AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setTitle("Change Save Type");

                builder.setItems(names, new DialogInterface.OnClickListener()
                {
                    @Override
                    public void onClick(DialogInterface dialog, final int which)
                    {
                        // Confirm the change because accidentally resizing a working save file could be bad!
                        AlertDialog.Builder builder2 = new AlertDialog.Builder(NooActivity.this);
                        builder2.setTitle("Changing Save Type");
                        builder2.setMessage("Are you sure? This may result in data loss!");
                        builder2.setNegativeButton("Cancel", null);

                        builder2.setPositiveButton("OK", new DialogInterface.OnClickListener()
                        {
                            @Override
                            public void onClick(DialogInterface dialog, int id)
                            {
                                // Apply the change and restart the core
                                pauseCore();
                                if (gba)
                                    resizeGbaSave(values[which]);
                                else
                                    resizeNdsSave(values[which]);
                                restartCore();
                                resumeCore();
                            }
                        });

                        builder2.create().show();
                    }
                });

                builder.create().show();
                return true;

            case R.id.restart_action:
                // Restart the core
                pauseCore();
                restartCore();
                resumeCore();
                return true;

            case R.id.bindings_action:
                // Open the input bindings menu
                startActivity(new Intent(this, BindingsMenu.class));
                return true;

            case R.id.settings_action:
                // Open the settings menu
                startActivity(new Intent(this, SettingsMenu.class));
                return true;

            case R.id.browser_action:
                // Go back to the file browser
                startActivity(new Intent(this, FileBrowser.class));
                finish();
                return true;
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
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        // Trigger a key press if a mapped key was pressed
        for (int i = 0; i < 12; i++)
        {
            if (keyCode == getKeyBind(i))
            {
                NooButton.pressKey(i);
                return true;
            }
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event)
    {
        // Trigger a key release if a mapped key was released
        for (int i = 0; i < 12; i++)
        {
            if (keyCode == getKeyBind(i))
            {
                NooButton.releaseKey(i);
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void onBackPressed()
    {
        // Simulate a menu button press to open the menu
        BaseInputConnection inputConnection = new BaseInputConnection(view, true);
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU));
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MENU));
    }

    private void pauseCore()
    {
        running = false;
        stopAudio();

        // Wait for the emulator to stop
        try
        {
            coreThread.join();
            saveThread.interrupt();
            saveThread.join();
            if (getShowFpsCounter() != 0)
            {
                fpsThread.interrupt();
                fpsThread.join();
            }
        }
        catch (Exception e)
        {
            // Oh well, I guess
        }

        view.onPause();
    }

    private void resumeCore()
    {
        running = true;
        startAudio();

        // Prepare the core thread
        coreThread = new Thread()
        {
            @Override
            public void run()
            {             
                while (running)
                    runFrame();
            }
        };

        coreThread.setPriority(Thread.MAX_PRIORITY);
        coreThread.start();

        // Prepare the save thread
        saveThread = new Thread()
        {
            @Override
            public void run()
            {
                while (running)
                {
                    // Check save files every few seconds and update them if changed
                    try
                    {
                        Thread.sleep(3000);
                    }
                    catch (Exception e)
                    {
                        // Oh well, I guess
                    }

                    writeSave();
                }
            }
        };

        saveThread.setPriority(Thread.MIN_PRIORITY);
        saveThread.start();

        if (getShowFpsCounter() != 0)
        {
            // Add the FPS counter to the layout if enabled
            if (!showingFps)
            {
                layout.addView(fpsCounter);
                showingFps = true;
            }

            // Prepare the FPS counter thread if enabled
            fpsThread = new Thread()
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

            fpsThread.setPriority(Thread.MIN_PRIORITY);
            fpsThread.start();
        }
        else if (showingFps)
        {
            // Remove the FPS counter from the layout if disabled
            layout.removeView(fpsCounter);
            showingFps = false;
        }

        // Resume rendering
        view.onResume();
    }

    public boolean isRunning() { return running; }

    public static native void startAudio();
    public static native void stopAudio();
    public static native int getKeyBind(int index);
    public static native int getShowFpsCounter();
    public static native int getFps();
    public static native boolean isGbaMode();
    public static native void runFrame();
    public static native void writeSave();
    public static native void restartCore();
    public static native void pressScreen(int x, int y);
    public static native void releaseScreen();
    public static native void resizeGbaSave(int size);
    public static native void resizeNdsSave(int size);
}
