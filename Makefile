# C compiler flags
CFLAGS = -g -O3 -march=armv8-a+sve2 
CFLAGS_MAIN = -g -O3 -march=armv8-a+sve2 

# names of the binary files
BINARIES = image-adjust1 image-adjust2 image-adjust3 image-adjust4

# tool used to run the binaries
RUNTOOL = qemu-aarch64

# tool used to time execution
TIMETOOL = time

all-test:		${BINARIES}
			echo "Making and testing all versions..."
			echo "===== Implementation 1 - Naive (but potentially auto-vectorized!)"
			${TIMETOOL} ${RUNTOOL} ./image-adjust1 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree1a.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust1 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree1b.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust1 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree1c.jpg
			echo "===== Implementation 2 - Inline assembler for SVE2"
			${TIMETOOL} ${RUNTOOL} ./image-adjust2 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree2a.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust2 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree2b.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust2 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree2c.jpg
			echo "===== Implementation 3 - Inline assembler for SVE2 (#2)"
			${TIMETOOL} ${RUNTOOL} ./image-adjust3 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree3a.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust3 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree3b.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust3 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree3c.jpg
			echo "===== Implementation 4 - Inline assembler for SVE2 (#2)"
			${TIMETOOL} ${RUNTOOL} ./image-adjust4 tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree4a.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust4 tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree4b.jpg
			${TIMETOOL} ${RUNTOOL} ./image-adjust4 tests/input/bree.jpg 2.0 2.0 2.0 tests/output/bree4c.jpg

all:			${BINARIES}

image-adjust1:		image-adjust.c adjust_channels1.o
			gcc ${CFLAGS_MAIN} image-adjust.c adjust_channels1.o -o image-adjust1

image-adjust2:		image-adjust.c adjust_channels2.o
			gcc ${CFLAGS_MAIN} image-adjust.c adjust_channels2.o -o image-adjust2

image-adjust3:		image-adjust.c adjust_channels3.o
			gcc ${CFLAGS_MAIN} image-adjust.c adjust_channels3.o -o image-adjust3

image-adjust4:		image-adjust.c adjust_channels4.o
			gcc ${CFLAGS_MAIN} image-adjust.c adjust_channels4.o -o image-adjust4

adjust_channels1.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=1 -o adjust_channels1.o

adjust_channels2.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=2 -o adjust_channels2.o

adjust_channels3.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=3 -o adjust_channels3.o

adjust_channels4.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=4 -o adjust_channels4.o

clean:			
			rm ${BINARIES} *.o tests/output/bree??.jpg tests/output/montage.jpg || true


