/*

        adjust_channels :: adjust red/green/blue colour channels in an image
        
        Multiple implementations are provided, selected by ADJUST_CHANNEL_IMPLEMENTATION:
        
        1. Naive implementation in C. Math is floating point, with multiple casts,
                and uses the MIN macro provided by <sys/param.h>. Can be vectorized
                by GCC 11.2.1
                
        2. Inline assembler implementation for SVE2 (Armv9) - load 3-element structure
        
        3. Inline assembler implementation for SVE2 (Armv9) - interleaved factor table
        
        4. Intrinsic (ACLE) implementation for SVE2 (Armv9).
        
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
#include <stdlib.h>
#include <stdint.h>

// -------------------------------------------------------------------- Naive implementation in C
#if ADJUST_CHANNEL_IMPLEMENTATION == 1

#include <sys/param.h>

void adjust_channels(unsigned char *image, int x_size, int y_size, 
        float red_factor, float green_factor, float blue_factor) {

        printf("Using adjust_channels() implementation #1 - Naive (autovectorizable)\n");
        
/*

        The image is stored in memory as pixels of 3 bytes, representing red/green/blue values.
        Each of these values is multiplied by the corresponding adjustment factor, with 
        saturation, and then stored back to the original memory location.
        
        This simple implementation causes int to float to int conversions.
        
*/

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

        printf("Using adjust_channels() implementation #2 - Inline assembler for SVE2, structure load\n");

/*

        This is a fixed-point SVE 2 implementation.
        
        The channel adjustment factors are 8-bit values with the radix point
        between bit 5 and 6 (i.e., bb.bbbbbb) which gives 128 values between 
        0.0 - 2.0

        Vector and predicate register usage in the following code:
                z0.b            red channel data
                z1.b            green channel data
                z2.b            blue channel data

                z3.h            red channel factor in each lane
                z4.h            green channel factor in each lane
                z5.h            blue channel factor in each lane
                
                z6.h            all-zeros
                
                z7.h            temporary register for math

                z8.b            red channel results
                z9.b            green channel results
                z10.b           blue channel results

                p0              predicate register for all predicated operations (load/store)
                
        Notes on the scatter/gather operations (load/store):
                * The image data is stored in memory as pixels of 3 bytes - red, green, blue
                * If N is the number of lanes, the LD3B instruction loads n*3 bytes into 3 vector registers,
                        taking every 3rd byte into the lanes of the first register, the following bytes into
                        the lanes of the second register, and the next bytes into the lanes of the third
                        register. In other words, if image is the array, and z0, z1, and z2 are the three
                        sve2 vector registers, then:
                        
                                For pixel #0,   red data == image[0]   -> lane 0 of z0
                                                green data == image[1] -> lane 0 of z1
                                                blue data == image[2]  -> lane 0 of z2
                                                
                        Where (in this case) the arrow operator -> means "is placed into".
                                                
                        This pattern continues for the following pixels, so that z0 contains the red
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
                * Two UQADD instructions are used in place of a multiply by 4 to provide saturation
                * A compound instruction like SQDMULH would be useful, but there is no 8-bit unsigned version available

*/
        // Get arguments into correct format, 0-64 representing 0.0-2.0
        int r = (int)((float)red_factor   * 64.0);
        int g = (int)((float)green_factor * 64.0);
        int b = (int)((float)blue_factor  * 64.0);
        printf("Adjusted factors: r:%d   g:%d   b:%d\n", r, g, b);
        
        // Variables with precalculated total array size and iterator
        int size = x_size * y_size * 3;
        int i = 0;
        
        // Diagnostic output
