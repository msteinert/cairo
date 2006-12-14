/*
  PerceptualDiff - a program that compares two images using a perceptual metric
  based on the paper :
  A perceptual metric for production testing. Journal of graphics tools, 9(4):33-40, 2004, Hector Yee
  Copyright (C) 2006 Yangli Hector Yee

  This program is free software; you can redistribute it and/or modify it under the terms of the
  GNU General Public License as published by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with this program;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include "lpyramid.h"
#include "RGBAImage.h"
#include "CompareArgs.h"
#include "Metric.h"

static bool Yee_Compare(CompareArgs &args)
{
    if ((args.ImgA->Get_Width() != args.ImgB->Get_Width()) ||
	(args.ImgA->Get_Height() != args.ImgB->Get_Height())) {
	args.ErrorStr = "Image dimensions do not match\n";
	return false;
    }

    unsigned int i, dim, pixels_failed;
    dim = args.ImgA->Get_Width() * args.ImgA->Get_Height();
    bool identical = true;
    for (i = 0; i < dim; i++) {
	if (args.ImgA->Get(i) != args.ImgB->Get(i)) {
	    identical = false;
	    break;
	}
    }
    if (identical) {
	args.ErrorStr = "Images are binary identical\n";
	return true;
    }

    pixels_failed = Yee_Compare_Images (args.ImgA, args.ImgB,
					args.Gamma, args.Luminance,
					args.FieldOfView, args.Verbose);

    if (pixels_failed < args.ThresholdPixels) {
	args.ErrorStr = "Images are perceptually indistinguishable\n";
	return true;
    }

    char different[100];
    sprintf(different, "%d pixels are different\n", pixels_failed);

    args.ErrorStr = "Images are visibly different\n";
    args.ErrorStr += different;

    if (args.ImgDiff) {
#if IMAGE_DIFF_CODE_ENABLED
	if (args.ImgDiff->WritePPM()) {
	    args.ErrorStr += "Wrote difference image to ";
	    args.ErrorStr+= args.ImgDiff->Get_Name();
	    args.ErrorStr += "\n";
	} else {
	    args.ErrorStr += "Could not write difference image to ";
	    args.ErrorStr+= args.ImgDiff->Get_Name();
	    args.ErrorStr += "\n";
	}
#endif
	args.ErrorStr += "Generation of image \"difference\" is currently disabled\n";
    }
    return false;
}

int main(int argc, char **argv)
{
    CompareArgs args;

    if (!args.Parse_Args(argc, argv)) {
	printf("%s", args.ErrorStr.c_str());
	return -1;
    } else {
	if (args.Verbose) args.Print_Args();
    }
    int result = Yee_Compare(args) == true;
    if (result) {
	printf("PASS: %s\n", args.ErrorStr.c_str());
    } else {
	printf("FAIL: %s\n", args.ErrorStr.c_str());
    }
    return result;
}
