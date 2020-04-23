#ifdef _WIN64
#include "windows.h"
#endif



/*
 * OCVWarp.cpp
 * 
 * Warps video files using the OpenCV framework. 
 * Appends F to the filename and saves as default codec (DIVX avi) in the same folder.
 * 
 * first commit:
 * Hari Nandakumar
 * 25 Jan 2020
 * 
 * 
 */

//#define _WIN64
//#define __unix__

// references 
// http://paulbourke.net/geometry/transformationprojection/
// equations in figure at http://paulbourke.net/dome/dualfish2sphere/
// http://paulbourke.net/dome/dualfish2sphere/diagram.pdf

// http://www.fmwconcepts.com/imagemagick/fisheye2pano/index.php
// http://www.fmwconcepts.com/imagemagick/pano2fisheye/index.php
// 
// https://docs.opencv.org/3.4/d8/dfe/classcv_1_1VideoCapture.html
// https://docs.opencv.org/3.4/d7/d9e/tutorial_video_write.html
// https://docs.opencv.org/3.4.9/d1/da0/tutorial_remap.html
// https://stackoverflow.com/questions/60221/how-to-animate-the-command-line
// https://stackoverflow.com/questions/11498169/dealing-with-angle-wrap-in-c-code
// https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html

// Pertinent equations from pano2fisheye:
// fov=180 for fisheye
// fov=2*phimax or phimax=fov/2
// note rmax=N/2; N=height of input
// linear: r=f*phi; f=rmax/phimax; f=(N/2)/((fov/2)*(pi/180))=N*180/(fov*pi)
// substitute fov=180
// linear: f=N/pi
// linear: phi=r*pi/N

// https://stackoverflow.com/questions/46883320/conversion-from-dual-fisheye-coordinates-to-equirectangular-coordinates
// taking Paul's page as ref, http://paulbourke.net/dome/dualfish2sphere/diagram.pdf
/* // 2D fisheye to 3D vector
phi = r * aperture / 2
theta = atan2(y, x)

// 3D vector to longitude/latitude
longitude = atan2(Py, Px)
latitude = atan2(Pz, (Px^2 + Py^2)^(0.5))

// 3D vector to 2D equirectangular
x = longitude / PI
y = 2 * latitude / PI
* ***/
/*
 * https://groups.google.com/forum/#!topic/hugin-ptx/wB-4LJHH5QI
 * panotools code
 * */

#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <string.h>

#include <time.h>
//#include <sys/stat.h>
// this is for mkdir

#include <opencv2/opencv.hpp>
#include "tinyfiledialogs.h"

#define CV_PI   3.1415926535897932384626433832795

using namespace cv;

// some global variables

std::string strpathtowarpfile;
Mat meshu, meshv, meshx, meshy, meshi, I;
Mat map2x, map2y;
float maxx=0, minx=0;	
// meshx is in the range [-aspectratio, aspectratio]
// we assume meshy is in the range [-1,1]
// meshu and meshv in [0,1]

bool ReadMesh(std::string strpathtowarpfile)
{
	
	//from https://github.com/hn-88/GL_warp2Avi/blob/master/GL2AviView.cpp
	// and http://paulbourke.net/dataformats/meshwarp/
	FILE *input = NULL;

   input = fopen(strpathtowarpfile.c_str(), "r");

   /* Set rows and columns to 2 initially, as this is the size of the default mesh. */
    int dummy, rows = 2, cols = 2;

    if (input != NULL)  {
		fscanf(input, " %d %d %d ", &dummy, &cols, &rows) ;
		float x, y, u, v, l;
		//meshrows=rows;
		//meshcolumns=cols;

		meshx = Mat(Size(cols,rows), CV_32FC1);	
		meshy = Mat(Size(cols,rows), CV_32FC1);
		meshu = Mat(Size(cols,rows), CV_32FC1);
		meshv = Mat(Size(cols,rows), CV_32FC1);
		meshi = Mat(Size(cols,rows), CV_32FC1);

	     for (int r = 0; r < rows ; r++) {
             for (int c = 0; c < cols ; c++) {
		                
                fscanf(input, "%f %f %f %f %f", &x, &y, &u, &v, &l) ;   
                
                if (x<minx)
					minx = x;
				else
				if (x>maxx)
					maxx = x;
                
                //~ mesh[cols*r+c].x = x;
                //~ mesh[cols*r+c].y = y;
                //~ mesh[cols*r+c].u = u;
                //~ mesh[cols*r+c].v = v;
                //~ mesh[cols*r+c].i = l;
                meshx.at<float>(r,c) = x;
                meshy.at<float>(r,c) = y;
                meshu.at<float>(r,c) = u;
                meshv.at<float>(r,c) = v;
                meshi.at<float>(r,c) = l;
 
			}
			 
		}
		
	}
	else // unable to read mesh
	{
		std::cout << "Unable to read mesh data file (similar to EP_xyuv_1920.map), exiting!" << std::endl;
		exit(0);
	}
}

