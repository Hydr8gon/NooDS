# NooDS
A (hopefully!) speedy NDS emulator.

### Overview
The goal of NooDS is to be a fast and portable Nintendo DS emulator. It's not quite there speed-wise, but it does offer most other features that you might expect from a DS emulator. It even supports GBA backwards compatability! I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. If not, that's fine too; like I said, I'm just having fun!

### Downloads
NooDS is available for Linux, macOS, Windows, Switch, and Android. Automatic builds are provided via GitHub Actions; to download them, you'll need to be signed in with a GitHub account. macOS and Linux builds are dynamically linked, so you'll need to install the correct versions of the wxWidgets and PortAudio libraries for them to work (see: Compiling for Linux or macOS). You shouldn't need anything additional for other systems.

### Usage
NooDS doesn't provide high-level emulation of the BIOS yet, so you'll need to provide BIOS and firmware files dumped from your physical DS. The file paths can be configured in the settings. Another thing lacking is automatic save type detection for DS games. If you load a game that doesn't have an existing save, you'll have to manually specify the save type. This information can be difficult to find, so it's easier if you have working save files already present.

### Compiling for Linux or macOS
To compile on Linux or macOS, you'll need to install [wxWidgets](https://www.wxwidgets.org) and [PortAudio](http://www.portaudio.com) using your favourite package manager. You can use [Homebrew](https://brew.sh) on macOS, since there is no package manager provided by default. The command will look something like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). After that, you can simply run `make` in the project root directory to compile.

### Compiling for Windows
To compile on Windows, you'll need to install [MSYS2](https://www.msys2.org). Once you have that set up and running, you can install all of the packages you'll need by running `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-wxWidgets mingw-w64-x86_64-portaudio mingw-w64-x86_64-jbigkit make`. It might also be a good idea to run `pacman -Syu` to ensure everything is up to date. After that, you can simply run `make` in the project root directory to compile.

### Compiling for Switch
To compile for the Switch, you'll need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `switch-dev` package. You can then run `make -f Makefile.switch` in the project root directory to compile.

### Compiling for Android
To compile for Android, you'll need to setup the [Android SDK](https://developer.android.com/studio#command-tools). Create a folder where you want to install the SDK, and within that folder extract the contents of the downloaded `tools` folder to `cmdline-tools/latest`. Use the `sdkmanager` command in the resulting `bin` folder to install `build-tools`, `cmake`, `ndk-bundle`, `platform-tools`, and `platforms;android-30`. You'll also need to set an `ANDROID_SDK_ROOT` environment variable to the folder containing `cmdline-tools` and the other installed components. If everything is set up properly, you should be able to compile by running `./gradlew assembleRelease` in the project root directory. To make the resulting .apk installable, you'll need to sign it using [apksigner](https://developer.android.com/studio/command-line/apksigner).

### Other Links
[The NooDS Discord server](https://discord.gg/JbNz7y4), where we can chat and do other fun stuff! \
[Hydra's Lair](https://hydr8gon.github.io/), where you can read about my progress!
