# NooDS
A (hopefully!) speedy NDS emulator.

### Overview
The goal of NooDS is to be a fast and portable Nintendo DS emulator. My hope is that it will eventually be able to run well on the Nintendo Switch. And desktop too, of course! I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. To be honest, I've gotten further than I had originally expected; with most major features implemented, including a fairly competent 3D renderer and audio output, it's starting to feel like I have a proper emulator on my hands. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. And hopefully some of the plans I have for the future will make it worthwhile. Let's have some fun with this!

### Downloading
NooDS is available for Linux, macOS, Windows, and the Nintendo Switch. Automatic builds are provided via GitHub Actions; to use them, you'll need to install [wxWidgets](https://www.wxwidgets.org) and [PortAudio](http://www.portaudio.com) on Linux or macOS (see: Compiling for Linux or macOS). On Windows and Switch, you shouldn't need anything additional.

### Compiling for Linux or macOS
To compile on Linux or macOS, you'll need to install wxWidgets and PortAudio using your favourite package manager. You can use [Homebrew](https://brew.sh) on macOS, since there is no package manager provided by default. The command will look something like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). After that, you can simply run `make` in the project root directory to compile.

### Compiling for Windows
To compile on Windows, you'll need to install [MSYS2](https://www.msys2.org). Once you have that set up and running, you can install all of the packages you'll need by running `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-wxWidgets mingw-w64-x86_64-portaudio mingw-w64-x86_64-jbigkit make`. After that, you can simply run `make` in the project root directory to compile.

### Compiling for Switch
To compile for the Switch, you'll need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `switch-dev` package. You can then run `make -f Makefile.switch` in the project root directory to compile.

### Usage
NooDS doesn't provide high-level emulation of the BIOS yet. To actually run games, you'll need to provide BIOS and firmware files, dumped from your physical DS. Another thing NooDS is currently lacking is save type detection. If you try to run a game that doesn't have a save, you'll have to manually specify the save type. This information can be difficult to find, so it's easier if you have working save files aready present.

### Other Links
[The NooDS Discord server](https://discord.gg/JbNz7y4), where we can chat and do other fun stuff! \
[Hydra's Lair](https://hydr8gon.github.io/), where you can read about my progress!
