# sve2-test

This is a test/demo of some SIMD concepts, including SVE2 on Armv9.
It builds a simple binary that takes an image file and three factors.
The red, green, and blue channels in the image are each adjusted by
the factors (which are in the range 0.0-2.0) and then the file is 
written to the specified filename.

The processing of the channel data is done by the adjust_channels()
function in adjust_channels.c. There are three implementations 
available:
1. naive implementation, in C - this may be subject to autovectorization!
2. inline assembler implementation 
3. ACLE intrinsics implementation (future)

The implementation may be selected with the ADJUST_CHANNEL_IMPLEMENTATION
macro (set it to 1/2/3 as desired).

The included Makefile will by default build all available implementations, 
and test each with scaling factors 1.0/1.0/1.0, 0.5/0.5/0.5, and 2.0/2.0/2.0;
the input file for the tests is in tests/input/bree.jpg and the output
files from the tests are stored in tests/output/bree[123][abc].jpg

Requires stbi_image and stb_image_writer libraries/headers to build, 
plus qemu-aarch64 to run.

(Current limitations: minimal argument checking is performed, and
the output file is a JPEG regardless of the specified extension).
