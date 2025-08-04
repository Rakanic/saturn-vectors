#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "rvv_mx.h"

extern size_t N;

extern uint16_t bf16a[] __attribute__((aligned(64)));
extern uint16_t bf16b[] __attribute__((aligned(64)));
extern uint16_t bf16r[] __attribute__((aligned(64)));

extern uint16_t fp16a[] __attribute__((aligned(64)));
extern uint16_t fp16b[] __attribute__((aligned(64)));
extern uint16_t fp16r[] __attribute__((aligned(64)));

int main() {

	size_t avl;
	size_t vl;
	uint16_t res;

	// FP16
	printf("Testing FP16\n");

	avl = N;
	vl = 0;

	uint16_t *fp16a_ = fp16a;
	uint16_t *fp16b_ = fp16b;
	uint16_t *fp16r_ = fp16r;

	while (avl > 0) {\
		asm volatile("vsetvli %0, %1, e16, m1" : "=r"(vl) : "r"(avl));
		avl -= vl;
		asm volatile("vle16.v v0, (%0)" : : "r"(fp16a_));
		asm volatile("vle16.v v8, (%0)" : : "r"(fp16b_));
		asm volatile("vfmul.vv v0, v0, v8");
		asm volatile("vle16.v v8, (%0)" : : "r"(fp16r_));
		asm volatile("vmsne.vv v16, v0, v8");
		asm volatile("vmv.x.s %0, v16" : "=r"(res));
		fp16a_ += vl;
		fp16b_ += vl;
		fp16r_ += vl;
		if (res) {
			printf("Test failed\n");
			exit(1);
		}
	}

	// BF16
	printf("Testing BF16\n");

	avl = N;
	vl = 0;

	uint16_t *bf16a_ = bf16a;
	uint16_t *bf16b_ = bf16b;
	uint16_t *bf16r_ = bf16r;

	while (avl > 0) {
		VSETVLI_ALTFMT(vl, avl, SEW_E16, LMUL_M1);
		avl -= vl;
		asm volatile("vle16.v v0, (%0)" : : "r"(bf16a_));
		asm volatile("vle16.v v8, (%0)" : : "r"(bf16b_));
		asm volatile("vfmul.vv v0, v0, v8");
		asm volatile("vle16.v v8, (%0)" : : "r"(bf16r_));
		asm volatile("vmsne.vv v16, v0, v8");
		asm volatile("vmv.x.s %0, v16" : "=r"(res));
		bf16a_ += vl;
		bf16b_ += vl;
		bf16r_ += vl;
		if (res) {
			printf("Test failed\n");
			exit(1);
		}
	}
	
	printf("All tests passed\n");

	return 0;
}

// if (1) {
// 				for (size_t i = 0; i < vl; i ++) {
// 					asm volatile("vmv.x.s %0, v0" : "=r"(res));
// 					printf("%04x\n", res);
// 					asm volatile("vmv.x.s %0, v8" : "=r"(res));
// 					printf("%04x\n", res);
// 					asm volatile("vslidedown.vi v0, v0, 1");
// 					asm volatile("vslidedown.vi v8, v8, 1");
// 					printf("Next\n");
// 				}
// 				// exit(1);
// 			}