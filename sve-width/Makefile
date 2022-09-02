BINARIES =		sve-width-intrinsics sve-width-asm

all:			${BINARIES}

%:			%.c
			gcc $@.c -o $@ -g -O3 -march=armv8-a+sve

#sve-width-asm:		sve-width-asm.c
#			gcc sve-width-asm.c -o sve-width-asm -g -O3 -march=armv8-a+sve
#
#sve-width-intrinsics:	sve-width-intrinsics.c
#			gcc sve-width-intrinsics.c -o sve-width-intrinsics -g -O3 -march=armv8-a+sve
#
clean:			
			rm ${BINARIES}
