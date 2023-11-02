# NooDS
A (hopefully!) speedy DS emulator.

### Overview
NooDS aims to be a fast and portable Nintendo DS emulator. It's not quite there speed-wise, but it offers most other features that you might expect from a DS emulator. It even supports GBA backwards compatibility! I'm doing it for fun and as a learning experience, also because I'm a huge DS fan. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. If not, that's fine too; like I said, I'm just having fun!

### Downloading
NooDS is available on Android, Linux, macOS, Switch, Vita and Windows. Automatic builds are provided via GitHub Actions; you can download them on the [releases page](https://github.com/Hydr8gon/NooDS/releases).

### Usage
NooDS should be able to run most things without additional setup. DS BIOS and firmware files must be provided to boot from the DS menu, which can be dumped from a DS with [DSBF Dumper](https://archive.org/details/dsbf-dumper). The firmware must be dumped from an original DS; DSi and 3DS dumps don't have any boot code. A GBA BIOS file can be optionally provided, and can be dumped from many systems with [this dumper](https://github.com/mgba-emu/bios-dump). The BIOS and firmware file paths can be set in the settings. Although not always accurate, save types are automatically detected. Manual alteration of the save type is needed if you load a new game and saving does not work.

### Building for Android
The easiest way to build for Android would be with [Android Studio](https://developer.android.com/studio). [Android NDK](https://developer.android.com/studio/projects/install-ndk) is needed for building native code. [Command line tools](https://developer.android.com/studio#command-tools) can be alternatively used; use `sdkmanager` to install `build-tools`, `cmake`, `ndk-bundle`, `platform-tools` and `platforms;android-29` and set an `ANDROID_SDK_ROOT` environment variable to the directory containing `cmdline-tools`. You can then run `./gradlew assembleRelease` in the project root directory to build.

### Building for Linux or macOS
[wxWidgets](https://www.wxwidgets.org) and [PortAudio](https://www.portaudio.com) installed via your favourite package manager are needed to build for Linux or macOS. [Homebrew](https://brew.sh) can be used on macOS; no package manager is given by default. The command will look like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). You can then run `make` in the project root directory to build.

### Building for Switch
[devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `switch-dev` package are needed to build for the Switch. You can then run `make -f Makefile.switch` in the project root directory to build.

### Building for Vita
[Vita SDK](https://vitasdk.org) is needed to build for the Vita. You can then run `make -f Makefile.vita` in the project root directory to build.

### Building for Windows
[MSYS2](https://www.msys2.org) is needed to build for Windows. You can install every needed package by running `pacman -S mingw-w64-x86_64-{gcc,pkg-config,wxWidgets,portaudio,jbigkit} make` once you have that set up and running. It might also be a good idea to run `pacman -Syu` to ensure everything is 
up to date. You can then run `make` in the project root directory to build.

### Contributing
While I appreciate anyone who wants to contribute, my goal with this project is to challenge myself and not to review code. I feel guilty rejecting a change that someone spent time on, but I also don't feel great accepting changes that I didn't ask for. For this reason, I've decided to stop accepting pull requests. You're of course still free to do anything with the code that's allowed by the license, but if you submit a pull request it will likely be ignored. I hope this is understandable!

### References
* [GBATEK](https://problemkaputt.de/gbatek.htm) by Martin Korth - It's where most of my information came from
* [GBATEK addendum](https://melonds.kuribo64.net/board/thread.php?id=13) by Arisotura - Some information came from here too as GBATEK isn't perfect
* Blog posts [1](https://melonds.kuribo64.net/comments.php?id=85), [2](https://melonds.kuribo64.net/comments.php?id=56), [3](https://melonds.kuribo64.net/comments.php?id=32), and [4](https://melonds.kuribo64.net/comments.php?id=27) by Arisotura - Great resources that detail the 3D GPU's lesser-known quirks
* [DraStic BIOS](https://drive.google.com/file/d/1dl6xgOXc892r43RzkIJKI6nikYIipzoN/view) by Exophase - Reference for the DS HLE BIOS functions
* [GBA BIOS](https://github.com/Cult-of-GBA/BIOS) by Cult of GBA - Reference for the GBA HLE BIOS functions
* [ARM Opcode Map](https://imrannazar.com/ARM-Opcode-Map) by Imran Nazar - Used to create the interpreter lookup table
* Hardware tests by me - When there's something that I can't find or want to verify, I write tests for it myself!

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io) - Blog where I may or may not write about things
* [Discord Server](https://discord.gg/JbNz7y4) - Place to chat about my projects and stuff