//      for (int e = 0; e < 27; e += 3) {
//              printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
//      }
        
        __asm__ __volatile__("                                                                                          \n\
                                                                                                                        \n\
                // ============================== Set up loop-invariant registers                                       \n\
                DUP z3.b, %w[red]       // scaling factor for red channel                                               \n\
                DUP z4.b, %w[green]     // scaling factor for green channel                                             \n\
                DUP z5.b, %w[blue]      // scaling factor for blue channel                                              \n\
                DUP z6.h, #0            // zero in all lanes for dummy ADDHNT operation                                 \n\
                                        //      (we're using  ADDHNB/ADDHNT just for narrowing, so we add zero)         \n\
                                                                                                                        \n\
                // ============================== Start loop and fetch data                                             \n\
                WHILELO p0.B, %[i], %[size]                             // set up predicate register p0                 \n\
    L1:                                                                                                                 \n\
                LD3B {z0.b, z1.b, z2.b}, p0/z, [ %[array], %[i] ]       // load 3 vector registers, scatter by bytes     \n\
                                                                                                                        \n\
                // ----------------------------- RED channel                                                            \n\
                // Process odd lanes                                                                                    \n\
                UMULLB z7.h, z0.b, z3.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNB z8.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // Process even lanes                                                                                   \n\
                UMULLT z7.h, z0.b, z3.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNT z8.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // ----------------------------- GREEN channel                                                          \n\
                // Process odd lanes                                                                                    \n\
                UMULLB z7.h, z1.b, z4.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNB z9.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // Process even lanes                                                                                   \n\
                UMULLT z7.h, z1.b, z4.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNT z9.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // ----------------------------- BLUE channel                                                           \n\
                // Process odd lanes                                                                                    \n\
                UMULLB z7.h, z2.b, z5.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNB z10.b, z7.h, z6.h                                // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // Process even lanes                                                                                   \n\
                UMULLT z7.h, z2.b, z5.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNT z10.b, z7.h, z6.h                                // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // ============================== Store data and loop if required                                       \n\
                ST3B {z8.b, z9.b, z10.b}, p0, [ %[array], %[i] ]        // store 3 vector registers, gather by bytes    \n\
                INCB %[i], ALL, MUL 3                                   // increment by number of lanes * 3             \n\
                WHILELO p0.B, %[i], %[size]                             // generate new predicate value                 \n\
                B.ANY L1                                                // branch if any predicate bits are set         \n\
                " 
                : [i]"+r"(i)
                : [array]"r"(image), [red]"r"(r), [green]"r"(g), [blue]"r"(b), [size]"r"(size), "r"(i) 
                : "memory");

        // Diagnostic output
//      printf("\n");
//      for (int e=0; e<27; e+=3) {
//              printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
//      }
        
}


// -------------------------------------------------------------------- Inline Assembley (#2)
#elif ADJUST_CHANNEL_IMPLEMENTATION == 3

