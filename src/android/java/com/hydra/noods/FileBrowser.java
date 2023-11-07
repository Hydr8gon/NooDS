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
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.documentfile.provider.DocumentFile;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.TypedValue;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Stack;

public class FileBrowser extends AppCompatActivity
{
    static
    {
        // Load the native library
        System.loadLibrary("noods-core");
    }

    private static final boolean PLAY_STORE = false;

    private ArrayList<String> storagePaths;
    private ArrayList<String> fileNames;
    private ArrayList<Bitmap> fileIcons;
    private ArrayList<Uri> fileUris;
    private ListView fileView;

    private Stack<Uri> pathUris;
    private String path;
    private int curStorage;
    private int curDepth;
    private boolean scoped;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        // Storage paths can be checked without permissions; prepare them for the options menu
        updateStoragePaths();
        super.onCreate(savedInstanceState);

        // Load settings and request storage permissions based on environment
        if (!loadSettings(getExternalFilesDir(null).getPath()) && PLAY_STORE)
            showInfo(true);
        else if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q && PLAY_STORE)
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), 2);
        else if (checkPermissions())
            init();
        else
            requestPermissions();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        // Continue requesting storage permissions until they are given
        if (checkPermissions())
            init();
        else
            requestPermissions();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.file_menu, menu);

        // Only show the storage switcher if there are multiple storage paths
        if (storagePaths.size() > 1)
        {
            MenuItem item = menu.findItem(R.id.storage_action);
            item.setVisible(true);
        }

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        switch (item.getItemId())
        {
            case R.id.info_action:
                // Show the Play Store information dialog
                showInfo(false);
                break;

            case R.id.storage_action:
                // Switch to the next storage device
                if (scoped) break;
                curStorage = (curStorage + 1) % storagePaths.size();
                path = storagePaths.get(curStorage);
                curDepth = 0;
                update();
                break;

            case R.id.bindings_action:
                // Open the input bindings menu
                startActivity(new Intent(this, BindingsMenu.class));
                return true;

            case R.id.settings_action:
                // Open the settings menu
                startActivity(new Intent(this, SettingsMenu.class));
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed()
    {
        if (curDepth > 0)
        {
            // Navigate to the previous directory
            if (scoped)
                pathUris.pop();
            else
                path = path.substring(0, path.lastIndexOf('/'));
            curDepth--;
            update();
        }
        else
        {
            // On the top directory, close the app
            this.finishAffinity();
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent resultData)
    {
        switch (requestCode)
        {
            case 1: // Manage files permission
                // Fall back to scoped storage if permission wasn't granted
                if (checkPermissions())
                    init();
                else
                    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), 2);
                return;

            case 2: // Scoped directory selection
                // Set the initial path for scoped storage mode
                pathUris = new Stack<Uri>();
                pathUris.push(resultData.getData());
                scoped = true;
                init();
                return;
        }
    }

    private void showInfo(final boolean start)
    {
        // Create the Play Store information text
        TextView view = new TextView(this);
        float d = getResources().getDisplayMetrics().density;
        view.setTextColor(Color.BLACK);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        view.setPadding((int)(d * 25), (int)(d * 5), (int)(d * 25), (int)(d * 5));
        view.setSingleLine(false);
        view.setMovementMethod(LinkMovementMethod.getInstance());
        view.setText(Html.fromHtml("Thanks for using my emulator! This is the Google Play version of NooDS, which " +
            "is limited to scoped storage on Android 10 and above. When starting the app, you'll be asked to " +
            "choose a folder that contains DS or GBA ROMs. If you have trouble accessing files, try using " +
            "<a href=\"https://github.com/Hydr8gon/NooDS/releases\">the GitHub version</a> instead.<br><br>" +
            "My projects are free and open-source, but donations help me continue to work on them. If you're " +
            "feeling generous, here are some ways to support me: <a href=\"https://paypal.me/Hydr8gon\">one-time " +
            "via PayPal</a> or <a href=\"https://www.patreon.com/Hydr8gon\">monthly via Patreon</a>."));

        // Create the Play Store information dialog
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Welcome to NooDS");
        builder.setView(view);
        builder.setCancelable(!start);
        builder.setPositiveButton("OK", new DialogInterface.OnClickListener()
        {
            @Override
            public void onClick(DialogInterface dialog, int id)
            {
                // Request storage permissions if this was run on start
                if (!start)
                    return;
                else if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q)
                    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), 2);
                else if (checkPermissions())
                    init();
                else
                    requestPermissions();
            }
        }).create().show();
    }

    private boolean checkPermissions()
    {
        // Check for the manage files permission on Android 11 and up
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R)
            return Environment.isExternalStorageManager();

        // Check for legacy storage permissions on Android 10 and below
        int perm1 = ContextCompat.checkSelfPermission(this, android.Manifest.permission.READ_EXTERNAL_STORAGE);
        int perm2 = ContextCompat.checkSelfPermission(this, android.Manifest.permission.WRITE_EXTERNAL_STORAGE);
        return perm1 == PackageManager.PERMISSION_GRANTED && perm2 == PackageManager.PERMISSION_GRANTED;
    }

    private void requestPermissions()
    {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R)
        {
            // Request the manage files permission on Android 11 and up
            Uri uri = Uri.parse("package:" + BuildConfig.APPLICATION_ID);
            startActivityForResult(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, uri), 1);
        }
        else
        {
            // Request legacy storage permissions on Android 10 and below
            ActivityCompat.requestPermissions(this, new String[] {android.Manifest.permission.READ_EXTERNAL_STORAGE,
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, 0);
        }
    }

    private void updateStoragePaths()
    {
        // Fall back to the default storage directory pre-Lollipop
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP)
        {
            storagePaths = new ArrayList<String>();
            storagePaths.add(Environment.getExternalStorageDirectory().getAbsolutePath());
            return;
        }

        // There's no way to directly get root storage paths, but media paths contain them
        storagePaths = new ArrayList<String>();
        File[] mediaDirs = getExternalMediaDirs(); // Added in Lollipop
        String basePath = Environment.getExternalStorageDirectory().getAbsolutePath();
        String mediaPath = "";

        // Extract the media sub-path so it can be removed from all paths
        for (int i = 0; i < mediaDirs.length; i++)
        {
            if (mediaDirs[i].getAbsolutePath().contains(basePath))
            {
                mediaPath = mediaDirs[i].getAbsolutePath().substring(basePath.length());
                break;
            }
        }

        // Add all mounted storage paths to the list
        for (int i = 0; i < mediaDirs.length; i++)
        {
            if (Environment.getExternalStorageState(mediaDirs[i]).equals(Environment.MEDIA_MOUNTED))
                storagePaths.add(mediaDirs[i].getAbsolutePath().replace(mediaPath, ""));
        }
    }

    private void init()
    {
        fileNames = new ArrayList<String>();
        fileIcons = new ArrayList<Bitmap>();
        fileUris = new ArrayList<Uri>();
        fileView = new ListView(this);
        path = storagePaths.get(0);
        curStorage = 0;
        curDepth = 0;

        fileView.setOnItemClickListener(new AdapterView.OnItemClickListener()
        {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id)
            {
                // Navigate to the selected directory
                if (scoped)
                    pathUris.push(fileUris.get(position));
                else
                    path += "/" + fileNames.get(position);
                curDepth++;
                update();
            }
        });

        // Set a custom launch path if one is specified
        Bundle extras = getIntent().getExtras();
        if (extras != null)
        {
            String launchPath = extras.getString("LaunchPath");
            if (launchPath != null)
                path = launchPath;
        }

        setContentView(fileView);
        update();
    }

    private void tryStartCore()
    {
        // Attempt to start the core
        int result = startCore();

        if (result == 0)
        {
            // Switch to the emulator activity if loading was successful
            startActivity(new Intent(this, NooActivity.class));
            finish();
        }
        else
        {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setPositiveButton("OK", null);

            // Inform the user of the error if loading wasn't successful
            switch (result)
            {
                case 1: // Missing BIOS files
                    builder.setTitle("Error Loading BIOS");
                    builder.setMessage("Make sure the path settings point to valid BIOS files and try again. " +
                                       "You can modify path settings in " + getExternalFilesDir(null).getPath() + "/noods.ini.");
                    break;

                case 2: // Non-bootable firmware file
                    builder.setTitle("Error Loading Firmware");
                    builder.setMessage("Make sure the path settings point to a bootable firmware file or try another boot method. " +
                                       "You can modify path settings in " + getExternalFilesDir(null).getPath() + "/noods.ini.");
                    break;

                case 3: // Unreadable ROM file
                    builder.setTitle("Error Loading ROM");
                    builder.setMessage("Make sure the ROM file is accessible and try again.");
                    break;
            }

            builder.create().show();
            onBackPressed();
        }
    }

    private void update()
    {
        DocumentFile file = scoped ? DocumentFile.fromTreeUri(this, pathUris.peek()) : DocumentFile.fromFile(new File(path));
        String ext = (file.getName().length() >= 4) ? file.getName().substring(file.getName().length() - 4) : "";

        // Check if the current file is a ROM
        if (!file.isDirectory() && (ext.equals(".nds") || ext.equals(".gba")))
        {
            // Set the NDS or GBA ROM path depending on the file extension, and try to start the core
            // If a ROM of the other type is already loaded, ask if it should be loaded alongside the new ROM
            if (ext.equals(".nds"))
            {
                setNdsPath(getRomPath(file));
                setNdsSave(getRomSave(file));

                if (!getGbaPath().equals(""))
                {
                    AlertDialog.Builder builder = new AlertDialog.Builder(this);
                    builder.setTitle("Loading NDS ROM");
                    builder.setMessage("Load the previous GBA ROM alongside this ROM?");

                    builder.setPositiveButton("Yes", new DialogInterface.OnClickListener()
                    {
                        @Override
                        public void onClick(DialogInterface dialog, int id)
                        {
                            tryStartCore();
                        }
                    });

                    builder.setNegativeButton("No", new DialogInterface.OnClickListener()
                    {
                        @Override
                        public void onClick(DialogInterface dialog, int id)
                        {
                            setGbaPath("");
                            tryStartCore();
                        }
                    });

                    builder.setOnCancelListener(new DialogInterface.OnCancelListener()
                    {
                        @Override
                        public void onCancel(DialogInterface dialog)
                        {
                            setGbaPath("");
                            tryStartCore();
                        }
                    });

                    builder.create().show();
                    return;
                }
            }
            else
            {
                setGbaPath(getRomPath(file));
                setGbaSave(getRomSave(file));

                if (!getNdsPath().equals(""))
                {
                    AlertDialog.Builder builder = new AlertDialog.Builder(this);
                    builder.setTitle("Loading GBA ROM");
                    builder.setMessage("Load the previous NDS ROM alongside this ROM?");

                    builder.setPositiveButton("Yes", new DialogInterface.OnClickListener()
                    {
                        @Override
                        public void onClick(DialogInterface dialog, int id)
                        {
                            tryStartCore();
                        }
                    });

                    builder.setNegativeButton("No", new DialogInterface.OnClickListener()
                    {
                        @Override
                        public void onClick(DialogInterface dialog, int id)
                        {
                            setNdsPath("");
                            tryStartCore();
                        }
                    });

                    builder.setOnCancelListener(new DialogInterface.OnCancelListener()
                    {
                        @Override
                        public void onCancel(DialogInterface dialog)
                        {
                            setNdsPath("");
                            tryStartCore();
                        }
                    });

                    builder.create().show();
                    return;
                }
            }

            tryStartCore();
            return;
        }

        // Sort all files in the current directory
        DocumentFile[] files = file.listFiles();
        Arrays.sort(files, new Comparator<DocumentFile>()
        {
            public int compare(DocumentFile f1, DocumentFile f2)
            {
                return f1.getName().compareTo(f2.getName());
            }
        });

        fileNames.clear();
        fileIcons.clear();
        fileUris.clear();

        // Get file names and icons for all folders and ROMs at the current path
        for (int i = 0; i < files.length; i++)
        {
            if (files[i].isDirectory())
            {
                fileNames.add(files[i].getName());
                fileIcons.add(BitmapFactory.decodeResource(getResources(), R.drawable.folder));
                fileUris.add(files[i].getUri());
            }
            else
            {
                ext = (files[i].getName().length() >= 4) ? files[i].getName().substring(files[i].getName().length() - 4) : "";
                if (ext.equals(".nds"))
                {
                    fileNames.add(files[i].getName());
                    Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    getNdsIcon(getRomPath(files[i]), bitmap);
                    fileIcons.add(bitmap);
                    fileUris.add(files[i].getUri());
                }
                else if (ext.equals(".gba"))
                {
                    fileNames.add(files[i].getName());
                    fileIcons.add(BitmapFactory.decodeResource(getResources(), R.drawable.file));
                    fileUris.add(files[i].getUri());
                }
            }
        }

        fileView.setAdapter(new FileAdapter(this, fileNames, fileIcons));
    }

    private String getRomPath(DocumentFile file)
    {
        // Get the raw file path in non-scoped mode
        if (!scoped)
            return file.getUri().getPath();

        try
        {
            // Get a descriptor for the file in scoped mode
            ParcelFileDescriptor desc = getContentResolver().openFileDescriptor(file.getUri(), "r");
            return "/proc/self/fd/" + desc.detachFd();
        }
        catch (Exception e)
        {
            // Oh well, I guess
            return "";
        }
    }

    private String getRomSave(DocumentFile file)
    {
        // Let the core handle saves in non-scoped mode
        if (!scoped)
            return "";

        // Make a save file URI based on the ROM file URI
        String str = file.getUri().toString();
        Uri uri = Uri.parse(str.substring(0, str.length() - 4) + ".sav");

        try
        {
            // Get a descriptor for the file in scoped mode
            ParcelFileDescriptor desc = getContentResolver().openFileDescriptor(uri, "r");
            return "/proc/self/fd/" + desc.detachFd();
        }
        catch (Exception e)
        {
            // Create a new save file if one doesn't exist
            String str2 = file.getName().toString();
            DocumentFile file2 = DocumentFile.fromTreeUri(this, pathUris.get(pathUris.size() - 2));
            file2.createFile("application/sav", str2.substring(0, str2.length() - 4) + ".sav");

            try
            {
                // Give the save an invalid size so the core treats it as new
                OutputStream out = getContentResolver().openOutputStream(uri);
                out.write(0xFF);
                out.close();
                return getRomSave(file);
            }
            catch (Exception e2)
            {
                // Oh well, I guess
                return "";
            }
        }
    }

    public static native boolean loadSettings(String rootPath);
    public static native void getNdsIcon(String romPath, Bitmap bitmap);
    public static native int startCore();
    public static native String getNdsPath();
    public static native String getGbaPath();
    public static native void setNdsPath(String value);
    public static native void setGbaPath(String value);
    public static native void setNdsSave(String value);
    public static native void setGbaSave(String value);
}
