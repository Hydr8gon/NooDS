/*
    Copyright 2019-2025 Hydr8gon

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

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;

public class FileAdapter extends BaseAdapter {
    public class FileInfo {
        public String name;
        public Bitmap icon;
        public Uri uri;
    }

    private Context context;
    private ArrayList<FileInfo> info;

    public FileAdapter(Context context) {
        this.context = context;
    }

    public void setInfo(ArrayList<FileInfo> info) {
        this.info = info;
    }

    @Override
    public int getCount() {
        return info.size();
    }

    @Override
    public Object getItem(int position) {
        return null;
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        // Create a new file row
        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(context);
            convertView = inflater.inflate(R.layout.file_row, parent, false);
        }

        // Set the file title and icon
        TextView title = (TextView)convertView.findViewById(R.id.name);
        title.setText(info.get(position).name);
        ImageView icon = (ImageView)convertView.findViewById(R.id.icon);
        icon.setImageBitmap(info.get(position).icon);
        return convertView;
    }
}