void update_map( double anglex, double angley, Mat &map_x, Mat &map_y, int transformtype )
{
	// explanation comments are most verbose in the last 
	// default (transformtype == 0) section
	switch (transformtype)
	{
	case 5: // 360 to 180 fisheye and then to warped
	//if (transformtype == 5)	
	{
		// create temp maps to the texture and then map from texture to output
		// this will need 2 remaps at the output side
		// and two sets of map files
		
		// the map file for the first remap has to change with change of anglex angley
		// the second one, for fisheye to warped, doesn't need to be recalculated.
		
		// so, update_map is called first to initialize map_x and map_y
		// using transformtype = 4
		
		// and later, and at all other times, only the map to the texture is updated.
		
		update_map( anglex, angley, map2x, map2y, 1 );
		
	}
		break;
		
	case 4:
	//if (transformtype == 4)	//  fisheye to warped
	{
		// similar to TGAWarp at http://paulbourke.net/dome/tgawarp/
		//
		
		Mat U, V, X, Y, IC1;
		Mat indexu, indexv, indexx, indexy, temp;
		ReadMesh(strpathtowarpfile);
		resize(meshx, X, map_x.size(), INTER_LANCZOS4);
		resize(meshy, Y, map_x.size(), INTER_LANCZOS4);
		//debug - changed INTER_LINEAR to INTER_LANCZOS4
		// not much of a penalty, so we leave it in.
		resize(meshu, U, map_x.size(), INTER_LANCZOS4);
		resize(meshv, V, map_x.size(), INTER_LANCZOS4);
		resize(meshi, IC1, map_x.size(), INTER_LANCZOS4);
		
		// I.convertTo(I, CV_32FC3); //this doesn't work 	
		//convert to 3 channel Mat, for later multiplication
		// https://stackoverflow.com/questions/23303305/opencv-element-wise-matrix-multiplication/23310109
		Mat t[] = {IC1, IC1, IC1};
		merge(t, 3, I);
		
		// map the values which are [minx,maxx] to [0,map_x.cols-1]
		temp = (map_x.cols-1)*(X - minx)/(maxx-minx);
		temp.convertTo(indexx, CV_32S);		// this does the rounding to int
		
		temp = (map_x.rows-1)*(Y+1)/2;	// assuming miny=-1, maxy=1
		temp.convertTo(indexy, CV_32S);
		
		temp = (map_x.cols-1)*U;	// assuming minu=0, maxu=1
		temp.convertTo(indexu, CV_32S);		// this does the rounding to int
		
		temp = (map_x.rows-1)*V;	// assuming minv=0, maxv=1
		temp.convertTo(indexv, CV_32S);
		
		// debug
		//~ imwrite("indexx.png", indexx);
		//~ imwrite("indexy.png", indexy);
		//~ imwrite("indexu.png", indexu);
		//~ imwrite("indexv.png", indexv);
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					//~ map_x.at<float>(i, j) = (float)(j); // this just maps input to output
				    //~ map_y.at<float>(i, j) = (float)(i); 
				    
				    // in the following, we assume indexx.at<int>(i,j) = j
				    // and indexy.at<int>(i,j) = i
				    // otherwise, a mesh effect due to discontinuities in indexx and indexy.
				    // The if statement is just a sanity check.
				    if ( (indexx.at<int>(i,j) >= 0 ) && (indexx.at<int>(i,j) < map_x.cols )
						&& (indexu.at<int>(i,j) >= 0 ) && (indexu.at<int>(i,j) < map_x.cols )
						&& (indexy.at<int>(i,j) >= 0 ) && (indexy.at<int>(i,j) < map_x.rows )
						&& (indexv.at<int>(i,j) >= 0 ) && (indexv.at<int>(i,j) < map_x.rows ) )
					{	
						map_x.at<float>(i,j) = (float) indexu.at<int>(i,j);
						map_y.at<float>(i,j) = (float) indexv.at<int>(i,j);
						//debug
						//~ if ((indexx.at<int>(i,j) == 73) && (indexy.at<int>(i,j) == 75) )
						//~ {
							//~ std::cout << "not black(" <<i<< "," << j << ")" <<std::endl;
							//~ std::cout << "x,y,u,v=";
							//~ std::cout << indexx.at<int>(i,j) << ", ";
							//~ std::cout << indexy.at<int>(i,j) << ", ";
							//~ std::cout << indexu.at<int>(i,j) << ", ";
							//~ std::cout << indexv.at<int>(i,j) << std::endl;
						//~ }
						//~ if (i==75)
						 //~ if (j<80 && j>=0)
						 //~ {
							//~ std::cout << "not black(" <<i<< "," << j << ")" <<std::endl;
							//~ std::cout << "x,y,u,v=";
							//~ std::cout << indexx.at<int>(i,j) << ", ";
							//~ std::cout << indexy.at<int>(i,j) << ", ";
							//~ std::cout << indexu.at<int>(i,j) << ", ";
							//~ std::cout << indexv.at<int>(i,j) << std::endl;
						//~ }
							
				    } // end if
				    
				    //~ else
				    //~ {
						//debug
						//~ std::cout << "skipped(" <<i<< "," << j << ")" <<std::endl;
						//~ std::cout << "x,y,u,v=";
						//~ std::cout << indexx.at<int>(i,j) << ", ";
						//~ std::cout << indexy.at<int>(i,j) << ", ";
						//~ std::cout << indexu.at<int>(i,j) << ", ";
						//~ std::cout << indexv.at<int>(i,j) << std::endl;
					//~ }
				}
			}
		return;
	}
	break;
		
	case 3:
	//if (transformtype == 3)	//  fisheye to Equirectangular - dual output - using parallel projection
	{
		// int xcd = floor(map_x.cols/2) - 1 + anglex;	// this just 'pans' the view
		// int ycd = floor(map_x.rows/2) - 1 + angley;
		int xcd = floor(map_x.cols/2) - 1 ;
		int ycd = floor(map_x.rows/2) - 1 ;
		int xd, yd;
		float px_per_theta = map_x.cols * 2 / (2*CV_PI); 	// src width = map_x.cols * 2
		float px_per_phi   = map_x.rows / CV_PI;			// src height = PI for equirect 360
		float rad_per_px = CV_PI / map_x.rows;
		float rd, theta, phiang, temp;
		float longi, lat, Px, Py, Pz, R;						// X and Y are map_x and map_y
		float aperture = CV_PI;
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					longi 	= (CV_PI    ) * (j - xcd) / (map_x.cols/2) + anglex;		// longi = x.pi for 360 image
					lat	 	= (CV_PI / 2) * (i - ycd) / (map_x.rows/2) + angley;		// lat = y.pi/2
					
					Px = cos(lat)*cos(longi);
					Py = cos(lat)*sin(longi);
					Pz = sin(lat);
					
					if (Px == 0 && Py == 0 && Pz == 0)
						R = 0;
					else 
						R = 2 * atan2(sqrt(Px*Px + Pz*Pz), Py) / aperture; 	
					
					if (Px == 0 && Pz ==0)
						theta = 0;
					else
						theta = atan2(Pz, Px);
						
					
					// map_x.at<float>(i, j) = R * cos(theta); this maps to [-1, 1]
					//map_x.at<float>(i, j) = R * cos(theta) * map_x.cols / 2 + xcd;
					map_x.at<float>(i, j) = - Px * map_x.cols / 2 + xcd;
					
					// this gives two copies in final output, top one reasonably correct
					
					// map_y.at<float>(i, j) = R * sin(theta); this maps to [-1, 1]
					//map_y.at<float>(i, j) = R * sin(theta) * map_x.rows / 2 + ycd;
					map_y.at<float>(i, j) = Py * map_x.rows / 2 + ycd;
					
				 } // for j
				   
			} // for i
			
	}
	break;
	
	case 2:
	//if (transformtype == 2)	// 360 degree fisheye to Equirectangular 360 
	{
		// int xcd = floor(map_x.cols/2) - 1 + anglex;	// this just 'pans' the view
		// int ycd = floor(map_x.rows/2) - 1 + angley;
		int xcd = floor(map_x.cols/2) - 1 ;
		int ycd = floor(map_x.rows/2) - 1 ;
		int xd, yd;
		float px_per_theta = map_x.cols  / (2*CV_PI); 	//  width = map_x.cols 
		float px_per_phi   = map_x.rows / CV_PI;		//  height = PI for equirect 360
		float rad_per_px = CV_PI / map_x.rows;
		float rd, theta, phiang, temp;
		float longi, lat, Px, Py, Pz, R;						// X and Y are map_x and map_y
		float aperture = 2*CV_PI;
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					longi 	= (CV_PI    ) * (j - xcd) / (map_x.cols/2) + anglex;		// longi = x.pi for 360 image
					lat	 	= (CV_PI / 2) * (i - ycd) / (map_x.rows/2) + angley;		// lat = y.pi/2
					
					Px = cos(lat)*cos(longi);
					Py = cos(lat)*sin(longi);
					Pz = sin(lat);
					
					if (Px == 0 && Py == 0 && Pz == 0)
						R = 0;
					else 
						R = 2 * atan2(sqrt(Px*Px + Py*Py), Pz) / aperture; 
						// exchanged Py and Pz from Paul's co-ords, 	
						// from Perspective projection the wrong imaging model 10.1.1.52.8827.pdf
						// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.52.8827&rep=rep1&type=pdf
						// Or else, Africa ends up sideways, and with the far east and west streched out on top and bottom
					
					if (Px == 0 && Pz ==0)
						theta = 0;
					else
						theta = atan2(Py, Px);
						
					
					// map_x.at<float>(i, j) = R * cos(theta); this maps to [-1, 1]
					map_x.at<float>(i, j) =  R * cos(theta) * map_x.cols / 2 + xcd;
					
					// currently upside down 
					
					// map_y.at<float>(i, j) = R * sin(theta); this maps to [-1, 1]
					map_y.at<float>(i, j) =  R * sin(theta) * map_x.rows / 2 + ycd;
					
					
				 } // for j
				   
			} // for i
			
	}
	break;
	
	case 1:
	//if (transformtype == 1)	// Equirectangular 360 to 180 degree fisheye
	{
		// using the transformations at
		// http://paulbourke.net/dome/dualfish2sphere/diagram.pdf
		int xcd = floor(map_x.cols/2) - 1 ;
		int ycd = floor(map_x.rows/2) - 1 ;
		float halfcols = map_x.cols/2;
		float halfrows = map_x.rows/2;
		
		
		float longi, lat, Px, Py, Pz, R, theta;						// X and Y are map_x and map_y
		float xfish, yfish, rfish, phi, xequi, yequi;
		float PxR, PyR, PzR;
		float aperture = CV_PI;
		float angleyrad = -angley*CV_PI/180;	// made these minus for more intuitive feel
		float anglexrad = -anglex*CV_PI/180;
		
		//Mat inputmatrix, rotationmatrix, outputmatrix;
		// https://en.wikipedia.org/wiki/Rotation_matrix#Basic_rotations
		//rotationmatrix = (Mat_<float>(3,3) << cos(angleyrad), 0, sin(angleyrad), 0, 1, 0, -sin(angleyrad), 0, cos(angleyrad)); //y
		//rotationmatrix = (Mat_<float>(3,3) << 1, 0, 0, 0, cos(angleyrad), -sin(angleyrad), 0, sin(angleyrad), cos(angleyrad)); //x
		//rotationmatrix = (Mat_<float>(3,3) << cos(angleyrad), -sin(angleyrad), 0, sin(angleyrad), cos(angleyrad), 0, 0, 0, 1); //z
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					// normalizing to [-1, 1]
					xfish = (j - xcd) / halfcols;
					yfish = (i - ycd) / halfrows;
					rfish = sqrt(xfish*xfish + yfish*yfish);
					theta = atan2(yfish, xfish);
					phi = rfish*aperture/2;
					
					// Paul's co-ords - this is suitable when phi=0 is Pz=0
					
					//Px = cos(phi)*cos(theta);
					//Py = cos(phi)*sin(theta);
					//Pz = sin(phi);
					
					// standard co-ords - this is suitable when phi=pi/2 is Pz=0
					Px = sin(phi)*cos(theta);
					Py = sin(phi)*sin(theta);
					Pz = cos(phi);
					
					if(angley!=0 || anglex!=0)
					{
						// cos(angleyrad), 0, sin(angleyrad), 0, 1, 0, -sin(angleyrad), 0, cos(angleyrad));
						
						PxR = Px;
						PyR = cos(angleyrad) * Py - sin(angleyrad) * Pz;
						PzR = sin(angleyrad) * Py + cos(angleyrad) * Pz;
						
						Px = cos(anglexrad) * PxR - sin(anglexrad) * PyR;
						Py = sin(anglexrad) * PxR + cos(anglexrad) * PyR;
						Pz = PzR;
					}
					
					
					longi 	= atan2(Py, Px);
					lat	 	= atan2(Pz,sqrt(Px*Px + Py*Py));	
					// this gives south pole centred, ie yequi goes from [-1, 0]
					// Made into north pole centred by - (minus) in the final map_y assignment
					
					xequi = longi / CV_PI;
					// this maps to [-1, 1]
					yequi = 2*lat / CV_PI;
					// this maps to [-1, 0] for south pole
					
					if (rfish <= 1)		// outside that circle, let it be black
					{
						map_x.at<float>(i, j) =  xequi * map_x.cols / 2 + xcd;
						//map_y.at<float>(i, j) =  yequi * map_x.rows / 2 + ycd;
						// this gets south pole centred view
						
						map_y.at<float>(i, j) =  yequi * map_x.rows / 2 + ycd;
					}
					
				 } // for j
				   
			} // for i
			
	}
	break;
	
	case 0:
	default:
	//else
	//if (transformtype == 0) // the default // Equirectangular 360 to 360 degree fisheye
	{
		// using code adapted from http://www.fmwconcepts.com/imagemagick/pano2fisheye/index.php
			// set destination (output) centers
			int xcd = floor(map_x.cols/2) - 1;
			int ycd = floor(map_x.rows/2) - 1;
			int xd, yd;
			//define destination (output) coordinates center relative xd,yd
			// "xd= x - xcd;"
			// "yd= y - ycd;"

			// compute input pixels per angle in radians
			// theta ranges from -180 to 180 = 360 = 2*pi
			// phi ranges from 0 to 90 = pi/2
			float px_per_theta = map_x.cols / (2*CV_PI);
			float px_per_phi   = map_x.rows / (CV_PI/2);
			// compute destination radius and theta 
			float rd; // = sqrt(x^2+y^2);
			
			// set theta so original is north rather than east
			float theta; //= atan2(y,x);
			
			// convert radius to phiang according to fisheye mode
			//if projection is linear then
			//	 destination output diameter (dimensions) corresponds to 180 deg = pi (fov); angle is proportional to radius
			float rad_per_px = CV_PI / map_x.rows;
			float phiang;     // = rad_per_px * rd;
			

			// convert theta to source (input) xs and phi to source ys
			// -rotate 90 aligns theta=0 with north and is faster than including in theta computation
			// y corresponds to h-phi, so that bottom of the input is center of output
			// xs = width + theta * px_per_theta;
			// ys = height - phiang * px_per_phi;
			
			
			for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					xd = j - xcd;
					yd = i - ycd;
					if (xd == 0 && yd == 0)
					{
						theta = 0 + anglex*CV_PI/180;
						rd = 0;
					}
					else
					{
						//theta = atan2(float(yd),float(xd)); // this sets orig to east
						// so America, at left of globe, becomes centred
						theta = atan2(xd,yd) + anglex*CV_PI/180;; // this sets orig to north
						// makes the fisheye left/right flipped if atan2(-xd,yd)
						// so that Africa is centred when anglex = 0.
						rd = sqrt(float(xd*xd + yd*yd));
					}
					// move theta to [-pi, pi]
					theta = fmod(theta+CV_PI, 2*CV_PI);
					if (theta < 0)
						theta = theta + CV_PI;
					theta = theta - CV_PI;	
					
					//phiang = rad_per_px * rd + angley*CV_PI/180; // this zooms in/out, not rotate cam
					phiang = rad_per_px * rd;
					
					map_x.at<float>(i, j) = (float)round((map_x.cols/2) + theta * px_per_theta);
					
					//map_y.at<float>(i, j) = (float)round((map_x.rows) - phiang * px_per_phi);
					// this above makes the south pole the centre.
					
					map_y.at<float>(i, j) = phiang * px_per_phi;
					// this above makes the north pole the centre of the fisheye
					// map_y.at<float>(i, j) = phiang * px_per_phi - angley; //this just zooms out
					
					
					 
				   // the following test mapping just makes the src upside down in dst
				   // map_x.at<float>(i, j) = (float)j;
				   // map_y.at<float>(i, j) = (float)( i); 
				   
				 } // for j
				   
			} // for i
				
            
     
     } // end of if transformtype == 0
	}	// end switch case
    
    
