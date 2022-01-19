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
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

public class SettingsMenu extends AppCompatActivity
{
    private SharedPreferences prefs;

    public static class SettingsFragment extends PreferenceFragmentCompat
    {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
        {
            setPreferencesFromResource(R.xml.settings, rootKey);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Load the settings from the core
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("direct_boot", (getDirectBoot() == 0) ? false : true);
        editor.putString("fps_limiter", Integer.toString(getFpsLimiter()));
        editor.putBoolean("threaded_2d", (getThreaded2D() == 0) ? false : true);
        editor.putString("threaded_3d", Integer.toString(getThreaded3D()));
        editor.putString("screen_rotation", Integer.toString(getScreenRotation()));
        editor.putString("screen_arrangement", Integer.toString(getScreenArrangement()));
        editor.putString("screen_sizing", Integer.toString(getScreenSizing()));
        editor.putString("screen_gap", Integer.toString(getScreenGap()));
        editor.putBoolean("integer_scale", (getIntegerScale() == 0) ? false : true);
        editor.putBoolean("gba_crop", (getGbaCrop() == 0) ? false : true);
        editor.putBoolean("screen_filter", (getScreenFilter() == 0) ? false : true);
        editor.putBoolean("show_fps_counter", (getShowFpsCounter() == 0) ? false : true);
        editor.commit();

        getSupportFragmentManager().beginTransaction()
            .add(android.R.id.content, new SettingsFragment())
            .commit();
    }

    @Override
    public void onBackPressed()
    {
        // Save the settings to the core
        setDirectBoot(prefs.getBoolean("direct_boot", true) ? 1 : 0);
        setFpsLimiter(Integer.parseInt(prefs.getString("fps_limiter", "1")));
        setThreaded2D(prefs.getBoolean("threaded_2d", true) ? 1 : 0);
        setThreaded3D(Integer.parseInt(prefs.getString("threaded_3d", "1")));
        setScreenRotation(Integer.parseInt(prefs.getString("screen_rotation", "0")));
        setScreenArrangement(Integer.parseInt(prefs.getString("screen_arrangement", "0")));
        setScreenSizing(Integer.parseInt(prefs.getString("screen_sizing", "0")));
        setScreenGap(Integer.parseInt(prefs.getString("screen_gap", "0")));
        setIntegerScale(prefs.getBoolean("integer_scale", false) ? 1 : 0);
        setGbaCrop(prefs.getBoolean("gba_crop", true) ? 1 : 0);
        setScreenFilter(prefs.getBoolean("screen_filter", true) ? 1 : 0);
        setShowFpsCounter(prefs.getBoolean("show_fps_counter", false) ? 1 : 0);
        saveSettings();

        // Return to the file browser
        finish();
    }

    public native int getDirectBoot();
    public native int getFpsLimiter();
    public native int getThreaded2D();
    public native int getThreaded3D();
    public native int getScreenRotation();
    public native int getScreenArrangement();
    public native int getScreenSizing();
    public native int getScreenGap();
    public native int getIntegerScale();
    public native int getGbaCrop();
    public native int getScreenFilter();
    public native int getShowFpsCounter();
    public native void setDirectBoot(int value);
    public native void setFpsLimiter(int value);
    public native void setThreaded2D(int value);
    public native void setThreaded3D(int value);
    public native void setScreenRotation(int value);
    public native void setScreenArrangement(int value);
    public native void setScreenSizing(int value);
    public native void setScreenGap(int value);
    public native void setIntegerScale(int value);
    public native void setGbaCrop(int value);
    public native void setScreenFilter(int value);
    public native void setShowFpsCounter(int value);
    public native void saveSettings();
}
