#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rvv_mx.h"
#include "bme.h"

extern const size_t M;
extern const size_t N;
extern const size_t K;
extern uint8_t a_src[] __attribute__((aligned(64)));
extern uint8_t b_src[] __attribute__((aligned(64)));
extern uint32_t r[] __attribute__((aligned(64)));
extern uint32_t rt[] __attribute__((aligned(64)));

// #define TCM_BASE 0x70000000

uint8_t *a;
uint8_t *b;

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
    uint32_t res[M * N];
    memset(res, 0, M * N * sizeof(uint32_t));
    int vl;
    asm volatile("csrr %0, cycle" : "=r"(cycles_start));

    VSETVLI_ALTFMT(vl, K, SEW_E8, LMUL_M1, 0);

    // Load first B0
    uint8_t *b0_base_first = b;
    asm volatile("vle8.v v16, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v17, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v18, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v19, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v20, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v21, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v22, (%0)" :: "r"(b0_base_first));
    b0_base_first += K;
    asm volatile("vle8.v v23, (%0)" :: "r"(b0_base_first));

    // Load first A0
    uint8_t *a0_base_first = a;
    asm volatile("vle8.v v0, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v1, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v2, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v3, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v4, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v5, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v6, (%0)" :: "r"(a0_base_first));
    a0_base_first += K;
    asm volatile("vle8.v v7, (%0)" :: "r"(a0_base_first));

    for (int j = 0; j < N; j += 16) {
        int j_K = j * K;
        int j_K2 = j_K + 8 * K;
        for (int i = 0; i < M; i += 16) {
            int i_K = i * K;
            uint32_t *res_base = res + i * N + j;
            uint32_t *res_base2 = res_base + 8;
            VDOTSETZEROBC_VV();
            int k;

            for (k = 0; k < K - vl; k += vl) {
                uint8_t *a0_base_next = a + k + vl + i_K; // Next iteration
                uint8_t *a1_base = a + k + i_K + 8 * K; // Current iteration
                uint8_t *b0_base_next = b + k + vl + j_K; // Next iteration
                uint8_t *b1_base = b + k + j_K2; // Current iteration

                VQBDOTUA_VV(X0, V19, V0); // A0 = V0, B0 = V16

                // Load B1 (current)
                asm volatile("vle8.v v24, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v25, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v26, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v27, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v28, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v29, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v30, (%0)" :: "r"(b1_base));
                b1_base += K;
                asm volatile("vle8.v v31, (%0)" :: "r"(b1_base));

                VQBDOTUA_VV(X16, V27, V0); // A0 = V0, B1 = V24

                // Load A1 (current)
                asm volatile("vle8.v v8, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v9, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v10, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v11, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v12, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v13, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v14, (%0)" :: "r"(a1_base));
                a1_base += K;
                asm volatile("vle8.v v15, (%0)" :: "r"(a1_base));

                VQBDOTUA_VV(X8, V19, V8); // A1 = V8, B0 = V16

                // Load A0 (next)
                asm volatile("vle8.v v0, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v1, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v2, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v3, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v4, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v5, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v6, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v7, (%0)" :: "r"(a0_base_next));

                VQBDOTUA_VV(X24, V27, V8); // A1 = V8, B1 = V24

                // Load B0 (next)
                asm volatile("vle8.v v16, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v17, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v18, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v19, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v20, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v21, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v22, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v23, (%0)" :: "r"(b0_base_next));
            }
            
            uint8_t *a0_base_next = a + i_K + K * 16; // Next iteration
            uint8_t *a1_base = a + k + i_K + 8 * K; // Current iteration
            uint8_t *b0_base_next = b + j_K; // Next iteration
            uint8_t *b1_base = b + k + j_K2; // Current iteration
            int final = j == N - 16 && i == M - 16;
            if (i == M - 16) {
                b0_base_next += K * 16;
                a0_base_next = a;
            }

            VQBDOTUA_VV(X0, V19, V0); // A0 = V0, B0 = V16

            // Load B1 (current)
            asm volatile("vle8.v v24, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v25, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v26, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v27, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v28, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v29, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v30, (%0)" :: "r"(b1_base));
            b1_base += K;
            asm volatile("vle8.v v31, (%0)" :: "r"(b1_base));

            VQBDOTUA_VV(X16, V27, V0); // A0 = V0, B1 = V24

            // Load A1 (current)
            asm volatile("vle8.v v8, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v9, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v10, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v11, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v12, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v13, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v14, (%0)" :: "r"(a1_base));
            a1_base += K;
            asm volatile("vle8.v v15, (%0)" :: "r"(a1_base));

            // Writeback A0 * B0 (acc0, using A0 registers)
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V0, X0, X3);
            asm volatile("vse32.v v0, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v1, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v2, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v3, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v4, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v5, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v6, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v7, (%0)" :: "r"(res_base));
            res_base += N;
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);

            VQBDOTUA_VV(X8, V19, V8); // A1 = V8, B0 = V16

            // Load A0 (next)
            if (!final) {
                asm volatile("vle8.v v0, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v1, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v2, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v3, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v4, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v5, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v6, (%0)" :: "r"(a0_base_next));
                a0_base_next += K;
                asm volatile("vle8.v v7, (%0)" :: "r"(a0_base_next));
            }

            // Writeback A0 * B1 (acc16, using B0 registers)
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V16, X16, X3);
            asm volatile("vse32.v v16, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v17, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v18, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v19, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v20, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v21, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v22, (%0)" :: "r"(res_base2));
            res_base2 += N;
            asm volatile("vse32.v v23, (%0)" :: "r"(res_base2));
            res_base2 += N;
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);

            VQBDOTUA_VV(X24, V27, V8); // A1 = V8, B1 = V24

            // Load B0 (next)
            if (!final) {
                asm volatile("vle8.v v16, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v17, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v18, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v19, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v20, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v21, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v22, (%0)" :: "r"(b0_base_next));
                b0_base_next += K;
                asm volatile("vle8.v v23, (%0)" :: "r"(b0_base_next));
            }

            // Writeback A1 * B0 (acc8, using B1 registers)
            VSETVLI_ALTFMT_X0(8, SEW_E32, LMUL_M1, 0);
            VDOTWB_VV(V24, X8, X3);
            asm volatile("vse32.v v24, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v25, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v26, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v27, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v28, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v29, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v30, (%0)" :: "r"(res_base));
            res_base += N;
            asm volatile("vse32.v v31, (%0)" :: "r"(res_base));
            res_base += N;
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);

            // Writeback A1 * B1 (acc24, using A1 registers)
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
            VSETVLI_ALTFMT_X0(vl, SEW_E8, LMUL_M1, 0);
        }
    }

    asm volatile("fence");
    asm volatile("csrr %0, cycle" : "=r"(cycles_end));
    printf("Cycles (BDot Multi-Acc) (Unroll M=32, rescheduled, old): %d\n", cycles_end - cycles_start);
    if (check) {
        for (int i = 0; i < M * N; i ++) {
            if (res[i] != r[i]) {
                printf("Bad value at index %d: got %d, expected %d\n", i, res[i], r[i]);
                exit(1);
            }
        }
        printf("Test passed\n");
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

#ifdef TCM_BASE
    a = (uint8_t *) TCM_BASE;
    b = a + M * K;

    memcpy(a, a_src, M * K);
    memcpy(b, b_src, N * K);
#else
    a = a_src;
    b = b_src;
#endif

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