void adjust_channels(unsigned char *image, int x_size, int y_size, 
        float red_factor, float green_factor, float blue_factor) {

        printf("Using adjust_channels() implementation #3 - Inline assembler for SVE2, interleaved\n");

/*

        This is a fixed-point SVE 2 implementation like the previous one,
        with the same basic principles (8-bit fixed-point channel adjustment
        factors, etc).

        The difference is in how data is loaded and manipulated. In the 
        implementation above, LD3B/ST3B is used to separate channel data
        into three different vector registers (one for each channel). In
        this implementation, the interleaved values are read into a single
        vector register, and then multiplied by an interleaved vector of the
        channel factors. However, the vector size may or may not be a multiple
        of three, so the stride through memory is adjusted to the highest 
        multiple of three that is less than or equal to the number of lanes
        available in the vector registers on the current system.

        In other words, the data is in memory in the regular interleaved
        fashion:

                byte 0          pixel 0         red data
                byte 1          pixel 0         green data
                byte 2          pixel 0         blue data
                byte 3          pixel 1         red data
                byte 4          pixel 1         green data
                byte 5          pixel 1         blue data
                ...

        Which is loaded into a vector register and multiplied by the
        channel factors which are repeated in the same order:

                byte 0                           channel factor for red
                byte 1                           channel factor for green
                byte 2                           channel factor for blue
                byte 3                           channel factor for red
                byte 4                           channel factor for green
                byte 5                           channel factor for blue
                ...

        After processing (including handling saturation), the data is placed
        back into the original memory locations.
        
        The number of vector lanes is not known at compile time in SVE2. If the
        number of lanes is not a multiple of three, then channel factors will be 
        wrong for the second iteration through the loop. For example, in a 128-bit
        SVE2 implementation, the first vector register will process 16 lanes of data,
        as r/g/b,r/g/b,r/g/b,r/g/b,r/g/b,r/g/b,r/g/b,r -- note that this is 5 full pixels
        (15 bytes) plus the red channel of a 6th pixel (16th byte). When the next 128 
        bits (16 bytes) of data is fetched, it will start with a green data byte, not 
        a red data byte.
        
        To rectify this, we have to cause the elements of the incomplete pixel to 
        remain unchanged, and then overlap the next load operation to start at the 
        beginning of the incomplete pixel -- that is, the second register load will
        start 15 bytes after the first register load.
        
        (Note that this will cause misaligned reads and writes).
        
        (Note that if the number of lanes in a vector register _is_ a multiple of 3,
        such as in a 384-bit implementation of SVE2, this extra fiddling is not required
        and will disappear).
        
        To ensure that the incomplete pixel is unchanged, we can either manipulate the 
        predicate register, or we can simply set the scaling factor for the incomplete
        pixel elements to 1.0 (which is the approach used here).

        Register usage is the same as the previous implementation above, except:
        
                z0              interleaved pixel data
                z1, z2          not used
                z3              interleaved channel factors
                z4, z5          not used
 
        Additional parameters:
        
                elements3       largest number of vector lanes available
                                        that is a multiple of 3
                factor_table    pointer to interleaved table of channel factors

*/
        // Get arguments into correct format, 0-64 representing 0.0-2.0
        int r = (int)((float)red_factor   * 64.0);
        int g = (int)((float)green_factor * 64.0);
        int b = (int)((float)blue_factor  * 64.0);
        printf("Adjusted factors: r:%d   g:%d   b:%d\n", r, g, b);
        int size = x_size * y_size * 3;
        int i = 0;
        
        // Diagnostic output
//      for (int e = 0; e < 27; e += 3) {
//              printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
//      }
        
        // Find out how many lanes we have to process
        uint64_t elements;
        __asm__ __volatile__ ("CNTB %[elements]" : [elements]"=r"(elements) : : );
        
        // Figure out the largest number less than or equal to lane count that is a multiple of 3
        int elements3 = (elements / 3) * 3;
        
        // Set up the factor table for multiplication
        uint8_t *factor_table;
        factor_table = malloc(elements);

        // ... set up elements [0 .. elements3] with interleaved channel factors
        for (int e = 0; e < elements3; e += 3) {
                factor_table[e]   = r;
                factor_table[e+1] = g;
                factor_table[e+2] = b;
        }
        // ... set up the remaining elements with a dummy channel factor of 1.0
        for (int e = elements3 ; e < elements; e++) {
                factor_table[e] = 64 ; // fixed-point value corresponding to 1.0
        }
        
        __asm__ __volatile__("                                                                                          \n\
                                                                                                                        \n\
                // Set up predicate register with initial value                                                         \n\
                WHILELO p0.b, %[i], %[size]                                                                             \n\
                                                                                                                        \n\
                // ============================== Set up loop-invariant registers                                       \n\
                LD1B z3.b, p0/z, [ %[factor_table]  ]    // load the factor table                                       \n\
                DUP z6.h, #0                     // zero in all lanes for dummy ADDHNB/ADDHNT operation                 \n\
                                                 //      (we're using  ADDHNB/ADDHNT just for narrowing, so we add zero)\n\
                                                                                                                        \n\
                // ============================== Start loop and fetch data                                             \n\
    L1:                                                                                                                 \n\
                LD1B z0.b, p0/z, [ %[array], %[i] ]                     // get data into one vector register            \n\
                                                                                                                        \n\
                // Process odd lanes                                                                                    \n\
                UMULLB z7.h, z0.b, z3.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNB z8.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // Process even lanes                                                                                   \n\
                UMULLT z7.h, z0.b, z3.b                                 // multiply data by factor                      \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                UQADD z7.h, z7.h, z7.h                                  // *2 with saturation                           \n\
                ADDHNT z8.b, z7.h, z6.h                                 // narrow to 8 bit (take high half)             \n\
                                                                                                                        \n\
                // ============================== Store data and loop if required                                       \n\
                ST1B z8.b, p0, [ %[array], %[i] ]                       // store data back to memory                    \n\
                ADD %[i], %[i], %[elements3]                            // advance to 1 bytes past last full pixel      \n\
                WHILELO p0.B, %[i], %[size]                             // set up new predicate value                   \n\
                B.ANY L1                                                // branch if any predicate bits are set         \n\
                " 
                : [i]"+r"(i)
                : [array]"r"(image), "r"(i), [elements3]"r"(elements3), [factor_table]"r"(factor_table), [size]"r"(size)
                : "memory");

        // Diagnostic output
//      printf("\n");
//      for (int e = 0; e < 27; e += 3) {
//              printf("e:%3d   r:%3d   g:%3d   b:%3d\n", e, image[e], image[e+1], image[e+2]);
//      }
        
}