// debug
    /*
    std::cout << "map_x -> " << std::endl;
    
    for ( int i = 0; i < map_x.rows; i+=100 ) // here, i is for y and j is for x
    {
        for ( int j = 0; j < map_x.cols; j+=100 )
        {
			std::cout << map_x.at<float>(i, j) << " " ;
		}
		std::cout << std::endl;
	}
	
	std::cout << "map_y -> " << std::endl;
    
    for ( int i = 0; i < map_x.rows; i+=100 ) // here, i is for y and j is for x
    {
        for ( int j = 0; j < map_x.cols; j+=100 )
        {
			std::cout << map_y.at<float>(i, j) << " " ;
		}
		std::cout << std::endl;
	}
	* */    
    
} // end function updatemap

std::string escaped(const std::string& input)
{
	// https://stackoverflow.com/questions/48260879/how-to-replace-with-in-c-string
    std::string output;
    output.reserve(input.size());
    for (const char c: input) {
        switch (c) {
            case '\a':  output += "\\a";        break;
            case '\b':  output += "\\b";        break;
            case '\f':  output += "\\f";        break;
            case '\n':  output += "\\n";        break;
            case '\r':  output += "\\r";        break;
            case '\t':  output += "\\t";        break;
            case '\v':  output += "\\v";        break;
            default:    output += c;            break;
        }
    }

    return output;
}


