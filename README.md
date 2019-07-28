# NooDS
A (hopefully!) speedy NDS emulator.

The goal of this project is to be a fast (and not necessarily accurate) Nintendo DS emulator. My hope is that it will at least be able to run well on the Nintendo Switch.

This is my third emulator project. I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. That being said, it's proving to be quite a step up from something like the NES; it took three weeks just to get the most basic libnds homebrew booting, and it still feels like I've only barely started. It may seem futile to attempt a DS emulator from scratch when there are some decent ones out there already, but I like to do things myself! And hopefully some of the plans I have for the future of the project will make it worthwhile. If I get that far.

Right now there's a build for Windows and Linux, and a barebones build for Switch. To build for desktop, you'll need to compile and install [wxWidgets](https://www.wxwidgets.org/). On Linux, simply run "make" in the project root directory to compile. On Windows, you can use the provided .sln file. To build for Switch, you'll need to have devkitPro set up on your machine and the switch-dev package installed. You can then run "make -f Makefile.switch" in the project root directory to compile.

You probably won't be able to run commercial games on this yet! If you still want to try it, you'll need BIOS files, named "bios7.bin" and "bios9.bin", and a firmware file, named "firmware.bin", placed in the same directory as the executable. On the Switch, you'll also have to name a ROM file "rom.nds" and place it in the directory, because there's no file browser yet.

I hope someone will find some use for this project as it matures. If anything it's at least helping me grow my skills as a programmer. Let's have some fun with this :)

Also check out:
[The NooDS Discord server](https://discord.gg/JbNz7y4), where we can chat and do other fun stuff!
[My Patreon](https://www.patreon.com/Hydr8gon), where you can support the project or read about my progress!
