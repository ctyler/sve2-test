CFLAGS = -g -O3 -march=armv8-a+sve2 
ADJUST_CHANNEL_IMPLEMENTATION := 1
BINARIES = image-adjust1 image-adjust2 image-adjust3

all-test:		${BINARIES}
			echo "Making and testing all versions..."
			echo "===== Implementation 1 - Naive (but potentially auto-vectorized!)"
			qemu-aarch64 ./image-adjust1 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree1a.jpg
			qemu-aarch64 ./image-adjust1 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree1b.jpg
			qemu-aarch64 ./image-adjust1 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree1c.jpg
			echo "===== Implementation 2 - Inline assembler for SVE2"
			qemu-aarch64 ./image-adjust2 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree2a.jpg
			qemu-aarch64 ./image-adjust2 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree2b.jpg
			qemu-aarch64 ./image-adjust2 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree2c.jpg
			echo "===== Implementation 3 - Inline assembler for SVE2 (#2)"
			qemu-aarch64 ./image-adjust3 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree3a.jpg
			qemu-aarch64 ./image-adjust3 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree3b.jpg
			qemu-aarch64 ./image-adjust3 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree3c.jpg

all:			${BINARIES}

image-adjust1:		image-adjust.c adjust_channels1.o
			gcc ${CFLAGS} image-adjust.c adjust_channels1.o -o image-adjust1

image-adjust2:		image-adjust.c adjust_channels2.o
			gcc ${CFLAGS} image-adjust.c adjust_channels2.o -o image-adjust2

image-adjust3:		image-adjust.c adjust_channels3.o
			gcc ${CFLAGS} image-adjust.c adjust_channels3.o -o image-adjust3

adjust_channels1.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=1 -o adjust_channels1.o

adjust_channels2.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=2 -o adjust_channels2.o

adjust_channels3.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=3 -o adjust_channels3.o

clean:			
			rm ${BINARIES} *.o tests/output/bree??.jpg tests/output/montage.jpg || true


