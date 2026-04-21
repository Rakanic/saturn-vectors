#!/usr/bin/env python3

import random
from typing import TextIO

m = 32
n = 32
k = 8192

a = [random.randrange(256) for _ in range(m * k)]
b = [random.randrange(256) for _ in range(n * k)]

r = [
    sum(a[x * k + i] * b[y * k + i] for i in range(k))
    for x in range(m)
    for y in range(n)
]

# rt = [
#     sum(a[x + i * k] * b[y + i * k] for i in range(k))
#     for x in range(m)
#     for y in range(n)
# ]

def write_matrix(file: TextIO, name: str, data: list[int], size: int, elems: int):
    file.write(f".global {name}\n")
    file.write(f".balign 64\n")
    file.write(f"{name}:\n")
    for i in range((elems * size) // 32):
        data_slice = reversed(data[i * (32 // size):(i + 1) * (32 // size)])
        data_hex = "".join(map(lambda x: format(x, f"0{size // 4}x"), data_slice))
        file.write(f"\t.word 0x{data_hex}\n")

with open("data.S", "w") as file:
    file.write(".section .data,\"aw\",@progbits\n")
    file.write(".global M\n")
    file.write(".balign 8\n")
    file.write("M:\n")
    file.write(f"\t.word 0x{m:08x}\n")
    file.write(".global N\n")
    file.write(".balign 8\n")
    file.write("N:\n")
    file.write(f"\t.word 0x{n:08x}\n")
    file.write(".global K\n")
    file.write(".balign 8\n")
    file.write("K:\n")
    file.write(f"\t.word 0x{k:08x}\n")
    write_matrix(file, "a_src", a, 8, m * k)
    file.write("\t.space 256\n")
    write_matrix(file, "b_src", b, 8, n * k)
    file.write("\t.space 256\n")
    write_matrix(file, "r", r, 32, m * n)
    # file.write("\t.space 256\n")
    # write_matrix(file, "rt", rt, 32, n * m)