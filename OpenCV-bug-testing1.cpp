#ifdef _WIN64
#include "windows.h"
#endif

/*
 * Copied from OCVWarp.cpp
 * 
 * 
 * testing for an opencv bug
 * https://github.com/opencv/opencv/issues/10694
 * 
 * first commit:
 * Hari Nandakumar
 * 23 May 2024
 * 
 * 
 */

//#define _WIN64
//#define __unix__


#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <string.h>
#include <fstream>
#include <time.h>
//#include <sys/stat.h>
// this is for mkdir

#include <opencv2/opencv.hpp>
#include "tinyfiledialogs.h"
#define CVUI_IMPLEMENTATION
#include "cvui.h"
#define WINDOW_NAME "OCVWARP - HIT <esc> TO CLOSE"

#define CV_PI   3.1415926535897932384626433832795

using namespace cv;


int main(int argc,char *argv[])
{
	
Mat m(960,1280,CV_16U);
randn(m,10000, 500);
Scalar M,D;
meanStdDev(m,M,D);
std::cout << M(0) << " " << D(0) << std::endl;
Mat m2 = m.reshape(0,1);
meanStdDev(m2,M,D);
std::cout << M(0) << " " << D(0);
	
    
    std::cout << std::endl << "Finished writing" << std::endl;
    return 0;
	   
	   
} // end main
