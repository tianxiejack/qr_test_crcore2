
#include "main.h"
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <osa.h>

int main(int argc, char **argv)
{
#if defined(__linux__)
    setenv ("DISPLAY", ":0", 0);
    printf("\n setenv DISPLAY=%s\n",getenv("DISPLAY"));
#endif
	M_MAIN(argc, argv);
}