int main(int argc,char *argv[])
{
	////////////////////////////////////////////////////////////////////
	// Initializing variables
	////////////////////////////////////////////////////////////////////
	bool doneflag = 0, interactivemode = 0;
    double anglex = 0;
    double angley = 0;
    
    int outputw = 1920;
    int outputh = 1080;
    
    int texturew = 2048;
    
    std::string tempstring;
    //std::string strpathtowarpfile; making this a global var
    strpathtowarpfile = "EP_xyuv_1920.map";
    char anglexstr[40];
    char angleystr[40];
    char outputfourccstr[40];	// leaving extra chars for not overflowing too easily
    outputfourccstr[0] = 'N';
    outputfourccstr[1] = 'U';
    outputfourccstr[2] = 'L';
    outputfourccstr[3] = 'L';
    
    //const bool askOutputType = argv[3][0] =='Y';  // If false it will use the inputs codec type
    // this line above causes the windows build to not run! although it compiles ok.
    // askOutputType=1 works only on Windows currently
    const bool askOutputType = 0;
    
    std::ifstream infile("OCVWarp.ini");
    
    int transformtype = 0;
    // 0 = Equirectangular to 360 degree fisheye
    // 1 = Equirectangular to 180 degree fisheye
    
    int ind = 1;
    // inputs from ini file
    if (infile.is_open())
		  {
			
			infile >> tempstring;
			infile >> tempstring;
			infile >> tempstring;
			// first three lines of ini file are comments
			infile >> anglexstr;
			infile >> tempstring;
			infile >> angleystr;
			infile >> tempstring;
			infile >> outputw;
			infile >> tempstring;
			infile >> outputh;
			infile >> tempstring;
			infile >> transformtype;
			infile >> tempstring;
			infile >> outputfourccstr;
			infile >> tempstring;
			infile >> strpathtowarpfile;
			infile.close();
			
			anglex = atof(anglexstr);
			angley = atof(angleystr);
		  }

	else std::cout << "Unable to open ini file, using defaults." << std::endl;
	
	std::cout << "Output codec type: " << outputfourccstr << std::endl;
	
	namedWindow("Display", WINDOW_NORMAL | WINDOW_KEEPRATIO); // 0 = WINDOW_NORMAL
	resizeWindow("Display", round(outputw/outputh*600), 600); // this doesn't work?
	moveWindow("Display", 0, 0);
	
	char const * FilterPatterns[2] =  { "*.avi","*.*" };
	char const * OpenFileName = tinyfd_openFileDialog(
		"Open a video file",
		"",
		2,
		FilterPatterns,
		NULL,
		0);

	if (! OpenFileName)
	{
		tinyfd_messageBox(
			"Error",
			"No file chosen. ",
			"ok",
			"error",
			1);
		return 1 ;
	}
	
	// reference:
	// https://docs.opencv.org/3.4/d7/d9e/tutorial_video_write.html
	
#ifdef __unix__
	
	VideoCapture inputVideo(OpenFileName);              // Open input
#endif
#ifdef _WIN64
	// Here, OpenCV on Windows needs escaped file paths. 
	// https://stackoverflow.com/questions/48260879/how-to-replace-with-in-c-string
	std::string escapedpath = escaped(std::string(OpenFileName));
	VideoCapture inputVideo(escapedpath.c_str());              // Open input
#endif

#ifdef __MINGW32__
	// Here, OpenCV on Windows needs escaped file paths. 
	// https://stackoverflow.com/questions/48260879/how-to-replace-with-in-c-string
	std::string escapedpath = escaped(std::string(OpenFileName));
	VideoCapture inputVideo(escapedpath.c_str());              // Open input
#endif
#ifdef __MINGW64__
	// Here, OpenCV on Windows needs escaped file paths. 
	// https://stackoverflow.com/questions/48260879/how-to-replace-with-in-c-string
	std::string escapedpath = escaped(std::string(OpenFileName));
	VideoCapture inputVideo(escapedpath.c_str());              // Open input
#endif

	
	if (!inputVideo.isOpened())
    {
        std::cout  << "Could not open the input video: " << OpenFileName << std::endl;
        return -1;
    }
     
#ifdef __unix__
    std::string OpenFileNamestr = OpenFileName; 
#endif
#ifdef _WIN64
	// Here, OpenCV on Windows needs escaped file paths. 
	std::string OpenFileNamestr = escapedpath; 
#endif   
#ifdef __MINGW32__
	// Here, OpenCV on Windows needs escaped file paths. 
	std::string OpenFileNamestr = escapedpath; 
#endif   
#ifdef __MINGW64__
	// Here, OpenCV on Windows needs escaped file paths. 
	std::string OpenFileNamestr = escapedpath; 
#endif   

    std::string::size_type pAt = OpenFileNamestr.find_last_of('.');                  // Find extension point
    std::string NAME = OpenFileNamestr.substr(0, pAt) + "F" + ".avi";   // Form the new name with container
    
    // here, we give an option for the user to choose the output file
    // path as well as type (container, like mp4, mov, avi).
    char const * SaveFileName = tinyfd_saveFileDialog(
		"Now enter the output video file name, like output.mp4",
		"",
		0,
		NULL,
		NULL);

	if (! SaveFileName)
	{
		tinyfd_messageBox(
			"No output file chosen.",
			"Will be saved as inputfilename + F.avi",
			"ok",
			"info",
			1);
		 
	}
	else
	{
#ifdef __unix__
	NAME = std::string(SaveFileName);
#else
	// for Windows, escape the \ characters in the path
	escapedpath = escaped(std::string(SaveFileName));
	NAME = escapedpath;
#endif
	}
	
    
    int ex = static_cast<int>(inputVideo.get(CAP_PROP_FOURCC));     // Get Codec Type- Int form
    // Transform from int to char via Bitwise operators
    char EXT[] = {(char)(ex & 0XFF) , (char)((ex & 0XFF00) >> 8),(char)((ex & 0XFF0000) >> 16),(char)((ex & 0XFF000000) >> 24), 0};
    Size S = Size((int) inputVideo.get(CAP_PROP_FRAME_WIDTH),    // Acquire input size
                  (int) inputVideo.get(CAP_PROP_FRAME_HEIGHT));
    Size Sout = Size(outputw,outputh);            
    VideoWriter outputVideo;                                        // Open the output
#ifdef __unix__
    if (!(outputfourccstr[0] == 'N' &&
    outputfourccstr[1] == 'U' &&
    outputfourccstr[2] == 'L' &&
    outputfourccstr[3] == 'L'))
        outputVideo.open(NAME, outputVideo.fourcc(outputfourccstr[0], outputfourccstr[1], outputfourccstr[2], outputfourccstr[3]), 
        inputVideo.get(CAP_PROP_FPS), Sout, true);
    else
        outputVideo.open(NAME, ex, inputVideo.get(CAP_PROP_FPS), Sout, true);
#endif
#ifdef _WIN64
	// OpenCV on Windows can ask for a suitable fourcc. 
	outputVideo.open(NAME, -1, inputVideo.get(CAP_PROP_FPS), Sout, true);
#endif  
#ifdef __MINGW32__
    if (!(outputfourccstr[0] == 'N' &&
    outputfourccstr[1] == 'U' &&
    outputfourccstr[2] == 'L' &&
    outputfourccstr[3] == 'L'))
        outputVideo.open(NAME, outputVideo.fourcc(outputfourccstr[0], outputfourccstr[1], outputfourccstr[2], outputfourccstr[3]), 
        inputVideo.get(CAP_PROP_FPS), Sout, true);
    else
        outputVideo.open(NAME, ex, inputVideo.get(CAP_PROP_FPS), Sout, true);
#endif
#ifdef __MINGW64__
    if (!(outputfourccstr[0] == 'N' &&
    outputfourccstr[1] == 'U' &&
    outputfourccstr[2] == 'L' &&
    outputfourccstr[3] == 'L'))
        outputVideo.open(NAME, outputVideo.fourcc(outputfourccstr[0], outputfourccstr[1], outputfourccstr[2], outputfourccstr[3]), 
        inputVideo.get(CAP_PROP_FPS), Sout, true);
    else
        outputVideo.open(NAME, ex, inputVideo.get(CAP_PROP_FPS), Sout, true);
#endif

    if (!outputVideo.isOpened())
    {
        std::cout  << "Could not open the output video for write: " << NAME << std::endl;
        return -1;
    }
    std::cout << "Input frame resolution: Width=" << S.width << "  Height=" << S.height
         << " of nr#: " << inputVideo.get(CAP_PROP_FRAME_COUNT) << std::endl;
    std::cout << "Input codec type: " << EXT << std::endl;
     
    int  fps, key;
	int t_start, t_end;
    unsigned long long framenum = 0;
     
    Mat src, res, tmp;
    Mat dstfloat, dstmult, dstres, dstflip;
    
    std::vector<Mat> spl;
    Mat dst(Sout, CV_8UC3); // S = src.size, and src.type = CV_8UC3
    Mat dst2;	// temp dst, for double remap
    Mat map_x, map_y;
    if ((transformtype == 4) || (transformtype == 5) ) 
    {
		//~ map_x = Mat(Size(outputw*2,outputh*2), CV_32FC1);	// for 2x resampling
		//~ map_y = Mat(Size(outputw*2,outputh*2), CV_32FC1);
		// the above code causes gridlines to appear in output
		if (outputw<961)	//1K
			texturew = 1024;
			//~ // debug
			//~ texturew = outputw;
		else if (outputw<1921)	//2K
			texturew = 2048;
		else if (outputw<3841)	//4K
			texturew = 4096;
		else // (outputw<7681)	//8K
			texturew = 8192;
			// debug - had set Size to outputw,h
		map_x = Mat(Size(texturew,texturew), CV_32FC1);	// for upsampling
		map_y = Mat(Size(texturew,texturew), CV_32FC1);
		if (transformtype == 5)
		{
			dst2 = Mat(Size(texturew,texturew), CV_32FC1);
		}
	}
	else
	{
		map_x = Mat(Sout, CV_32FC1);
		map_y = Mat(Sout, CV_32FC1);
	}
    Mat dst_x, dst_y;
    // Mat map2x, map2y // these are made global vars 
    Mat dst2x, dst2y;	// for transformtype=5, double remap
    if (transformtype == 5) 
    {
		map2x = Mat(Size(texturew,texturew), CV_32FC1);	
		map2y = Mat(Size(texturew,texturew), CV_32FC1);
		map2x = Scalar((texturew+texturew)*10);
		map2y = Scalar((texturew+texturew)*10);
		// initializing so that it points outside the image
		// so that unavailable pixels will be black
	}
	
    map_x = Scalar((outputw+outputh)*10);
    map_y = Scalar((outputw+outputh)*10);
    // initializing so that it points outside the image
    // so that unavailable pixels will be black
    
    if (transformtype == 5) 
    {
		update_map(anglex, angley, map_x, map_y, 4);
		// first initialize map_x and map_y for final warp,
		// which is exactly like transformtype=4
		update_map(anglex, angley, map2x, map2y, 5);
		convertMaps(map_x, map_y, dst_x, dst_y, CV_16SC2);	// supposed to make it faster to remap
		convertMaps(map2x, map2y, dst2x, dst2y, CV_16SC2);
		
	}
	else
	{
		update_map(anglex, angley, map_x, map_y, transformtype);
		convertMaps(map_x, map_y, dst_x, dst_y, CV_16SC2);	// supposed to make it faster to remap
	}
    
    t_start = time(NULL);
	fps = 0;
	
    for(;;)
    {
        inputVideo >> src;              // read
        if (src.empty()) break;         // check if at end
        //imshow("Display",src);
        key = waitKey(10);
        
        if(interactivemode)
        {
			if (transformtype==5)
			{
				// only map2 needs to be updated
			update_map(anglex, angley, map2x, map2y, 5);
			convertMaps(map2x, map2y, dst2x, dst2y, CV_16SC2);	// supposed to make it faster to remap
			}
			else
			{
			update_map(anglex, angley, map_x, map_y, transformtype);
			convertMaps(map_x, map_y, dst_x, dst_y, CV_16SC2);	// supposed to make it faster to remap
			}
    
			interactivemode = 0;
		}
		
		switch (transformtype)
				{
				case 5: // 360 to 180 fisheye and then to warped
					resize( src, res, Size(texturew, texturew), 0, 0, INTER_CUBIC);
					break;

				case 4: // 180 fisheye to warped
					// the transform needs a flipped source image, flipud
					flip(src, src, 0);	// because the mesh assumes 0,0 is bottom left
					//debug - had changed to outputw, h
					resize( src, res, Size(texturew, texturew), 0, 0, INTER_CUBIC);
					break;

				case 3: // 360 fisheye to Equirect
					resize( src, res, Size(outputw, outputh), 0, 0, INTER_CUBIC);
					break;
					
				case 2: // 360 fisheye to Equirect
					resize( src, res, Size(outputw, outputh), 0, 0, INTER_CUBIC);
					break;
					
				case 1: // Equirect to 180 fisheye
					resize( src, res, Size(outputw, outputh), 0, 0, INTER_CUBIC);
					break;
				
				default:	
				case 0: // Equirect to 360 fisheye
					resize( src, res, Size(outputw, outputh), 0, 0, INTER_CUBIC);
					break;
					
				}
		if (transformtype == 5)
		{
			// here we have two remaps
			remap( res, dst2, dst2x, dst2y, INTER_LINEAR, BORDER_CONSTANT, Scalar(0, 0, 0) );
			// the second remap needs flipping, like transformtype=4
			flip(dst2, dst2, 0);	// because the mesh assumes 0,0 is bottom left
			remap( dst2, dst, dst_x, dst_y, INTER_LINEAR, BORDER_CONSTANT, Scalar(0, 0, 0) );
		}
		else
		{
        remap( res, dst, dst_x, dst_y, INTER_LINEAR, BORDER_CONSTANT, Scalar(0, 0, 0) );
		}
	
        if ((transformtype == 4) || (transformtype == 5) )
        {
			// multiply by the intensity Mat
			dst.convertTo(dstfloat, CV_32FC3);
			multiply(dstfloat, I, dstmult);
			//debug
			//dstmult = dstfloat;
			dstmult.convertTo(dstres, CV_8UC3);
			// this transform is 2x2 oversampled
			resize(dstres, dstflip, Size(outputw,outputh), 0, 0, INTER_AREA);
			flip(dstflip, dst, 0); 	// flip up down again
		}
			
        
        imshow("Display", dst);
        //std::cout << "\x1B[2K"; // Erase the entire current line.
#ifdef __unix__
        std::cout << "\x1B[0E"; // Move to the beginning of the current line.
#else
		//std::cout << std::endl;
#endif
        
        fps++;
        t_end = time(NULL);
		if (t_end - t_start >= 5)
		{
#ifdef __unix__
			std::cout << "Frame: " << framenum++ << " x: " << anglex << " y: " << angley << " fps: " << fps/5 << std::flush;
#else
			std::cout << "Frame: " << framenum++ << " x: " << anglex << " y: " << angley << " fps: " << fps/5 << std::endl;
#endif
			t_start = time(NULL);
			fps = 0;
		}
		else
#ifdef __unix__		
		std::cout << "Frame: " << framenum++ << " x: " << anglex << " y: " << angley << std::flush;
#else
        framenum++; 
#endif
        
       //outputVideo.write(res); //save or
       outputVideo << dst;
       
       
       switch (key)
				{

				case 27: //ESC key
				case 'x':
				case 'X':
					doneflag = 1;
					break;

				case 'u':
				case '+':
				case '=':	// increase angley
					angley = angley + 1.0;
					interactivemode = 1;
					break;
					
				case 'm':
				case '-':
				case '_':	// decrease angley
					angley = angley - 1.0;
					interactivemode = 1;
					break;
					
				case 'k':
				case '}':
				case ']':	// increase anglex
					anglex = anglex + 1.0;
					interactivemode = 1;
					break;
					
				case 'h':
				case '{':
				case '[':	// decrease anglex
					anglex = anglex - 1.0;
					interactivemode = 1;
					break;
				
				case 'U':
					// increase angley
					angley = angley + 10.0;
					interactivemode = 1;
					break;
					
				case 'M':
					// decrease angley
					angley = angley - 10.0;
					interactivemode = 1;
					break;
					
				case 'K':
					// increase anglex
					anglex = anglex + 10.0;
					interactivemode = 1;
					break;
					
				case 'H':
					// decrease anglex
					anglex = anglex - 10.0;
					interactivemode = 1;
					break;	
					
				default:
					break;
				
				}
				
		if (doneflag == 1)
		{
			break;
		}
    } // end for(;;) loop
    
    std::cout << std::endl << "Finished writing" << std::endl;
    return 0;
	   
	   
} // end main
