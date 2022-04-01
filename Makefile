CFLAGS = -O3
ADJUST_CHANNEL_IMPLEMENTATION := 1

test:			s
			./s tests/input/bree.jpg 1.0 1.0 1.0 tests/output/bree1.jpg
			echo
			./s tests/input/bree.jpg 0.5 0.5 0.5 tests/output/bree2.jpg

s:			s.c adjust_channels.o
			gcc s.c adjust_channels.o -o s

adjust_channels.o:	adjust_channels.c
			gcc ${CFLAGS} -c adjust_channels.c -D ADJUST_CHANNEL_IMPLEMENTATION=${ADJUST_CHANNEL_IMPLEMENTATION}

clean:			
			rm ${BINARIES} *.o || true




