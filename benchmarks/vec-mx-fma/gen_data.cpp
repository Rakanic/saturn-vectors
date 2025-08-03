#include <cstdio>
#include <iostream>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include "lo_float.h"

#define COUNT 128

template <int exp, int sig>
struct Std_InfChecker {
	bool operator()(uint32_t bits) const {
		return (bits & ((1 << (exp + sig)) - 1)) == ((1 << exp) - 1) << sig;
	}
	
	uint32_t minNegInf() const {
		return ((1 << (exp + 1)) - 1) << sig;
	}

	uint32_t minPosInf() const {
		return ((1 << exp) - 1) << sig;
	}
};

template <int exp, int sig>
struct Std_NaNChecker {
	bool operator()(uint32_t bits) const {
		return (bits & ((1 << exp) - 1) << sig) == ((1 << exp) - 1) << sig && (bits & ((1 << sig) - 1)) != 0;
	}
	
	uint32_t qNanBitPattern() const {
		return ((1 << (exp + 1)) - 1) << (sig - 1);
	}

	uint32_t sNanBitPattern() const {
		return ((1 << (exp + 1)) - 1) << (sig - 1);
	}
};

template <int exp, int sig>
constexpr lo_float::FloatingPointParams param_std(
	exp + sig + 1, // Total bitwidth
	sig, // Mantissa
	(1 << (exp - 1)) - 1, // Bias
	lo_float::Rounding_Mode::RoundToNearestEven,
	lo_float::Inf_Behaviors::Extended,
	lo_float::NaN_Behaviors::QuietNaN,
	lo_float::Signedness::Signed,
	Std_InfChecker<exp, sig>(),
	Std_NaNChecker<exp, sig>(),
	1 // stoch_len
);

const char fp16name[] = "fp16";
const char bf16name[] = "bf16";
const char ofp8e5m2name[] = "ofp8e5m2";


template <class T, size_t SIZE, auto NAME, double MIN, double MAX>
void test() {

	T vals_a[COUNT];
	T vals_b[COUNT];

	std::random_device rd;
	std::mt19937 e2(rd());
	std::uniform_real_distribution<> dist(MIN, MAX);

	for (size_t i = 0; i < COUNT; i ++) {
		vals_a[i] = static_cast<T>(dist(e2));
		vals_b[i] = static_cast<T>(dist(e2));
	}

	vals_a[0] = static_cast<T>(INFINITY);
	vals_a[1] = static_cast<T>(NAN);

	vals_b[2] = static_cast<T>(INFINITY);
	vals_b[3] = static_cast<T>(NAN);

	vals_a[4] = static_cast<T>(-INFINITY);
	vals_a[5] = static_cast<T>(-NAN);

	vals_a[6] = static_cast<T>(INFINITY);
	vals_b[6] = static_cast<T>(NAN);

	vals_a[7] = static_cast<T>(0);

	vals_a[8] = static_cast<T>(INFINITY);
	vals_a[8] = static_cast<T>(0);

	std::cout << ".global " << NAME << "a" << std::endl << ".balign 64" << std::endl << NAME << "a:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (size_t j = 0; j < 4 / SIZE; j ++) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) vals_a[i * (4 / SIZE) + j].rep();
		}
		std::cout << std::endl;
	}

	std::cout << ".global " << NAME << "b" << std::endl << ".balign 64" << std::endl << NAME << "b:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (size_t j = 0; j < 4 / SIZE; j ++) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) vals_b[i * (4 / SIZE) + j].rep();
		}
		std::cout << std::endl;
	}

	std::cout << ".global " << NAME << "r" << std::endl << ".balign 64" << std::endl << NAME << "r:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (size_t j = 0; j < 4 / SIZE; j ++) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) (vals_a[i * (4 / SIZE) + j] * vals_b[i * (4 / SIZE) + j]).rep();
		}
		std::cout << std::endl;
	}
}

int main() {

	using fp16 = lo_float::Templated_Float<param_std<5, 10>>;
	using bf16 = lo_float::Templated_Float<param_std<8, 7>>;
	using ofp8e5m2 = lo_float::Templated_Float<param_std<5, 2>>;

	std::cout << ".section .data,\"aw\",@progbits" << std::endl;

	std::cout << ".global N" << std::endl \
		<< ".balign 8" << std::endl \
		<< "N:" << std::endl \
		<< "    .word 0x" << std::hex << std::setfill('0') << std::setw(8) << COUNT << std::endl \
		<< "    .word 0x00000000" << std::endl;
	
		test<fp16, 2, fp16name, -1e2, 1e2>();
		test<bf16, 2, bf16name, -1e15, 1e15>();
		// test<ofp8e5m2, 1, ofp8e5m2name>();

	return 0;
}