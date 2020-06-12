=== Needs Cleanup

this is ported from older code bases and could use some code cleanup

current main is in src/test.cpp

this includes the functional plus library (headers linked from FunctionalPlus to include/fplus)

lvgl includes no make file.  we either need to add the lvgl soruce to our make file, or compile the lvgl code into a library.

I'm kind of leaning toward compiling lvgl into a library and then just linking it in.

