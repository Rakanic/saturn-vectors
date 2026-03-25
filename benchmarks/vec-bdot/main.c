#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rvv_mx.h"
#include "bme.h"

extern const size_t N;
extern uint8_t a[] __attribute__((aligned(64)));
extern uint8_t b[] __attribute__((aligned(64)));
extern uint32_t r[] __attribute__((aligned(64)));
extern uint32_t rt[] __attribute__((aligned(64)));

#define BLOCK_SIZE 16

void matmul_opu() {
    int cycles_start;
    int cycles_end;
    uint32_t res[N * N];
    memset(res, 0, N * N * sizeof(uint32_t));
    int vl;
    asm volatile("csrr %0, cycle" : "=r"(cycles_start));

    VSETVLI_ALTFMT(vl, N, SEW_E8, LMUL_M1, 0);
    for (int i = 0; i < N; i += vl) {
        for (int j = 0; j < N; j += vl) {
            VSETVLI_ALTFMT_X0(vl, SEW_E32, LMUL_M4, 0);
            asm volatile("vmv.v.i v0, 0");
            OPMVINBCAST("x1", "x0");
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 1);
            for (int k = 0; k < N; k ++) {
                asm volatile("vle8.v v0, (%0)" :: "r"(a + i + k * N));
                asm volatile("vle8.v v1, (%0)" :: "r"(b + j + k * N));
                OPFMACC("x1", "x1", "x0");
            }
            VSETVLI_ALTFMT_X0(vl, SEW_E32, LMUL_M4, 0);
            for (int l = 0; l < vl; l ++) {
                OPMVOUT("x0", "x1", l);
                asm volatile("vle32.v v4, (%0)" :: "r"(res + (i + l) * N + j));
                asm volatile("vadd.vv v0, v0, v4");
                asm volatile("vle32.v v0, (%0)" :: "r"(res + (i + l) * N + j));
            }
        }
    }

    asm volatile("fence");
    asm volatile("csrr %0, cycle" : "=r"(cycles_end));
    printf("Cycles (OPU): %d\n", cycles_end - cycles_start);
    // for (int i = 0; i < N * N; i ++) {
    //     if (res[i] != rt[i]) {
    //         printf("Bad value at index %d: got %d, expected %d\n", i, res[i], r[i]);
    //         exit(1);
    //     }
    // }
}


