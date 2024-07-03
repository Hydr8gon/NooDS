/*
    Copyright 2019-2024 Hydr8gon

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
import androidx.preference.PreferenceManager;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
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
import java.util.Collections;
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
    private ArrayList<FileAdapter.FileInfo> fileInfo;
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
        else if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R && PLAY_STORE)
            openScoped();
        else if (checkPermissions())
            initialize();
        else
            requestPermissions();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        // Continue requesting storage permissions until they are given
        if (checkPermissions())
            initialize();
        else
            requestPermissions();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        // Only show the storage switcher if there are multiple storage paths
        getMenuInflater().inflate(R.menu.file_menu, menu);
        if (storagePaths.size() > 1 || scoped)
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
                // Open the scoped storage selector in scoped mode
                if (scoped)
                {
                    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), 2);
                    break;
                }

                // Switch to the next storage device
                curStorage = (curStorage + 1) % storagePaths.size();
                path = storagePaths.get(curStorage);
                curDepth = 0;
                update();
                break;

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
                    initialize();
                else
                    openScoped();
                return;

            case 2: // Scoped directory selection
                // Save the returned URI with persistent permissions so it can be restored next time
                SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(this).edit();
                int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                getContentResolver().takePersistableUriPermission(resultData.getData(), flags);
                editor.putString("scoped_uri", resultData.getData().toString());
                editor.commit();

                // Initialize for scoped storage mode
                pathUris = new Stack<Uri>();
                pathUris.push(resultData.getData());
                scoped = true;
                initialize();
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
            "is limited to scoped storage on Android 11 and above. You can select a folder that the app will have " +
            "access to; it should contain DS or GBA ROMs. If you have trouble accessing files, try using " +
            "<a href=\"https://github.com/Hydr8gon/NooDS/releases\">the GitHub version</a> instead.<br><br>" +
            "My projects are free and open-source, but donations help me continue to work on them. If you're " +
            "feeling generous, here are some ways to support me: <a href=\"https://paypal.me/Hydr8gon\">one-time " +
            "via PayPal</a> or <a href=\"https://www.patreon.com/Hydr8gon\">monthly via Patreon</a>."));

        // Create the Play Store information dialog
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.welcome_to_noods);
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
                else if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R)
                    openScoped();
                else if (checkPermissions())
                    initialize();
                else
                    requestPermissions();
            }
        }).create().show();
    }

    private void openScoped()
    {
        // Try to restore the previous URI and initialize for scoped storage mode
        try
        {
            Uri scopedUri = Uri.parse(PreferenceManager.getDefaultSharedPreferences(this).getString("scoped_uri", ""));
            if (DocumentFile.fromTreeUri(this, scopedUri).isDirectory())
            {
                pathUris = new Stack<Uri>();
                pathUris.push(scopedUri);
                scoped = true;
                initialize();
                return;
            }
        }
        catch (Exception e) {}

        // Open the scoped storage selector if restoring failed
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), 2);
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

    private void initialize()
    {
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
                    pathUris.push(fileInfo.get(position).uri);
                else
                    path += "/" + fileInfo.get(position).name;
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
                    builder.setTitle(R.string.error_bios_1);
                    builder.setMessage(getString(R.string.error_bios_2) +
                                       getString(R.string.error_bios_3) + getExternalFilesDir(null).getPath() + "/noods.ini.");
                    break;

                case 2: // Non-bootable firmware file
                    builder.setTitle(R.string.error_firmware_1);
                    builder.setMessage(getString(R.string.error_firmware_2) +
                                       getString(R.string.error_firmware_3) + getExternalFilesDir(null).getPath() + "/noods.ini.");
                    break;

                case 3: // Unreadable ROM file
                    builder.setTitle(R.string.error_rom_1);
                    builder.setMessage(R.string.error_rom_2);
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
                if (scoped)
                    setNdsFds(getRomFd(file.getUri()), getSaveFd(file), getStateFd(file));
                else
                    setNdsPath(file.getUri().getPath());

                if (isGbaLoaded())
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
                            setGbaFds(-1, -1, -1);
                            tryStartCore();
                        }
                    });

                    builder.setOnCancelListener(new DialogInterface.OnCancelListener()
                    {
                        @Override
                        public void onCancel(DialogInterface dialog)
                        {
                            setGbaPath("");
                            setGbaFds(-1, -1, -1);
                            tryStartCore();
                        }
                    });

                    builder.create().show();
                    return;
                }
            }
            else
            {
                if (scoped)
                    setGbaFds(getRomFd(file.getUri()), getSaveFd(file), getStateFd(file));
                else
                    setGbaPath(file.getUri().getPath());

                if (isNdsLoaded())
                {
                    AlertDialog.Builder builder = new AlertDialog.Builder(this);
                    builder.setTitle(R.string.loading_gba_rom);
                    builder.setMessage(R.string.load_the_previous_nds_rom);

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
                            setNdsFds(-1, -1, -1);
                            tryStartCore();
                        }
                    });

                    builder.setOnCancelListener(new DialogInterface.OnCancelListener()
                    {
                        @Override
                        public void onCancel(DialogInterface dialog)
                        {
                            setNdsPath("");
                            setNdsFds(-1, -1, -1);
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

        FileAdapter fileAdapter = new FileAdapter(this);
        fileInfo = new ArrayList<FileAdapter.FileInfo>();

        if (scoped)
        {
            // Populate the file list in a way optimized for scoped storage
            Uri uri = file.getUri();
            String docId = DocumentsContract.getDocumentId(uri);
            Uri childUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, docId);
            String[] fields = new String[] {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE};
            Cursor cursor = getContentResolver().query(childUri, fields, null, null, null);
            while (cursor.moveToNext())
            {
                FileAdapter.FileInfo info = fileAdapter.new FileInfo();
                info.name = cursor.getString(1);
                info.uri = DocumentsContract.buildDocumentUriUsingTree(uri, cursor.getString(0));
                boolean directory = cursor.getString(2).equals(DocumentsContract.Document.MIME_TYPE_DIR);
                processFile(info, directory);
            }
        }
        else
        {
            // Populate the file list normally
            DocumentFile[] files = file.listFiles();
            for (int i = 0; i < files.length; i++)
            {
                FileAdapter.FileInfo info = fileAdapter.new FileInfo();
                info.name = files[i].getName();
                info.uri = files[i].getUri();
                boolean directory = files[i].isDirectory();
                processFile(info, directory);
            }
        }

        // Sort the file list by name
        Collections.sort(fileInfo, new Comparator<FileAdapter.FileInfo>()
        {
            public int compare(FileAdapter.FileInfo f1, FileAdapter.FileInfo f2)
            {
                return f1.name.compareTo(f2.name);
            }
        });

        fileAdapter.setInfo(fileInfo);
        fileView.setAdapter(fileAdapter);
    }

    private void processFile(FileAdapter.FileInfo info, boolean directory)
    {
        // Add a directory with icon to the file list
        if (directory)
        {
            info.icon = BitmapFactory.decodeResource(getResources(), R.drawable.folder);
            fileInfo.add(info);
            return;
        }

        // Add a DS or GBA ROM with icon to the file list
        String ext = (info.name.length() >= 4) ? info.name.substring(info.name.length() - 4) : "";
        if (ext.equals(".nds"))
        {
            Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
            getNdsIcon(getRomFd(info.uri), bitmap);
            info.icon = bitmap;
            fileInfo.add(info);
        }
        else if (ext.equals(".gba"))
        {
            info.icon = BitmapFactory.decodeResource(getResources(), R.drawable.file);
            fileInfo.add(info);
        }
    }

    private int getRomFd(Uri romUri)
    {
        try
        {
            // Get a descriptor for the file in scoped mode
            return getContentResolver().openFileDescriptor(romUri, "r").detachFd();
        }
        catch (Exception e)
        {
            // Oh well, I guess
            return -1;
        }
    }

    private int getSaveFd(DocumentFile rom)
    {
        // Make a save file URI based on the ROM file URI
        String str = rom.getUri().toString();
        Uri uri = Uri.parse(str.substring(0, str.length() - 4) + ".sav");

        try
        {
            // Get a descriptor for the file in scoped mode
            return getContentResolver().openFileDescriptor(uri, "rw").detachFd();
        }
        catch (Exception e)
        {
            // Create a new save file if one doesn't exist
            str = rom.getName().toString();
            DocumentFile save = DocumentFile.fromTreeUri(this, pathUris.get(pathUris.size() - 2));
            save.createFile("application/sav", str.substring(0, str.length() - 4) + ".sav");

            try
            {
                // Give the save an invalid size so the core treats it as new
                OutputStream out = getContentResolver().openOutputStream(uri);
                out.write(0xFF);
                out.close();
                return getSaveFd(rom);
            }
            catch (Exception e2)
            {
                // Oh well, I guess
                return -1;
            }
        }
    }

    private int getStateFd(DocumentFile rom)
    {
        // Make a state file URI based on the ROM file URI
        String str = rom.getUri().toString();
        Uri uri = Uri.parse(str.substring(0, str.length() - 4) + ".noo");

        try
        {
            // Get a descriptor for the file in scoped mode
            return getContentResolver().openFileDescriptor(uri, "rw").detachFd();
        }
        catch (Exception e)
        {
            // Create a new state file if one doesn't exist
            str = rom.getName().toString();
            DocumentFile save = DocumentFile.fromTreeUri(this, pathUris.get(pathUris.size() - 2));
            save.createFile("application/sav", str.substring(0, str.length() - 4) + ".noo");
            return getStateFd(rom);
        }
    }

    public static native boolean loadSettings(String rootPath);
    public static native void getNdsIcon(int fd, Bitmap bitmap);
    public static native int startCore();
    public static native boolean isNdsLoaded();
    public static native boolean isGbaLoaded();
    public static native void setNdsPath(String value);
    public static native void setGbaPath(String value);
    public static native void setNdsFds(int romFd, int saveFd, int stateFd);
    public static native void setGbaFds(int romFd, int saveFd, int stateFd);
}
