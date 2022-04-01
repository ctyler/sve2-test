/*

  sve2-test
  
  (C)2022 Seneca College of Applied Arts and Technology.
  Written by Chris Tyler. Licensed under the terms of the GPL verion 2.
  
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

// adjust_channels is where all the real action is
// this file is just scaffolding
#include "adjust_channels.h"

// Using the STBI image reader/writer
// See https://github.com/nothings/stb
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main(int argc, char *argv[]) {

	// ==================== Load the image file (arg 1)
	int x, y, n;
	unsigned char *image = stbi_load(argv[1], 
		&x, &y, &n, 3);

	printf("x:%d\ty:%d\tn:%d\n", x, y, n);

	if (image == NULL) {
		dprintf(2, "Invalid argument or file did not load.\n");
		return 1;
	}
	
	// ==================== Adjust the channels
	
	// Get arguments 2, 3, and 4; each should be a number in the range 0.0 .. 1.0
	// Yes this is ugly and should be improved, this is a quick & dirty test program :-)
	float redarg   = MIN(2, MAX(0, strtof(argv[2],NULL)));
	float greenarg = MIN(2, MAX(0, strtof(argv[3],NULL)));
	float bluearg  = MIN(2, MAX(0, strtof(argv[4],NULL)));
	
	printf("red: %f\tgreen: %f\tblue: %f\n", redarg, greenarg, bluearg);
	
	adjust_channels(image, x, y, redarg, greenarg, bluearg);

	// ==================== Save the resulting file (jpg) (arg 5)
	stbi_write_jpg(argv[5], x, y, n, image, 90);
}

