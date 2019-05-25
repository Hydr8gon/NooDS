# NoiDS
A "noice" NDS emulator.

The goal of this project is to be a fast (and not necessarily accurate) Nintendo DS emulator. My hope is that it will at least be able to run well on the Nintendo Switch.

This is my third emulator project. I'm doing this for fun and as a learning experience, and also because I'm a huge fan of the DS. That being said, it's proving to be quite a step up from something like the NES; it took three weeks just to get the most basic libnds homebrew booting, and it still feels like I've only barely started. It may seem futile to attempt a DS emulator from scratch when there are some decent ones out there already, but I like to do things myself! And hopefully some of the plans I have for the future of the project will make it worthwhile. If I get that far.

Right now there's only a barebones Linux build (it might be portable to Windows without too much hassle?). A simple "make" command should suffice for compiling. A Switch build will happen once some more things are running.

You probably won't be able to run anything on this yet! If you still want to try it, you'll need BIOS files, named "bios7.bin" and "bios9.bin", placed in the same directory as the executable. There's no ROM selector, so you'll have to specify a ROM by launching from the terminal.

I hope some people will find some use for this project as it matures. If anything it's at least helping me grow my skills as a programmer. Let's have some fun with this :)