// -------------------------------------------------------------------- ACLE Intrinsics
#elif ADJUST_CHANNEL_IMPLEMENTATION == 4

#include <arm_sve.h>

void adjust_channels(unsigned char *image, int x_size, int y_size, 
        float red_factor, float green_factor, float blue_factor) {

        printf("Using adjust_channels() implementation #4 - ACLE (intrinsics for SVE2)\n");

/*

        This is an ACLE (Arm C Language Extensions) (C intrinsics) version of
        Implementation 2, above.
        
*/

        svuint8_t       red_data, green_data, blue_data;        // data vectors for colours
        svuint8x3_t     pixels = svcreate3(red_data, green_data, blue_data);     // tuple of 3 data vectors

        uint64_t        lanes = svcntb();                       // count of data lanes
        int             i=0;                                    // iterator
        int             size = x_size * y_size * 3;             // image array size in bytes
        svbool_t        p;                                      // predicate for load/store

        svuint8_t       r = svdup_u8((uint8_t)(red_factor * 64.0));    // vector register with duplicated red_factor
        svuint8_t       g = svdup_u8((uint8_t)(green_factor * 64.0));  // vector register with duplicated green_factor
        svuint8_t       b = svdup_u8((uint8_t)(blue_factor * 64));     // vector register with duplicated blue_factor

        svuint16_t      zero = svdup_u16(0);                    // vector register with duplicated zeros
        svuint8_t       zerob = svdup_u8(0);                    // vector register with duplicated zeros

        svuint16_t      tmp;                                    // vectors for temporary math values
        svuint8_t       tmp_out;        
        
	for (i=0; i<size; i += lanes * 3) {
        
                // ========= Load data
                p = svwhilelt_b8(i, size);                      // get predicate value
                pixels = svld3(p, image + i);                   // load tuple with image data
                
                // ========= Proccess channels
                // --------- Red channel
                tmp = svmullb_u16(svget3(pixels, 0), r);        // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp_out = svaddhnb(tmp, zero);                  // narrow to 8-bit          

                tmp = svmullt(svget3(pixels, 0), r);            // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                pixels = svset3(pixels, 0, svaddhnt(tmp_out, tmp, zero));// narrow to 8-bit
                
                // --------- Green channel
                tmp = svmullb(svget3(pixels, 1), g);            // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp_out = svaddhnb(tmp, zero);                  // narrow to 8-bit          

                tmp = svmullt(svget3(pixels, 1), g);            // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                pixels =  svset3(pixels, 1, svaddhnt(tmp_out, tmp, zero));// narrow to 8-bit
                
                // --------- Blue channel
                tmp = svmullb(svget3(pixels, 2), b);            // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp_out = svaddhnb(tmp, zero);                  // narrow to 8-bit          

                tmp = svmullt(svget3(pixels, 2), b);            // multiply data by factor, widen to 16-bit
                tmp = svqadd(tmp, tmp);                         // double with saturation
                tmp = svqadd(tmp, tmp);                         // double with saturation
                pixels = svset3(pixels, 2, svaddhnt(tmp_out, tmp, zero));// narrow to 8-bit

                // ========= Save data
                svst3(p, image + i, pixels);                    // store tuple to image
                                
        }

}

#else
#error The macro ADJUST_CHANNEL_IMPLEMENTATION must be set to a number (1-4)
#endif

