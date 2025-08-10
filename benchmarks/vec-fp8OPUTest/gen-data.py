#!/usr/bin/env python3
import math, struct, sys, json, argparse, textwrap

# ---------- FP8 E4M3 encode/decode (IEEE-like, bias=7) ----------
# Format: 1 sign bit, 4 exponent bits (bias=7), 3 fraction bits
# Special cases:
#   exp=0, frac=0 => zero
#   exp=0, frac!=0 => subnormal: value = sign * 2^(1-bias) * (frac / 2^3)
#   exp=0xF, frac=0 => +/- Infinity
#   exp=0xF, frac!=0 => NaN
# Rounding: round-to-nearest, ties-to-even
BIAS_E4M3 = 7

def _round_half_even(x: float) -> int:
    # x can be positive
    f = math.floor(x)
    r = x - f
    if r > 0.5: return f + 1
    if r < 0.5: return f
    # tie
    return f if (f & 1) == 0 else f + 1

def float_to_fp8_e4m3(x: float) -> int:
    # Returns 8-bit int (0..255) bit pattern
    # Handle NaN / Inf
    if math.isnan(x):
        return 0b01111101  # quiet NaN (sign=0, exp=0xF, frac!=0)
    sign = 1 if math.copysign(1.0, x) < 0 else 0
    ax = abs(x)
    if math.isinf(ax):
        return (sign << 7) | (0xF << 3) | 0
    
    if ax == 0.0:
        return sign << 7

    # Normal/subnormal split
    # Want E and y such that ax = y * 2^E with y in [1,2)
    m, e = math.frexp(ax)  # ax = m * 2^e, m in [0.5,1)
    E = e - 1
    y = m * 2.0  # in [1,2)

    # Try normal first
    exp_code = E + BIAS_E4M3
    if 1 <= exp_code <= 0xE:
        frac_unrounded = (y - 1.0) * 8.0  # because 3 fraction bits
        frac = _round_half_even(frac_unrounded)
        if frac == 8:
            # carry into exponent
            frac = 0
            exp_code += 1
            if exp_code >= 0xF:
                # overflow to Inf
                return (sign << 7) | (0xF << 3) | 0
        if exp_code <= 0:
            # underflow into subnormal after rounding
            # fall-through into subnormal path
            pass
        else:
            return (sign << 7) | ((exp_code & 0xF) << 3) | (frac & 0x7)

    # Subnormal region
    # value = sign * 2^(1-bias) * (frac/8). Solve for frac
    # frac_target = ax / 2^(1-bias) * 8 = ax * 2^(bias-1+3) = ax * 2^(BIAS_E4M3+2) = ax * 2^9 = ax * 512
    frac_target = ax * 512.0
    frac = _round_half_even(frac_target)
    if frac == 0:
        return sign << 7
    if frac >= 8:
        # Rounds up to the smallest normal
        return (sign << 7) | (0x1 << 3) | 0
    return (sign << 7) | (0x0 << 3) | (frac & 0x7)

def fp8_e4m3_to_float(bits: int) -> float:
    bits &= 0xFF
    sign = -1.0 if (bits >> 7) & 1 else 1.0
    exp  = (bits >> 3) & 0xF
    frac = bits & 0x7

    if exp == 0:
        if frac == 0:
            return math.copysign(0.0, -1.0 if sign < 0 else 1.0) * 0.0
        # subnormal: 2^(1-bias) * (frac/8)
        return sign * math.ldexp(frac / 8.0, 1 - BIAS_E4M3)
    if exp == 0xF:
        if frac == 0:
            return sign * float('inf')
        return float('nan')
    # normal: 2^(exp-bias) * (1 + frac/8)
    return sign * math.ldexp(1.0 + (frac / 8.0), exp - BIAS_E4M3)

# ---------- Helpers for float32 and int32 bit patterns ----------

def to_float32(x: float) -> float:
    # quantize to IEEE-754 binary32
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]

def float32_to_int32_bits(x: float) -> int:
    # return signed int32 value with same bit pattern as float32
    u = int.from_bytes(struct.pack('<f', float(x)), 'little', signed=False)
    return u - (1 << 32) if u >= (1 << 31) else u

def int8_signed(u8: int) -> int:
    u8 &= 0xFF
    return u8 - 256 if u8 >= 128 else u8

