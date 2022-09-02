#include <stdio.h>
#include <stdint.h>
#include <arm_sve.h>

int main() {
	uint64_t lanes = svcntb();
	printf("SVE vector width is %d bytes or %d bits.\n", lanes, lanes*8);
}

