/*

	adjust_channel :: adjust red/green/blue colour channels in an image
	
	Three implementations are provided, selected by ADJUST_CHANNEL_IMPLEMENTATION
	
	1. Naive implementation in C. Math is floating point, with multiple casts,
		and uses the MIN macro provided by <sys/param.h>
		
	2. Inline assembler implementation for SVE2 (Armv9).
	
	3. Intrinsic (ACLE) implementation for SVE2 (Armv9).
	
*/

#include <stdio.h>

// -------------------------------------------------------------------- Naive
#if ADJUST_CHANNEL_IMPLEMENTATION == 1

#include <sys/param.h>

void adjust_channels(unsigned char *image, int x_size, int y_size, 
	float red_factor, float green_factor, float blue_factor) {

	printf("Using adjust_channels() implementation #1 - naive\n");

	
	for (int i = 0; i < x_size * y_size * 3; i += 3) {
		image[i]   = MIN((float)image[i]   * red_factor,   255);
		image[i+1] = MIN((float)image[i+1] * blue_factor,  255);
		image[i+2] = MIN((float)image[i+2] * green_factor, 255);
	}
	
}

// -------------------------------------------------------------------- Inline Assembley
#elif ADJUST_CHANNEL_IMPLEMENTATION == 2

void adjust_channels(unsigned char *image, int x_size, int y_size, 
	float red_factor, float green_factor, float blue_factor) {

	printf("Using adjust_channels() implementation #2 - __asm__\n");

}

// -------------------------------------------------------------------- ACLE Intrinsics
#elif ADJUST_CHANNEL_IMPLEMENTATION == 3

void adjust_channels(unsigned char *image, int x_size, int y_size, 
	float red_factor, float green_factor, float blue_factor) {

	printf("Using adjust_channels() implementation #2 - ACLE (intrinsics)\n");
	
}

#else
#error The macro ADJUST_CHANNEL_IMPLEMENTATION must be set to 1, 2, or 3
#endif