def bits8(u8: int) -> str:
    return format(u8 & 0xFF, '08b')

def bits32(i32: int) -> str:
    # show two's complement 32-bit bitstring of signed int32
    return format(i32 & 0xFFFFFFFF, '032b')

# ---------- Core: outer product ----------

def outer_product_fp8_e4m3(a_fp, b_fp):
    """
    a_fp, b_fp: iterables of 16 Python floats (they will be quantized to FP8 E4M3)
    Returns dict with:
      - a_bits, b_bits: list of 16 integers in [-128,127] (signed int8 view of FP8 codes)
      - a_bits_bin, b_bits_bin: 8-bit binary strings
      - a_quant, b_quant: quantized float values (decoded from FP8)
      - C_float32: 16x16 list of lists of float32
      - C_int32: 16x16 list of lists of signed int32 raw bit patterns of the float32s
    """
    a_bits_u8 = [float_to_fp8_e4m3(x) for x in a_fp]
    b_bits_u8 = [float_to_fp8_e4m3(x) for x in b_fp]
    a_quant = [fp8_e4m3_to_float(u) for u in a_bits_u8]
    b_quant = [fp8_e4m3_to_float(u) for u in b_bits_u8]

    # Quantize inputs to float32 before multiplication
    a_q32 = [to_float32(x) for x in a_quant]
    b_q32 = [to_float32(x) for x in b_quant]

    # Outer product in float32 with final quantize to float32
    C_float32 = []
    C_int32 = []
    for i in range(16):
        row_f = []
        row_i = []
        for j in range(16):
            prod = to_float32(a_q32[i] * b_q32[j])
            row_f.append(prod)
            row_i.append(float32_to_int32_bits(prod))
        C_float32.append(row_f)
        C_int32.append(row_i)

    out = {
        "a_bits": [int8_signed(u) for u in a_bits_u8],
        "b_bits": [int8_signed(u) for u in b_bits_u8],
        "a_bits_bin": [format(u, '08b') for u in a_bits_u8],
        "b_bits_bin": [format(u, '08b') for u in b_bits_u8],
        "a_quant": a_q32,
        "b_quant": b_q32,
        "C_float32": C_float32,
        "C_int32": C_int32,
    }
    return out

def pretty_print_result(res):
    def fmtf(x: float) -> str:
        # compact float formatting
        return f"{x:.6g}"

    print("=== Inputs encoded to FP8 E4M3 (as signed int8 and 8-bit binary) ===")
    print("a_bits (int8):", res["a_bits"])
    print("a_bits (bin) :", res["a_bits_bin"])
    print("b_bits (int8):", res["b_bits"])
    print("b_bits (bin) :", res["b_bits_bin"])

    print("\n=== Quantized inputs (decoded FP8 -> float32) ===")
    print("a_quant:", [fmtf(x) for x in res["a_quant"]])
    print("b_quant:", [fmtf(x) for x in res["b_quant"]])

    print("\n=== Outer product result C = a ⊗ b (float32) ===")
    for row in res["C_float32"]:
        print("  ", " ".join(f"{fmtf(x):>10}" for x in row))

    print("\n=== C as int32 raw bit patterns (two's complement) ===")
    for row in res["C_int32"]:
        print("  ", " ".join(f"{x:>11d}" for x in row))

def main():
    parser = argparse.ArgumentParser(
        description="Outer product of two length-16 FP8 E4M3 vectors, computing in float32 and printing int32 bit patterns."
    )
    parser.add_argument("--a", type=str, default="", help="JSON list of 16 floats for vector a")
    parser.add_argument("--b", type=str, default="", help="JSON list of 16 floats for vector b")
    parser.add_argument("--example", action="store_true", help="Run a built-in example if no inputs are provided")
    args = parser.parse_args()

    if args.a and args.b:
        a = json.loads(args.a)
        b = json.loads(args.b)
        if len(a) != 16 or len(b) != 16:
            print("Both --a and --b must be length-16 lists.", file=sys.stderr)
            sys.exit(1)
    else:
        # Example: evenly spaced values in [-1.0, 1.0]
        a = [ -1.0 + (2.0 * i / 15.0) for i in range(16) ]
        b = [  1.0 - (2.0 * i / 15.0) for i in range(16) ]

    res = outer_product_fp8_e4m3(a, b)
    pretty_print_result(res)

if __name__ == "__main__":
    main()

