#include <stdio.h>
#include <stdint.h>

int main() {
	uint64_t lanes;
        __asm__ __volatile__ ("CNTB %[lanes]" : [lanes]"=r"(lanes) : : );
	printf("SVE vector width is %d bytes or %d bits.\n", lanes, lanes*8);
}

