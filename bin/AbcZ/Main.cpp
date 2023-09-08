//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#include <conio.h>
#include <stdio.h>
#include <chrono>
#include <windows.h>
#include <lib7zr.h>

int abcz(int argc, char* argv[]);

//-*****************************************************************************
int main( int argc, char *argv[] ){
    auto now = std::chrono::system_clock::now();

#if 1   
    abcz(argc, argv);
#elif 0
    char* filename = argv[1];
    filename = "1plane.abc";
    filename = "1plane_tri.abc";
    filename = "a1_nonormals.abc";
    filename = "a1_1.abc";
    filename = "chr_DaJiaZhang.abc";      // totalsize_max: 87,700,200
    //filename = "chr_WuMa.abc";            // totalsize_max: 177,828,336
    //filename = "DaJiaZhangMaCheA.abc";    // totalsize_max: 1,481,316,480
    char* argv2[] = { NULL, filename };

    //argc = 2;
    //abcz(argc, argv2);
#else   //decompress
#endif
    auto duration = std::chrono::system_clock::now() - now;
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    printf("spent time: %.2fs\n", ((float)ms) / 1000);

    return 0;
}