void matmul_bdot_multi_acc_unroll_m_32_rescheduled_old(int check) {
    int cycles_start;
    int cycles_end;
    uint32_t res[N * N];
    memset(res, 0, N * N * sizeof(uint32_t));
    int vl;
    asm volatile("csrr %0, cycle" : "=r"(cycles_start));

    VSETVLI_ALTFMT(vl, N, SEW_E8, LMUL_M1, 0);

    // Load first VS2
    uint8_t *b_base = b;
    uint8_t *b_base2;
    asm volatile("vle8.v v0, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v1, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v2, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v3, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v4, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v5, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v6, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v7, (%0)" :: "r"(b_base));
    b_base += N;

    asm volatile("vle8.v v24, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v25, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v26, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v27, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v28, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v29, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v30, (%0)" :: "r"(b_base));
    b_base += N;
    asm volatile("vle8.v v31, (%0)" :: "r"(b_base));

    for (int j = 0; j < N; j += 16) {
        int j_N = j * N;
        int j_N2 = j_N + 8 * N;
        for (int i = 0; i < N; i += 16) {
            int i_N = i * N;
            uint32_t *res_base = res + i_N + j;
            uint32_t *res_base2 = res_base + 8;
            VDOTSETZEROBC_VV();
            int k;

            for (k = 0; k < N - vl; k += vl) {
                uint8_t *a_base = a + k + i_N;
                b_base = b + k + vl + j_N; // The b_base for the next iteration
                b_base2 = b + k + vl + j_N2;

                // Load VS1 and accumulate

                asm volatile("vle8.v v8, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v9, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v10, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v11, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v12, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v13, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v14, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v15, (%0)" :: "r"(a_base));
                a_base += N;
                VQBDOTUA_VV(X0, V3, V8);
                VQBDOTUA_VV(X16, V27, V8);

                asm volatile("vle8.v v8, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v9, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v10, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v11, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v12, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v13, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v14, (%0)" :: "r"(a_base));
                a_base += N;
                asm volatile("vle8.v v15, (%0)" :: "r"(a_base));
                VQBDOTUA_VV(X8, V3, V8);
                VQBDOTUA_VV(X24, V27, V8);

                // Load next VS2
                asm volatile("vle8.v v0, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v1, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v2, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v3, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v4, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v5, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v6, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v7, (%0)" :: "r"(b_base));

                asm volatile("vle8.v v24, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v25, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v26, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v27, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v28, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v29, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v30, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v31, (%0)" :: "r"(b_base2));
            }
            
            uint8_t *a_base = a + k + i_N;
            b_base = b + j_N; // The b_base for the next iteration
            b_base2 = b + j_N2;
            if (i == N - 16) {
                b_base += N * 16;
                b_base2 += N * 16;
            }

            // Load VS1 and accumulate
            
            asm volatile("vle8.v v8, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v9, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v10, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v11, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v12, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v13, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v14, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v15, (%0)" :: "r"(a_base));
            a_base += N;

            VQBDOTUA_VV(X0, V3, V8);
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V16, X0, X3);
            asm volatile("vse32.v v16, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v17, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v18, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v19, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v20, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v21, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v22, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v23, (%0)" :: "r"(res_base));
            res_base += N;
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);
            VQBDOTUA_VV(X16, V27, V8);
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V8, X16, X3);
            asm volatile("vse32.v v8, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v9, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v10, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v11, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v12, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v13, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v14, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v15, (%0)" :: "r"(res_base2));
            res_base2 += N;
            
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);
            asm volatile("vle8.v v8, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v9, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v10, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v11, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v12, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v13, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v14, (%0)" :: "r"(a_base));
            a_base += N;
            asm volatile("vle8.v v15, (%0)" :: "r"(a_base));
            a_base += N;
            
            VQBDOTUA_VV(X8, V3, V8);
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V16, X8, X3);
            asm volatile("vse32.v v16, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v17, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v18, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v19, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v20, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v21, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v22, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v23, (%0)" :: "r"(res_base));
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);
            VQBDOTUA_VV(X24, V27, V8);
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V8, X24, X3);
            asm volatile("vse32.v v8, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v9, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v10, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v11, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v12, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v13, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v14, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v15, (%0)" :: "r"(res_base2));
            res_base2 += N;

            // Writeback

            if (!(j == N - 16 && i == N - 16)) {
                VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);
                asm volatile("vle8.v v0, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v1, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v2, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v3, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v4, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v5, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v6, (%0)" :: "r"(b_base));
                b_base += N;
                asm volatile("vle8.v v7, (%0)" :: "r"(b_base));

                asm volatile("vle8.v v24, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v25, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v26, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v27, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v28, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v29, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v30, (%0)" :: "r"(b_base2));
                b_base2 += N;
                asm volatile("vle8.v v31, (%0)" :: "r"(b_base2));
            }
        }
    }

    asm volatile("fence");
    asm volatile("csrr %0, cycle" : "=r"(cycles_end));
    printf("Cycles (BDot Multi-Acc) (Unroll M=32, rescheduled, old): %d\n", cycles_end - cycles_start);
    if (check) {
        for (int i = 0; i < N * N; i ++) {
            if (res[i] != r[i]) {
                printf("Bad value at index %d: got %d, expected %d\n", i, res[i], r[i]);
                exit(1);
            }
        }
    }
}

int debug() {
    int vl;
    int a = 128;
    VSETVLI_ALTFMT(vl, a, SEW_E8, LMUL_M1, 0);
    VDOTSETZEROBC_VV();
    // vs2
    asm volatile("vmv.v.i v0, 2");
    asm volatile("vmv.v.i v1, 3");
    asm volatile("vmv.v.i v2, 4");
    asm volatile("vmv.v.i v3, 5");
    asm volatile("vmv.v.i v4, 6");
    asm volatile("vmv.v.i v5, 7");
    asm volatile("vmv.v.i v6, 8");
    asm volatile("vmv.v.i v7, 9");
    // vs1
    asm volatile("vmv.v.i v8, 10");
    asm volatile("vmv.v.i v9, 11");
    asm volatile("vmv.v.i v10, 12");
    asm volatile("vmv.v.i v11, 13");
    asm volatile("vmv.v.i v12, 14");
    asm volatile("vmv.v.i v13, 15");
    asm volatile("vmv.v.i v14, -16");
    asm volatile("vmv.v.i v15, -15");

    VQBDOTUA_VV(X0, V3, V8);

    VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
    
    VDOTWB_VV(V16, X0, X0);
    VDOTWB_VV(V17, X1, X0);
    VDOTWB_VV(V18, X2, X0);
    VDOTWB_VV(V19, X3, X0);
    VDOTWB_VV(V20, X4, X0);
    VDOTWB_VV(V21, X5, X0);
    VDOTWB_VV(V22, X6, X0);
    VDOTWB_VV(V23, X7, X0);

    return 0;
}

