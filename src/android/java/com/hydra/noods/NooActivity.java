/*
    Copyright 2019-2023 Hydr8gon

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
import androidx.core.content.ContextCompat;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
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
    private boolean initButtons = true;
    private boolean showingButtons = true;
    private boolean showingFps;

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
        setContentView(layout);
        layout.addView(view);

        // Create the FPS counter
        fpsCounter.setTextSize(24);
        fpsCounter.setTextColor(Color.WHITE);

        // Add the FPS counter to the layout if enabled
        if (showingFps = (SettingsMenu.getShowFpsCounter() != 0))
            layout.addView(fpsCounter);
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

        // Hide the status and navigation bars
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
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
                    for (int i = 0; i < 6; i++)
                        layout.addView(buttons[i]);
                }
                else
                {
                    for (int i = 0; i < 6; i++)
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

        // Rehide the status and navigation bars
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        // Trigger a key press if a mapped key was pressed
        for (int i = 0; i < 12; i++)
        {
            if (keyCode == BindingsPreference.getKeyBind(i) - 1)
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
            if (keyCode == BindingsPreference.getKeyBind(i) - 1)
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

    public void updateButtons()
    {
        // Remove old buttons from the layout if visible
        if (!initButtons && showingButtons)
        {
            for (int i = 0; i < 6; i++)
                layout.removeView(buttons[i]);
        }

        // Set layout parameters based on display and settings
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        float d = metrics.density * (SettingsMenu.getButtonScale() + 10) / 15;
        int w = renderer.width, h = renderer.height;
        int b = Math.min(((w > h) ? w : h) * (SettingsMenu.getButtonSpacing() + 10) / 40, h);

        // Create D-pad and ABXY buttons, placed 2/3rds down the virtual controller
        int x = Math.min(h - b / 3 - (int)(d * 82.5), h - (int)(d * 170));
        buttons[0] = new NooButton(this, NooButton.resAbxy, new int[] {0, 11, 10, 1},
            w - (int)(d * 170), x, (int)(d * 165), (int)(d * 165));
        buttons[1] = new NooButton(this, NooButton.resDpad, new int[] {4, 5, 6, 7},
            (int)(d * 21.5), x + (int)(d * 16.5), (int)(d * 132), (int)(d * 132));

        // Create L and R buttons, placed in the top corners of the virtual controller
        int y = Math.min(h - b + (int)(d * 5), x - (int)(d * 49));
        buttons[2] = new NooButton(this, new int[] {R.drawable.l, R.drawable.l_pressed},
            new int[] {9}, (int)(d * 5), y, (int)(d * 110), (int)(d * 44));
        buttons[3] = new NooButton(this, new int[] {R.drawable.r, R.drawable.r_pressed},
            new int[] {8}, w - (int)(d * 115), y, (int)(d * 110), (int)(d * 44));

        // Create start and select buttons, placed along the bottom of the virtual controller
        int z = Math.min(h - y, w / 2) - (int)(d * 49.5);
        buttons[4] = new NooButton(this, new int[] {R.drawable.start, R.drawable.start_pressed},
            new int[] {3}, w - z - (int)(d * 33), h - (int)(d * 38), (int)(d * 33), (int)(d * 33));
        buttons[5] = new NooButton(this, new int[] {R.drawable.select, R.drawable.select_pressed},
            new int[] {2}, z, h - (int)(d * 38), (int)(d * 33), (int)(d * 33));

        // Add new buttons to the layout if visible
        if (initButtons || showingButtons)
        {
            for (int i = 0; i < 6; i++)
                layout.addView(buttons[i]);
            initButtons = false;
        }
    }

    private boolean canEnableMic()
    {
        // Check if the microphone is enabled and permission has been granted
        int perm = ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO);
        return perm == PackageManager.PERMISSION_GRANTED && SettingsMenu.getMicEnable() == 1;
    }

    private void pauseCore()
    {
        // Stop audio output and input if enabled
        running = false;
        if (canEnableMic())
            stopAudioRecorder();
        stopAudioPlayer();

        // Wait for the emulator to stop
        try
        {
            coreThread.join();
            saveThread.interrupt();
            saveThread.join();
            if (SettingsMenu.getShowFpsCounter() != 0)
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
        // Start audio output and input if enabled
        running = true;
        startAudioPlayer();
        if (canEnableMic())
            startAudioRecorder();

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

        if (SettingsMenu.getShowFpsCounter() != 0)
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

    public static native void startAudioPlayer();
    public static native void startAudioRecorder();
    public static native void stopAudioPlayer();
    public static native void stopAudioRecorder();
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
