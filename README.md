# flolinker

Flat Loadable Object Linker (**flolinker**) is a (x64 only for now) linker that produces a binary that can be easily loaded into the address space of an application and have functions defined in it called. Yes, it's kind of a .dll or .so.

It has been slightly tested only under Win64 with object files produced by [tdm-gcc](http://tdm-gcc.tdragon.net/) and using the small memory model (`-mcmodel=small`).

It comes with an example program, and has been tested on Windows only.