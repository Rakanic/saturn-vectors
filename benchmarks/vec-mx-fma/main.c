#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "rvv_mx.h"

extern size_t N;
size_t avl;
size_t vl;

#define TEST_DATA(type, name) \
	extern type name ## a[] __attribute__((aligned(64))); \
	extern type name ## b[] __attribute__((aligned(64))); \
	type *name ## a_; \
	type *name ## b_;

#define TEST_OP(type, name, op) \
	extern type name ## _ ## op[] __attribute__((aligned(64))); \
	type *name ## _ ## op ## _; \
	type name ## _ ## res ## _ ## op ## _;

/*
	T - input type
	W - output type
	name - float name
	op - operation name
	sew - input sew
	wsew - output sew
	alt - altfmt
	walt - output altfmt
	vle - RVV instruction for loading the input type
	wvle - RVV instruction for loading the output type
	vfop - RVV instruction for performing the operation
*/
#define TEST(name, op, sew, wsew, alt, walt, vle, wvle, vfop) \
	printf("Testing " #name "_" #op "\n"); \
	avl = N; \
	vl = 0; \
	name ## a_ = name ## a; /* input A pointer */ \
	name ## b_ = name ## b; /* input B pointer */ \
	name ## _ ## op ## _ = name ## _ ## op; /* result pointer */ \
	while (avl > 0) { \
		VSETVLI_ALTFMT(vl, avl, sew, LMUL_M1, alt); /* vsetvli */ \
		asm volatile(vle " v0, (%0)" : : "r"(name ## a_)); /* load A */ \
		asm volatile(vle " v8, (%0)" : : "r"(name ## b_)); /* load B */\
		asm volatile(vfop " v24, v0, v8"); /* operation */ \
		if (sew != wsew) { \
			VSETVLI_ALTFMT_X0(avl, wsew, LMUL_M2, walt); /* vsetvli */ \
		} \
		asm volatile(wvle " v8, (%0)" : : "r"(name ## _ ## op ## _)); /* load result */ \
		asm volatile("vmsne.vv v16, v24, v8"); /* compare */ \
		asm volatile("vmv.x.s %0, v16" : "=r"(name ## _ ## res ## _ ## op ## _)); /* extract comparison */ \
		name ## a_ += vl; /* increment input A pointer */ \
		name ## b_ += vl; /* increment input B pointer */ \
		name ## _ ## op ## _ += vl; /* increment result pointer */ \
		if (name ## _ ## res ## _ ## op ## _) { /* fail if not equal */ \
			printf("Test failed\n"); \
			printf("Index: %d\n", avl); \
			for (size_t i = 0; i < vl; i ++) { \
				asm volatile("vmv.x.s %0, v24" : "=r"(name ## _ ## res ## _ ## op ## _)); \
				printf("%#010x\n", name ## _ ## res ## _ ## op ## _); \
				asm volatile("vmv.x.s %0, v8" : "=r"(name ## _ ## res ## _ ## op ## _)); \
				printf("%#010x\nNext\n", name ## _ ## res ## _ ## op ## _); \
				asm volatile("vslidedown.vi v24, v24, 1"); \
				asm volatile("vslidedown.vi v8, v8, 1"); \
			} \
			exit(1); \
		} \
		avl -= vl; \
	}

TEST_DATA(uint32_t, fp32)
TEST_OP(uint32_t, fp32, mul)
TEST_OP(uint32_t, fp32, add)

TEST_DATA(uint16_t, fp16)
TEST_OP(uint16_t, fp16, mul)
TEST_OP(uint16_t, fp16, add)

TEST_DATA(uint16_t, bf16)
TEST_OP(uint16_t, bf16, mul)
TEST_OP(uint16_t, bf16, add)

TEST_DATA(uint8_t, ofp8e5m2)
TEST_OP(uint8_t, ofp8e5m2, mul)
TEST_OP(uint8_t, ofp8e5m2, add)

TEST_DATA(uint8_t, ofp8e4m3)
TEST_OP(uint8_t, ofp8e4m3, mul)
TEST_OP(uint8_t, ofp8e4m3, add)

TEST_DATA(uint16_t, fp16W)
TEST_OP(uint32_t, fp16W, mul)
TEST_OP(uint32_t, fp16W, add)

TEST_DATA(uint16_t, bf16W)
TEST_OP(uint32_t, bf16W, mul)
TEST_OP(uint32_t, bf16W, add)

TEST_DATA(uint8_t, ofp8e5m2W)
TEST_OP(uint16_t, ofp8e5m2W, mul)
TEST_OP(uint16_t, ofp8e5m2W, add)

TEST_DATA(uint8_t, ofp8e4m3W)
TEST_OP(uint16_t, ofp8e4m3W, mul)
TEST_OP(uint16_t, ofp8e4m3W, add)

int main() {
	
	TEST(fp32, mul, SEW_E32, SEW_E32, 0, 0, "vle32.v", "vle32.v", "vfmul.vv")
	TEST(fp16, mul, SEW_E16, SEW_E16, 0, 0, "vle16.v", "vle16.v", "vfmul.vv")
	TEST(bf16, mul, SEW_E16, SEW_E16, 1, 1, "vle16.v", "vle16.v", "vfmul.vv")
	TEST(ofp8e5m2, mul, SEW_E8, SEW_E8, 1, 1, "vle16.v", "vle16.v", "vfmul.vv")
	TEST(ofp8e4m3, mul, SEW_E8, SEW_E8, 0, 0, "vle8.v", "vle8.v", "vfmul.vv")
	TEST(bf16W, mul, SEW_E16, SEW_E32, 1, 0, "vle16.v", "vle32.v", "vfwmul.vv")
	TEST(fp16W, mul, SEW_E16, SEW_E32, 0, 0, "vle16.v", "vle32.v", "vfwmul.vv")
	TEST(ofp8e5m2W, mul, SEW_E8, SEW_E16, 1, 1, "vle8.v", "vle16.v", "vfwmul.vv")
	TEST(ofp8e4m3W, mul, SEW_E8, SEW_E16, 0, 1, "vle8.v", "vle16.v", "vfwmul.vv")
	
	TEST(fp32, add, SEW_E32, SEW_E32, 0, 0, "vle32.v", "vle32.v", "vfadd.vv")
	TEST(fp16, add, SEW_E16, SEW_E16, 0, 0, "vle16.v", "vle16.v", "vfadd.vv")
	TEST(bf16, add, SEW_E16, SEW_E16, 1, 1, "vle16.v", "vle16.v", "vfadd.vv")
	TEST(ofp8e5m2, add, SEW_E8, SEW_E8, 1, 1, "vle16.v", "vle16.v", "vfadd.vv")
	TEST(ofp8e4m3, add, SEW_E8, SEW_E8, 0, 0, "vle8.v", "vle8.v", "vfadd.vv")
	TEST(bf16W, add, SEW_E16, SEW_E32, 1, 0, "vle16.v", "vle32.v", "vfwadd.vv")
	TEST(fp16W, add, SEW_E16, SEW_E32, 0, 0, "vle16.v", "vle32.v", "vfwadd.vv")
	TEST(ofp8e5m2W, add, SEW_E8, SEW_E16, 1, 1, "vle8.v", "vle16.v", "vfwadd.vv")
	TEST(ofp8e4m3W, add, SEW_E8, SEW_E16, 0, 1, "vle8.v", "vle16.v", "vfwadd.vv")

	printf("All tests passed\n");

	return 0;
}
