#!/usr/bin/env bash

set -o errexit
set -o pipefail

app=NooDS.app
contents=$app/Contents

if [[ -d "$app" ]]; then
	rm -rf "$app"
fi

libs=($(otool -L noods | grep -vE "/System|/usr/lib|:$" | sed -E 's/\t(.*) \(.*$/\1/'))

install -dm755 "${contents}"/{MacOS,Resources,Frameworks}
install -sm755 noods "${contents}/MacOS/NooDS"
install -m644 Info.plist "$contents/Info.plist"

install_name_tool -add_rpath "@executable_path/../Frameworks" $contents/MacOS/NooDS

for lib in "${libs[@]}"; do
	base=$(basename "$lib")
	libname=$(echo $base | cut -d. -f1)
	install -m644 "$lib" "$contents/Frameworks/$base"
	install_name_tool -change "$lib" "@rpath/$base" $contents/MacOS/NooDS
done

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
