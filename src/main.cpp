#include<iostream>
#include<cstdlib>
#include<string>

#include<opencv2/opencv.hpp>
#include<opencv2/core/core.hpp>
#include "opencv2/highgui/highgui.hpp"

// /brlcad/build/share/db/moss.g
// /brlcad/src/gtools/rgen/visualization
// ../../../../../build/bin/mged ../db/moss.g tops
// ../../../../../build/bin/rt ../db/moss.g all.g

using namespace cv;

int main(int argc, char **argv) {
    if (argc == 2) {
        const char* filename = argv[1];
        std::cout << "Processing file: " << filename << std::endl;
        const char* mgedTops = "../../../../../build/bin/mged ../db/moss.g tops";
        auto result1 = system(mgedTops);
        const char* rtAll = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" ../db/moss.g all.g";
        auto result2 = system(rtAll);
        std::cout << "Success\n";
    } else {
            // To create an image
            // CV_8UC3 depicts : (3 channels,8 bit image depth)
            // Height  = 750 pixels, Width = 1500 pixels
            // Background is set to white
            Mat img(750, 1500, CV_8UC3, Scalar(255,255,255));

            putText(img, "Report:", Point(0, 50), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 0, 0), 1);

            // check whether the image is loaded or not
            if (img.empty())
            {
                std::cout << "\n Image not created. You"
                    " have done something wrong. \n";
                return -1;    // Unsuccessful.
            }

            // first argument: name of the window
            // second argument: flag- types: 
            // WINDOW_NORMAL If this is set, the user can 
            //               resize the window.
            // WINDOW_AUTOSIZE If this is set, the window size
            //                is automatically adjusted to fit 
            //                the displayed image, and you cannot
            //                change the window size manually.
            // WINDOW_OPENGL  If this is set, the window will be
            //                created with OpenGL support.
            namedWindow("Report", WINDOW_AUTOSIZE);

            //Saves to png
            imwrite("report.png", img);

            // first argument: name of the window
            // second argument: image to be shown(Mat object)
            imshow("Report", img);

            /*  Mat whiteMatrix(200, 200, CV_8UC3, Scalar(255, 255, 255));//Declaring a white matrix
                Point center(100, 100);//Declaring the center point
                int radius = 50; //Declaring the radius
                Scalar line_Color(0, 0, 0);//Color of the circle
                int thickness = 2;//thickens of the line
                namedWindow("whiteMatrix");//Declaring a window to show the circle
                circle(whiteMatrix, center,radius, line_Color, thickness);//Using circle()function to draw the line//
                imshow("WhiteMatrix", whiteMatrix);//Showing the circle//*/

            waitKey(0); //wait infinite time for a keypress

            // destroy the window with the name, "MyWindow"
            destroyWindow("Report");
        return 1;
    }
}

