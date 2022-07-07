# NooDS
A (hopefully!) speedy DS emulator. Aims to be a fast, portable Nintendo DS emulator. It is not quite there speed-wise, but it offers most other features that you might expect from a DS emulator. It even supports GBA backwards compatibility! I am doing it for fun & as a learning experience, also because I am a huge DS fan. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. If not, that is fine too; like I said, I am just having fun!
## Installation
It is available on Android, Linux, macOS, Switch, Vita & Windows. Automatic builds are provided via GitHub Actions; you can download them via the [releases page](https://github.com/Hydr8gon/NooDS/releases).
## Usage
NooDS should be able to run most things without additional setups. DS BIOS & firmware files must be provided to boot from the DS menu, which can be dumped from a DS with [DSBF Dumper](https://archive.org/details/dsbf-dumper). The firmware must be dumped from an original DS; DSi and 3DS dumps do not have any boot code. A GBA BIOS file must be provided to run GBA games, which can be dumped from many systems with [this dumper](https://github.com/mgba-emu/bios-dump). The BIOS and firmware file paths can be set in the settings. Although not always accurate, save types are automatically detected. You will need to manually change the save type until it does if you load a new game and saving does not work.
### Building for Android
The easiest way would be with [Android Studio](https://developer.android.com/studio) to build for Android. [Android NDK](https://developer.android.com/studio/projects/install-ndk) is needed for building native code. [Command line tools](https://developer.android.com/studio#command-tools) can be alternatively used; use `sdkmanager` to install `build-tools`, `cmake`, `ndk-bundle`, `platform-tools` & `platforms;android-29` & set an `ANDROID_SDK_ROOT` environment variable to the directory containing `cmdline-tools`. You can then run `./gradlew assembleRelease` in the project root directory to build.
### Building for Linux or macOS
[wxWidgets](https://www.wxwidgets.org) & [PortAudio](http://www.portaudio.com) installed via your favourite package manager are needed to build for Linux or macOS. [Homebrew](https://brew.sh) can be used on macOS; no package manager is given by default. The command will look like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). You can then run `make` in the project root directory to build.
### Building for Switch
[devkitPro](https://devkitpro.org/wiki/Getting_Started) & the `switch-dev` package are needed to build for the Switch. You can then run `make -f Makefile.switch` in the project root directory to build.
### Building for Vita
[Vita SDK](https://vitasdk.org/) is needed to build for the Vita. You can then run `make -f Makefile.vita` in the project root directory to build.
### Building for Windows
[MSYS2](https://www.msys2.org) is needed to build for Windows. You can install every needed package by running `pacman -S mingw-w64-x86_64-{gcc,pkg-config,wxWidgets,portaudio,jbigkit} make` once you have that set up and running. It might also be a good idea to run `pacman -Syu` to ensure everything is up to date. You can then run `make` in the project root directory to build.
## References
* [GBATEK](https://problemkaputt.de/gbatek.htm) by Martin Korth - It is where most of my information came from
* [GBATEK addendum](http://melonds.kuribo64.net/board/thread.php?id=13) by Arisotura - It is not perfect, so some information came from here too
* Blog posts [1](http://melonds.kuribo64.net/comments.php?id=85), [2](https://melonds.kuribo64.net/comments.php?id=56), [3](https://melonds.kuribo64.net/comments.php?id=32), and [4](https://melonds.kuribo64.net/comments.php?id=27) by Arisotura - Great resources that detail the 3D GPU's lesser-known quirks
* [DraStic BIOS](https://drive.google.com/file/d/1dl6xgOXc892r43RzkIJKI6nikYIipzoN/view) by Exophase - Reference for the HLE BIOS implementation
* [ARM Opcode Map](https://imrannazar.com/ARM-Opcode-Map) by Imran Nazar - Used to create the interpreter lookup table
* Hardware tests by me - When there is something that I cannot find or want to verify, I write tests for it myself!

## See also
* [Hydra's Lair](https://hydr8gon.github.io/) - You can read about my progress here!
* [NooDS Discord](https://discord.gg/JbNz7y4) - We can chat & do other fun stuff here!
