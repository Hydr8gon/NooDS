#!/usr/bin/env bash

set -o errexit
set -o pipefail

app=NooDS.app
contents=$app/Contents

if [[ -d "$app" ]]; then
	rm -rf "$app"
fi

install -dm755 "${contents}"/{MacOS,Resources,Frameworks}
install -sm755 noods "${contents}/MacOS/NooDS"
install -m644 Info.plist "$contents/Info.plist"

# Recursively copy dependent libraries to the Frameworks directory
# and fix their load paths
fixup_libs() {
	local libs=($(otool -L "$1" | grep -vE "/System|/usr/lib|:$" | sed -E 's/\t(.*) \(.*$/\1/' | sort -d))
	
	for lib in "${libs[@]}"; do
		local base="$(basename "$lib")"
		local libname="$(echo "$base" | cut -d- -f1)"

		existing=$(find "$contents/Frameworks" -name "${libname}*")
		if [[ ! -z "$existing" ]]; then
			install_name_tool -change "$lib" "@rpath/$(basename "$existing")" "$1"
			continue
		else
			install_name_tool -change "$lib" "@rpath/$base" "$1"
		fi

		if [[ ! -f "$contents/Frameworks/$base" ]]; then
			install -m644 "$lib" "$contents/Frameworks/$base"
			strip -SNTx "$contents/Frameworks/$base"
			fixup_libs "$contents/Frameworks/$base"
		fi
	done
}

install_name_tool -add_rpath "@executable_path/../Frameworks" $contents/MacOS/NooDS

fixup_libs $contents/MacOS/NooDS

mkdir -p NooDS.iconset
cp src/android/res/drawable-v26/icon_foreground.png NooDS.iconset/icon_512x512.png
iconutil --convert icns NooDS.iconset --output NooDS.app/Contents/Resources/NooDS.icns
rm -r NooDS.iconset

codesign --deep -s - NooDS.app

if [[ $1 == '--dmg' ]]; then
	mkdir build/dmg
	cp -r NooDS.app build/dmg/
	ln -s /Applications build/dmg/Applications
	hdiutil create -volname NooDS -srcfolder build/dmg -ov -format UDZO NooDS.dmg
	rm -r build/dmg
fi
