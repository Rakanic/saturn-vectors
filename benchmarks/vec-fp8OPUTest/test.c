
#include <stdio.h>
#include <riscv-pk/encoding.h>
#include <riscv_vector.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// HACK reuse the scalar registers to avoid assembler hacking for now
#define m0 "x0"
#define m1 "x1"
#define m2 "x2"
#define m3 "x3"
#define m4 "x4"
#define m5 "x5"
#define m6 "x6"
#define m7 "x7"

#define v0 "x0"
#define v1 "x1"
#define v2 "x2"
#define v3 "x3"
#define v4 "x4"
#define v5 "x5"
#define v6 "x6"
#define v7 "x7"
#define v8 "x8"
#define v9 "x9"
#define v10 "x10"
#define v11 "x11"
#define v12 "x12"
#define v13 "x13"
#define v14 "x14"
#define v15 "x15"
#define v16 "x16"
#define v17 "x17"
#define v18 "x18"
#define v19 "x19"
#define v20 "x20"
#define v21 "x21"
#define v22 "x22"
#define v23 "x23"
#define v24 "x24"
#define v25 "x25"
#define v26 "x26"
#define v27 "x27"
#define v28 "x28"
#define v29 "x29"
#define v30 "x30"
#define v31 "x31"

#define VLEN 16
#define MIN 1
#define MAX 39
#define STEP 12

// opmvx. f6=b101010, f7=b1010101
#define OPMVIN(md, vs2, rs1) \
  asm volatile(".insn r 0x57, 0x6, 0x55, " md ", %0, " vs2 : : "r"(rs1));

// opmvx. f6=b101110, f7=b1011101
#define OPMVOUT(vd, ms2, rs1) \
  asm volatile(".insn r 0x57, 0x6, 0x5d, " vd ", %0, " ms2 : : "r"(rs1));

// opmvx. f6=b101100, f7=b1011001
#define OPMVINBCAST(md, vs2) \
  asm volatile(".insn r 0x57, 0x6, 0x59, " md ", x0, " vs2);

// opmvv. f6=b101000, f7=b1010001 
#define OPMACC(md, vs2, vs1) \
  asm volatile(".insn r 0x57, 0x2, 0x51, " md ", " vs1 ", " vs2);


void fp8_opu_simple(int8_t* vecA, int8_t* vecB, int32_t* result, size_t vec_size) {
	asm volatile("vsetvli %[vl], %[vl], e32, m1, ta, ma" : : [vl]"r"(vec_size));
	asm volatile("vmv.v.i v0, 0x0");
	OPMVINBCAST(m1, v0);
	
	asm volatile("vsetvli %[vl], %[vl], e8, m1, ta, ma" : : [vl]"r"(vec_size));
        asm volatile("vle8.v v1, (%0)" : : "r"(vecA));
        asm volatile("vle8.v v0, (%0)" : : "r"(vecB));
        OPMACC(m1, v1, v0);

	asm volatile("vsetvli %[vl], %[vl], e32, m1, ta, ma" : : [vl]"r"(vec_size));
      	for (size_t r = 0; r < vec_size; r++) {
		OPMVOUT(v0, m1, r);
		asm volatile("vse32.v v0, (%0)" : : "r"(&result[r*vec_size]));
	}
}	

int main(void) {

  int8_t A[VLEN] = {-72, -74, -76, -78, -81, -85, -91, -103, 25, 37, 43, 47, 50, 52, 54, 56};
  int8_t B[VLEN] = {56, 54, 52, 50, 47, 43, 37, 25, -103, -91, -85, -81, -78, -76, -74, -72};
  int32_t C[VLEN*VLEN];

  fp8_opu_simple(A, B, C, VLEN);

  for (int i = 0; i < VLEN; i ++) {
  	for (int j = 0; j < VLEN; j ++) {
		printf("%d ", C[i*VLEN + j]);
	}
	printf("\n");
  }

  return 0;
  }
