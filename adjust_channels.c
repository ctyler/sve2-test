/*

	adjust_channel :: adjust red/green/blue colour channels in an image
	
	Three implementations are provided, selected by ADJUST_CHANNEL_IMPLEMENTATION
	
	1. Naive implementation in C. Math is floating point, with multiple casts,
		and uses the MIN macro provided by <sys/param.h>
		
	2. Inline assembler implementation for SVE2 (Armv9).
	
	3. Intrinsic (ACLE) implementation for SVE2 (Armv9).
	
	This function accepts:
	        unsigned char *image            :: pointer to image data
	        int x_size                      :: width of image
	        int y_size                      :: height of image
	        float red_factor                :: adjustment factor for red   (0.0-2.0)
	        fload green_factor              :: adjustment factor for green (0.0-2.0)
	        fload blue_factor               :: adjustment factor for blue  (0.0-2.0)
	        
        The function returns an adjusted image in the original location.
        
        Copyright (C)2022 Seneca College of Applied Arts and Technology
        Written by Chris Tyler
        Distributed under the terms of the GNU GPL v2
	
*/

#include <stdio.h>

// -------------------------------------------------------------------- Naive
#if ADJUST_CHANNEL_IMPLEMENTATION == 1

#include <sys/param.h>

void adjust_channels(unsigned char *image, int x_size, int y_size, 
	float red_factor, float green_factor, float blue_factor) {

	printf("Using adjust_channels() implementation #1 - Naive (possibly autovectorizable)\n");

	
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

	printf("Using adjust_channels() implementation #2 - Inline assembler for SVE2\n");

	// Get arguments into correct format, 0-64 representing 0.0-2.0
	int r = (int)((float)red_factor   * 64.0);
	int g = (int)((float)green_factor * 64.0);
	int b = (int)((float)blue_factor  * 64.0);
	printf("Adjusted factors: r:%d   g:%d   b:%d\n", r, g, b);
	int size = x_size * y_size * 3;
	int i = 0;
	
	for (int e=0; e<27; e+=3) {
	        printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
	}
	
/*

        This is a fixed-point SVE 2 implementation.

	Vector and predicate register usage in the following code:
		z0.b		red channel data
		z1.b		green channel data
		z2.b		blue channel data

		z3.h		red channel factor in each lane
		z4.h		green channel factor in each lane
		z5.h		blue channel data in each lane
		
		z6.h		all-zeros
		
		z7.h		temporary register for math

		z8.b		red channel results
		z9.b		green channel results
		z10.b		blue channel results

		p0		predicate register for all predicated operations (load/store)
		
	Notes on the scatter/gather operations (load/store):
		* The image data is stored in memory as groups of 3 bytes - red, green, blue
		* If N is the number of lanes, the LD3B instruction loads n*3 bytes into 3 vector registers,
			taking every 3rd byte into the lanes of the first register, the following bytes into
			the lanes of the second register, and the next bytes into the lanes of the third
			register. In other words, if image is the array, and z0, z1, and z2 are the three
			sve2 vector registers, then:
			
				For pixel #0,   red data == image[0]   -> lane 0 of z0
						green data == image[1] -> lane 0 of z1
						blue data == image[2]  -> lane 0 of z2
						
			And the pattern continues for the following pixels, so that z0 contains the red
			bytes, z1 contains the green bytes, and z2 contains the blue bytes. We can then
			process the channels separately.

	Notes on the operation of the loop:
		* The register containing 'i' is the current element number
		* The register containing 'array' is a pointer to the start of the data array
		* The WHILELO instruction sets p0 according to how many elements remain to be processed;
			usually this will be 1 for all lanes, but at the end of the array it will have
			some lanes set to 1 (up to the end of the array) and the rest of the lanes will
			be set to 0
		* The B.ANY branches back to continue the loop if the WHILELO set any bits (i.e., it will 
			not branch if we're past the end of the array, which would cause WHILELO to set
			all the bits in p0 to 0). 
			
	Note on saturation:
	        * UQADD is used in place of a multiply to provide saturation

*/

	__asm__ __volatile__("													\n\
									                                         	\n\
		// ============================== Set up loop-invariant registers					\n\
		DUP z3.b, %w[red]	// scaling factor for red channel       					\n\
		DUP z4.b, %w[green]	// scaling factor for green channel						\n\
		DUP z5.b, %w[blue]	// scaling factor for blue channel						\n\
		DUP z6.h, #0		// zero in all lanes for dummy ADDHNT operation					\n\
                                        //      (we're using  ADDHNB/ADDHNT just for narrowing, so we add zero)		\n\
															\n\
		// ============================== Start loop and fetch data						\n\
		WHILELO p0.B, %[i], %[size]				// set up predicate register p0			\n\
    L1:															\n\
		LD3B {z0.b, z1.b, z2.b}, p0/z, [ %[array], %[i] ]	// get 3 vector registers, scatter by bytes	\n\
															\n\
		// ----------------------------- RED channel								\n\
		// Process odd lanes											\n\
		UMULLB z7.h, z0.b, z3.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNB z8.b, z7.h, z6.h					// narrow to 8 bit (take high half)		\n\
															\n\
		// Process even lanes											\n\
		UMULLT z7.h, z0.b, z3.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNT z8.b, z7.h, z6.h					// narrow to 8 bit (take high half)		\n\
															\n\
		// ----------------------------- GREEN channel								\n\
		// Process odd lanes											\n\
		UMULLB z7.h, z1.b, z4.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNB z9.b, z7.h, z6.h					// narrow to 8 bit (take high half)		\n\
															\n\
		// Process even lanes											\n\
		UMULLT z7.h, z1.b, z4.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNT z9.b, z7.h, z6.h					// narrow to 8 bit (take high half)		\n\
															\n\
		// ----------------------------- BLUE channel								\n\
		// Process odd lanes											\n\
		UMULLB z7.h, z2.b, z5.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNB z10.b, z7.h, z6.h				// narrow to 8 bit (take high half)		\n\
															\n\
		// Process even lanes											\n\
		UMULLT z7.h, z2.b, z5.b					// multiply data by factor			\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		UQADD z7.h, z7.h, z7.h                       		// *2						\n\
		ADDHNT z10.b, z7.h, z6.h				// narrow to 8 bit (take high half)		\n\
															\n\
		// ============================== Store data and loop if required					\n\
		ST3B {z8.b, z9.b, z10.b}, p0, [ %[array], %[i] ]	// store 3 vector registers, gather by bytes	\n\
		INCB %[i], ALL, MUL 3											\n\
		WHILELO p0.B, %[i], %[size]										\n\
		B.ANY L1												\n\
		" \
		: [i]"+r"(i)
		: [array]"r"(image), [red]"r"(r), [green]"r"(g), [blue]"r"(b), [size]"r"(size), "r"(i) 
		: "memory");

        printf("\n");
	for (int e=0; e<27; e+=3) {
	        printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
	}
	
}

// -------------------------------------------------------------------- ACLE Intrinsics
#elif ADJUST_CHANNEL_IMPLEMENTATION == 3

void adjust_channels(unsigned char *image, int x_size, int y_size, 
	float red_factor, float green_factor, float blue_factor) {

	printf("Using adjust_channels() implementation #3 - ACLE (intrinsics for SVE2)\n");
	printf("Coming soon!\n");
	
}

#else
#error The macro ADJUST_CHANNEL_IMPLEMENTATION must be set to 1, 2, or 3
#endif

