# PlaneSize
Bite sized code projects made during one plane ride

## Script

Script is a script engine that compiles C functions in 
[these 100 lines of unreadable code](Script/script.cc).
It calculates the first 20 numbers from the Fibonacci sequence as calculated through
an embedded script.

### Compile and run

To compile on Linux and MacOS

> g++ -o script -std=c++11 script.cc

On Windows

> cl script.cc

Just run the resulting binary directly. [readable.cc](Script/readable.cc) is the
same code except in a more readable format.
