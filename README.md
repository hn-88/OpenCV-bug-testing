# OCVWarp
Warping images and videos for planetarium fulldome display using OpenCV.

If OpenCV and CMake are installed on your system, instructions for building:

```
cd build
cmake ..
make OCVWarp.bin
```
Only OCVWarp.bin is strictly necessary. If desired, 
```
make OCVWarpNorth.bin
make getfourcc.bin
```
Initial behaviour and parameters are set using OCVWarp.ini in the build folder. Please see [transformtype.txt](https://github.com/hn-88/OCVWarp/blob/master/build/transformtype.txt) for supported transforms. 

A file open dialog asks you for the input file. The output file is put in the same directory, with F.avi appended to the input filename. The codec used for the output is the same codec as for the input if available on your system, or as chosen in the ini file. (If the input file's codec is not available, the output is saved as an uncompressed avi, which can quickly become huge.)

Keyboard commands are
```
ESC, x or X to exit
u, + or =   to increase angley by 1 degree - the angle seen vertically in output
m, - or _   to decrease angley by 1 degree 
U           to increase angley by 10 degrees
M           to decrease angley by 10 degrees
k, ] or }   to increase anglex by 1 degree - the angle seen horizontally in output
h, [ or {   to decrease anglex by 1 degree - the angle seen horizontally in output
K           to increase anglex by 10 degrees
H           to decrease anglex by 10 degrees
d or D      to toggle display of warped file. Turning off display marginally increases processing speed.
```