int main() {

    // matmul_bdot_multi_acc(); // Warm up cache
    // matmul_opu();
    // matmul_bdot_multi_acc();
    // matmul_bdot_multi_acc_unroll_m_32_k_2();
    // matmul_bdot_multi_acc_unroll_m_32_k_2_rescheduled();

    // matmul_bdot_multi_acc_unroll_m_32_rescheduled();

    matmul_bdot_multi_acc_unroll_m_32_rescheduled_old(0);
    matmul_bdot_multi_acc_unroll_m_32_rescheduled_old(1);

    // matmul_bdot_multi_acc_unroll_m_32();
    // matmul_bdot_multi_acc_unroll_k_2();
    // matmul_bdot();
    // matmul_vector_inner();
    // matmul_scalar();

    exit(0);

    debug();

    exit(0);

    
    int res;
    int a = 128;
    int vl;
    int cycles_start;
    int cycles_end;

    VSETVLI_ALTFMT_X0(a, SEW_E32, LMUL_M2, 0);
    // vd
    asm volatile("vmv.v.i v24, 1");
    VSETVLI_ALTFMT(vl, a, SEW_E8, LMUL_M1, 0);
    // vs2
    asm volatile("vmv.v.i v8, 2");
    asm volatile("vmv.v.i v9, 3");
    asm volatile("vmv.v.i v10, 4");
    asm volatile("vmv.v.i v11, 5");
    asm volatile("vmv.v.i v12, 6");
    asm volatile("vmv.v.i v13, 7");
    asm volatile("vmv.v.i v14, 8");
    asm volatile("vmv.v.i v15, 9");
    // vs1
    VSETVLI_ALTFMT_X0(a, SEW_E8, LMUL_M4, 0);
    asm volatile("vmv.v.i v16, 3");

    // asm volatile("csrr %0, cycle" : "=r"(cycles_start));

    VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
    // VDOTSET_VV("x24", "x1");
    VDOTSETZERO_VV(X0);
    VDOTSETZERO_VV(X1);
    VSETVLI_ALTFMT_X0(a, SEW_E8, LMUL_M1, 0);
    VQBDOTUA_VV(X0, V8, V16);
    VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
    // VDOTSETZEROBC_VV();
    VSETVLI_ALTFMT_X0(a, SEW_E8, LMUL_M1, 0);
    // VQBDOTUA_VV(X1, V8, V16);
    // VQBDOTUA_VV("x8", "x16");
    // VQBDOTUA_VV("x8", "x16");
    // VQLDOTUA_VV("x8", "x16"); // Long dot product
    VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
    // STALL(100);
    VDOTWB_VV(V24, X0, X0);
    // VDOTWB_VV(V26, X1);
    // VDOTWB_VV("x16", "x1");

    // exit(0);

    // asm volatile("vmv.x.s x0, v24"); // Wait for writeback
    // asm volatile("fence");
    // asm volatile("csrr %0, cycle" : "=r"(cycles_end));

    for (int i = 0; i < 8; i ++) {
        asm volatile("vmv.x.s %0, v24" : "=r"(res));
        printf("Result %d: %d\n", i, res);
        asm volatile("vslidedown.vi v24, v24, 1");
    }

    exit(0);

    for (int i = 0; i < 8; i ++) {
        asm volatile("vmv.x.s %0, v26" : "=r"(res));
        printf("Result %d: %d\n", i, res);
        asm volatile("vslidedown.vi v26, v26, 1");
    }

    // exit(0);
    // printf("Cycles: %d\n", cycles_end - cycles_start);
    // printf("VL: %d\n", vl);
    // for (int i = 0; i < 8; i ++) {
    //     asm volatile("vmv.x.s %0, v24" : "=r"(res));
    //     printf("Result %d: %d\n", i, res);
    //     asm volatile("vslidedown.vi v24, v24, 1");
    // }

    // VSETVLI_ALTFMT_X0(a, SEW_E8, LMUL_M2, 1);
    // asm volatile("vmv.v.i v8, 5");
    // asm volatile("vmv.v.i v16, 3");
    // VQLDOTSA_VV("x24", "x8", "x16");
    // // STALL(100);
    // asm volatile("vmv.v.i v16, 4");
    // VQLDOTUA_VV("x24", "x8", "x16");
    // asm volatile("vsetvli zero, a0, e32, m1");
    // asm volatile("vmv.x.s %0, v24" : "=r"(res));
    // printf("Result: %d\n", res);

    return 0;
}