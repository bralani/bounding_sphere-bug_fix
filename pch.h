#pragma once

// OpenCV header files
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include "opencv2/highgui/highgui.hpp"

// Necessary C++ header files
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <sys/stat.h> 
#include "picohash.h"
#include<cstdio>
#include<filesystem>
#include <sys/stat.h>
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#ifdef HAVE_WINDOWS_H
    #include <windows.h>
    #include <stdio.h>
    #include <aclapi.h>
#endif 

// BRL-CAD header files
#include "bu/getopt.h"
#include "bu/log.h"
#include "ged.h"

// Visualization project header files
#include "Options.h"
#include "IFPainter.h"
#include "InformationGatherer.h"
#include "PerspectiveGatherer.h"
#include "RenderHandler.h"
#include "FactsHandler.h"
#include "Position.h"
