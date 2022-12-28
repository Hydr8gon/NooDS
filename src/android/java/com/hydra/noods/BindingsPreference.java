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

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.view.KeyEvent;

public class BindingsPreference extends Preference
{
    private Context context;
    private int index;

    public BindingsPreference(Context context, AttributeSet attrs)
    {
        super(context, attrs);
        this.context = context;
        index = Integer.parseInt(attrs.getAttributeValue(null, "index"));

        // Set the subtext based on the binding for the key index
        int value = getKeyBind(index);
        if (value == 0)
            setSummary("None");
        else
            setSummary("Input " + Integer.toString(value - 1));
    }

    @Override
    protected void onClick()
    {
        // Create the dialog for rebinding an input
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(getTitle());
        builder.setMessage("Press a key to bind it to this input.");
        builder.setNegativeButton("Cancel", null);

        builder.setPositiveButton("Clear", new DialogInterface.OnClickListener()
        {
            @Override
            public void onClick(DialogInterface dialog, int id)
            {
                // Clear the binding for the key index
                setKeyBind(index, 0);
                setSummary("None");
            }
        });

       builder.setOnKeyListener(new DialogInterface.OnKeyListener()
       {
            @Override
            public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event)
            {
                // Set the binding for the key index
                setKeyBind(index, keyCode + 1);
                setSummary("Input " + Integer.toString(keyCode));
                dialog.dismiss();
                return true;
            }
        });

        builder.create().show();
    }

    public static native int getKeyBind(int index);
    public static native void setKeyBind(int index, int value);
}
