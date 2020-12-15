# NooDS
A (hopefully!) speedy NDS emulator.

### Overview
The goal of NooDS is to be a fast and portable Nintendo DS emulator. It's not quite there speed-wise, but it does offer most other features that you might expect from a DS emulator. It even supports GBA backwards compatability! I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. If not, that's fine too; like I said, I'm just having fun!

### Downloads
NooDS is available for Linux, macOS, Windows, Switch, and Android. Automatic builds are provided via GitHub Actions; to download them, you'll need to be signed in with a GitHub account. macOS and Linux builds are dynamically linked, so you'll need to install the correct versions of the wxWidgets and PortAudio libraries for them to work (see: Compiling for Linux or macOS). You shouldn't need anything additional for other systems.

### Usage
NooDS doesn't provide high-level emulation of the BIOS yet, so you'll need to provide BIOS and firmware files dumped from your physical DS. The file paths can be configured in the settings. It also currently lacks automatic save type detection for DS games. If you load a new game and saving doesn't work, you'll have to manually change the save type. This information can be difficult to find, so it's easier if you have working save files already present.

### Compiling for Linux or macOS
To compile on Linux or macOS, you'll need to install [wxWidgets](https://www.wxwidgets.org) and [PortAudio](http://www.portaudio.com) using your favourite package manager. You can use [Homebrew](https://brew.sh) on macOS, since there is no package manager provided by default. The command will look something like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). After that, you can simply run `make` in the project root directory to compile.

### Compiling for Windows
To compile on Windows, you'll need to install [MSYS2](https://www.msys2.org). Once you have that set up and running, you can install all of the packages you'll need by running `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-wxWidgets mingw-w64-x86_64-portaudio mingw-w64-x86_64-jbigkit make`. It might also be a good idea to run `pacman -Syu` to ensure everything is up to date. After that, you can simply run `make` in the project root directory to compile.

### Compiling for Switch
To compile for the Switch, you'll need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `switch-dev` package. You can then run `make -f Makefile.switch` in the project root directory to compile.

### Compiling for Android
To compile for Android, the easiest way would be to use [Android Studio](https://developer.android.com/studio). You'll also need to install the [Android NDK](https://developer.android.com/studio/projects/install-ndk) for compiling native code. Alternatively, you can use the [command line tools](https://developer.android.com/studio#command-tools); use `sdkmanager` to install `build-tools`, `cmake`, `ndk-bundle`, `platform-tools`, and `platforms;android-30`, and set an `ANDROID_SDK_ROOT` environment variable to the folder containing `cmdline-tools`. You should then be able to compile by running `./gradlew assembleRelease` in the project root directory.

### References
* [GBATEK](https://problemkaputt.de/gbatek.htm) by Martin Korth - This is where most of my information came from
* [GBATEK Addendum](http://melonds.kuribo64.net/board/thread.php?id=13) by Arisotura - GBATEK isn't perfect, so some information came from here as well
* Blog Posts [1](http://melonds.kuribo64.net/comments.php?id=85), [2](http://melonds.kuribo64.net/comments.php?id=56), [3](http://melonds.kuribo64.net/comments.php?id=32), and [4](http://melonds.kuribo64.net/comments.php?id=27) by Arisotura - Great resources that detail the 3D GPU's lesser-known quirks
* [ARM Opcode Map](http://imrannazar.com/ARM-Opcode-Map) by Imran Nazar - Used to create the interpreter lookup table
* Hardware tests by me - When there's something that I can't find or want to verify, I write tests for it myself!

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io/) - you can read about my progress here!
* [NooDS Discord](https://discord.gg/JbNz7y4) - here we can chat and do other fun stuff!
