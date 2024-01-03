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
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreferenceCompat;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;

public class SettingsMenu extends AppCompatActivity
{
    private SharedPreferences prefs;
    private SettingsFragment fragment;

    public static class SettingsFragment extends PreferenceFragmentCompat
    {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
        {
            // Load settings from the XML file
            setPreferencesFromResource(R.xml.settings, rootKey);

            // Request the microphone permission if not granted when the setting is enabled
            findPreference("mic_enable").setOnPreferenceClickListener(new Preference.OnPreferenceClickListener()
            {
                @Override
                public boolean onPreferenceClick(Preference pref)
                {
                    if (!((SwitchPreferenceCompat)pref).isChecked()) return true;
                    String perm = android.Manifest.permission.RECORD_AUDIO;
                    if (ContextCompat.checkSelfPermission(getActivity(), perm) != PackageManager.PERMISSION_GRANTED)
                        ActivityCompat.requestPermissions(getActivity(), new String[] {perm}, 0);
                    return true;
                }
            });
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        // Disable the microphone setting if permission wasn't granted
        int perm = ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO);
        if (perm != PackageManager.PERMISSION_GRANTED)
            ((SwitchPreferenceCompat)fragment.findPreference("mic_enable")).setChecked(false);
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
        editor.putBoolean("high_res_3d", (getHighRes3D() == 0) ? false : true);
        editor.putString("screen_position", Integer.toString(getScreenPosition()));
        editor.putString("screen_rotation", Integer.toString(getScreenRotation()));
        editor.putString("screen_arrangement", Integer.toString(getScreenArrangement()));
        editor.putString("screen_sizing", Integer.toString(getScreenSizing()));
        editor.putString("screen_gap", Integer.toString(getScreenGap()));
        editor.putBoolean("integer_scale", (getIntegerScale() == 0) ? false : true);
        editor.putBoolean("gba_crop", (getGbaCrop() == 0) ? false : true);
        editor.putBoolean("screen_filter", (getScreenFilter() == 0) ? false : true);
        editor.putBoolean("mic_enable", (getMicEnable() == 0) ? false : true);
        editor.putBoolean("show_fps_counter", (getShowFpsCounter() == 0) ? false : true);
        editor.putInt("button_scale", getButtonScale());
        editor.putInt("button_spacing", getButtonSpacing());
        editor.putInt("vibrate_strength", getVibrateStrength());
        editor.commit();

        // Populate the settings UI
        fragment = new SettingsFragment();
        getSupportFragmentManager().beginTransaction()
            .add(android.R.id.content, fragment)
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
        setHighRes3D(prefs.getBoolean("high_res_3d", true) ? 1 : 0);
        setScreenPosition(Integer.parseInt(prefs.getString("screen_position", "0")));
        setScreenRotation(Integer.parseInt(prefs.getString("screen_rotation", "0")));
        setScreenArrangement(Integer.parseInt(prefs.getString("screen_arrangement", "0")));
        setScreenSizing(Integer.parseInt(prefs.getString("screen_sizing", "0")));
        setScreenGap(Integer.parseInt(prefs.getString("screen_gap", "0")));
        setIntegerScale(prefs.getBoolean("integer_scale", false) ? 1 : 0);
        setGbaCrop(prefs.getBoolean("gba_crop", true) ? 1 : 0);
        setScreenFilter(prefs.getBoolean("screen_filter", true) ? 1 : 0);
        setMicEnable(prefs.getBoolean("mic_enable", false) ? 1 : 0);
        setShowFpsCounter(prefs.getBoolean("show_fps_counter", false) ? 1 : 0);
        setButtonScale(prefs.getInt("button_scale", 5));
        setButtonSpacing(prefs.getInt("button_spacing", 10));
        setVibrateStrength(prefs.getInt("vibrate_strength", 1));
        saveSettings();

        // Return to the previous activity
        finish();
    }

    public static native int getDirectBoot();
    public static native int getFpsLimiter();
    public static native int getThreaded2D();
    public static native int getThreaded3D();
    public static native int getHighRes3D();
    public static native int getScreenPosition();
    public static native int getScreenRotation();
    public static native int getScreenArrangement();
    public static native int getScreenSizing();
    public static native int getScreenGap();
    public static native int getIntegerScale();
    public static native int getGbaCrop();
    public static native int getScreenFilter();
    public static native int getMicEnable();
    public static native int getShowFpsCounter();
    public static native int getButtonScale();
    public static native int getButtonSpacing();
    public static native int getVibrateStrength();
    public static native void setDirectBoot(int value);
    public static native void setFpsLimiter(int value);
    public static native void setThreaded2D(int value);
    public static native void setThreaded3D(int value);
    public static native void setHighRes3D(int value);
    public static native void setScreenPosition(int value);
    public static native void setScreenRotation(int value);
    public static native void setScreenArrangement(int value);
    public static native void setScreenSizing(int value);
    public static native void setScreenGap(int value);
    public static native void setIntegerScale(int value);
    public static native void setGbaCrop(int value);
    public static native void setScreenFilter(int value);
    public static native void setMicEnable(int value);
    public static native void setShowFpsCounter(int value);
    public static native void setButtonScale(int value);
    public static native void setButtonSpacing(int value);
    public static native void setVibrateStrength(int value);
    public static native void saveSettings();
}
