package com.hydra.noods;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.os.Environment;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

public class FileBrowser extends AppCompatActivity
{
    static
    {
        // Load the native library
        System.loadLibrary("noods-core");
    }

    private ArrayList<String> fileNames;
    private ArrayList<Bitmap> fileIcons;
    private ListView fileView;
    private String path;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Request storage permissions if they haven't been given
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED)
            ActivityCompat.requestPermissions(this, new String[] { android.Manifest.permission.READ_EXTERNAL_STORAGE, android.Manifest.permission.WRITE_EXTERNAL_STORAGE }, 0);
        else
            init();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        // Continue requesting storage permissions until they are given
        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED)
            init();
        else
            ActivityCompat.requestPermissions(this, new String[] { android.Manifest.permission.READ_EXTERNAL_STORAGE, android.Manifest.permission.WRITE_EXTERNAL_STORAGE }, 0);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.file_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        switch (item.getItemId())
        {
            case R.id.settings_action:
            {
                // Open the settings menu
                startActivity(new Intent(this, SettingsMenu.class));
                return true;
            }
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed()
    {
        // Navigate to the previous directory
        if (!path.equals(Environment.getExternalStorageDirectory().getAbsolutePath()))
        {
            path = path.substring(0, path.lastIndexOf('/'));
            update();
        }
    }

    private void init()
    {
        fileNames = new ArrayList<String>();
        fileIcons = new ArrayList<Bitmap>();
        fileView = new ListView(this);
        path = Environment.getExternalStorageDirectory().getAbsolutePath();

        fileView.setOnItemClickListener(new AdapterView.OnItemClickListener()
        {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id)
            {
                // Navigate to the selected directory
                path += "/" + fileNames.get(position);
                update();
            }
        });

        // Load the settings, creating the emulator directory if it doesn't exist
        File dir = new File(path + "/noods");
        if (!dir.exists()) dir.mkdir();
        loadSettings(path);

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

            // Inform the user of the error if loading was not successful
            if (result == 1) // Missing BIOS and/or firmware files
            {
                builder.setTitle("Missing BIOS/Firmware");
                builder.setMessage("Initialization failed. " +
                                   "Make sure the path settings point to valid BIOS and firmware files and try again. " +
                                   "You can modify the path settings in the noods.ini file.");
            }
            else // Unreadable ROM file
            {
                builder.setTitle("Unreadable ROM");
                builder.setMessage("Initialization failed. " +
                                   "Make sure the ROM file is accessible and try again.");
            }

            builder.create().show();
            onBackPressed();
        }
    }

    private void update()
    {
        File file = new File(path);
        String ext = (file.getName().length() >= 4) ? file.getName().substring(file.getName().length() - 4) : "";

        // Check if the current file is a ROM
        if (!file.isDirectory() && (ext.equals(".nds") || ext.equals(".gba")))
        {
            // Set the NDS or GBA ROM path depending on the file extension, and try to start the core
            // If a ROM of the other type is already loaded, ask if it should be loaded alongside the new ROM
            if (ext.equals(".nds"))
            {
                setNdsPath(path);

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
                setGbaPath(path);

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

        // Get all files in the current directory
        File[] files = file.listFiles();
        Arrays.sort(files);

        fileNames.clear();
        fileIcons.clear();

        // Get file names and icons for all folders and ROMs at the current path
        for (int i = 0; i < files.length; i++)
        {
            if (files[i].isDirectory())
            {
                fileNames.add(files[i].getName());
                fileIcons.add(BitmapFactory.decodeResource(getResources(), R.drawable.folder));
            }
            else
            {
                ext = (files[i].getName().length() >= 4) ? files[i].getName().substring(files[i].getName().length() - 4) : "";
                if (ext.equals(".nds"))
                {
                    fileNames.add(files[i].getName());
                    Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    getNdsIcon(path + "/" + files[i].getName(), bitmap);
                    fileIcons.add(bitmap);
                }
                else if (ext.equals(".gba"))
                {
                    fileNames.add(files[i].getName());
                    fileIcons.add(BitmapFactory.decodeResource(getResources(), R.drawable.file));
                }
            }
        }

        fileView.setAdapter(new FileAdapter(this, fileNames, fileIcons));
    }

    public native void loadSettings(String rootPath);
    public native void getNdsIcon(String romPath, Bitmap bitmap);
    public native int startCore();
    public native String getNdsPath();
    public native String getGbaPath();
    public native void setNdsPath(String value);
    public native void setGbaPath(String value);
}
