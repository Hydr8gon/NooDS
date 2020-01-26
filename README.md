# NooDS
A (hopefully!) speedy NDS emulator.

### Overview
The goal of NooDS is to be a fast and portable Nintendo DS emulator. My hope is that it will eventually be able to run well on the Nintendo Switch. And desktop too, of course! I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. To be honest, I've gotten further than I had originally expected; with most major features implemented, including a fairly competent 3D renderer and audio output, it's starting to feel like I have a proper emulator on my hands. It may not be a worthy competitor for the other DS emulators just yet, but I believe that I can get it there someday. And hopefully some of the plans I have for the future will make it worthwhile. Let's have some fun with this!

### Downloading
NooDS is available for Linux, macOS, Windows, and the Nintendo Switch. Automatic builds are provided via GitHub Actions; to use them, you'll need to install [wxWidgets](https://www.wxwidgets.org) and [PortAudio](http://www.portaudio.com) on Linux or macOS (see: Compiling for Linux or macOS). On Windows, you'll only need [the latest Visual C++ runtime library](https://support.microsoft.com/en-ca/help/2977003/the-latest-supported-visual-c-downloads).

### Compiling for Linux or macOS
To compile on Linux or macOS, you'll first need to install wxWidgets and PortAudio using your favourite package manager. You can use [Homebrew](https://brew.sh) on macOS, since there is no package manager provided by default. The command will look something like `apt install libwxgtk3.0-dev portaudio19-dev` (Ubuntu) or `brew install wxmac portaudio` (macOS). After that, you can simply run `make` in the project root directly to compile.

### Compiling for Windows
Unfortunately, compiling on Windows is a bit of a hassle. A Visual Studio solution file is provided, but you'll need to compile wxWidgets and PortAudio before you can use it. To make matters worse, the directories are hardcoded to work around an issue with the automatic builds. To compile wxWidgets, first download [the wxWidgets source](https://github.com/wxWidgets/wxWidgets/releases/download/v3.1.2/wxWidgets-3.1.2.zip) and extract it to the root of your C drive. Next, navigate to `C:\wxWidgets-3.1.2\build\msw` with the [Visual Studio Developer Command Prompt](https://docs.microsoft.com/en-us/dotnet/framework/tools/developer-command-prompt-for-vs) and run `nmake.exe -f makefile.vc BUILD=release TARGET_CPU=x64` to compile. To compile PortAudio, download [the PortAudio source](https://app.assembla.com/spaces/portaudio/git/source/pa_stable_v190600_20161030) and extract it to the root of your C drive. You'll need to install [CMake](https://cmake.org) for this next part: navigate to `C:\portaudio\build` with the Developer Command Prompt and run `cmake .. -G "Visual Studio 16 2019" & msbuild.exe portaudio.sln /property:Configuration=Release` to compile PortAudio. If everything was successful, you should finally be able to use the provided solution file to compile NooDS.

### Compiling for Switch
To build for the Switch, you'll need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `switch-dev` package. You can then run `make -f Makefile.switch` in the project root directory to compile.

### Usage
NooDS doesn't provide high-level emulation of the BIOS yet. To actually run games, you'll need to provide BIOS and firmware files, dumped from your physical DS. On the Switch, you'll also need a ROM file named "rom.nds" in the same directory as the executable, because there's no file browser yet.

### Other Links
[The NooDS Discord server](https://discord.gg/JbNz7y4), where we can chat and do other fun stuff! \
[My Patreon](https://www.patreon.com/Hydr8gon), where you can support the project or read about my progress!
