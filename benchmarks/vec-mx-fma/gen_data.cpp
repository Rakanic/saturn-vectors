#include <cstdio>
#include <iostream>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <functional>
#include "lo_float.h"

#define COUNT 128
#define SPECIAL_COUNT 20

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
struct FN_InfChecker {
	bool operator()(uint32_t bits) const {
		return false;
	}
	
	uint32_t minNegInf() const {
		return (1 << (exp + sig)) - 1;
	}

	uint32_t minPosInf() const {
		return (1 << (exp + sig)) - 1;
	}
};

template <int exp, int sig>
struct FN_NaNChecker {
	bool operator()(uint32_t bits) const {
		return (bits & ((1 << (exp + sig)) - 1)) == (1 << (exp + sig)) - 1;
	}
	
	uint32_t qNanBitPattern() const {
		return (1 << (exp + sig)) - 1;
	}

	uint32_t sNanBitPattern() const {
		return (1 << (exp + sig)) - 1;
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

template <int exp, int sig>
constexpr lo_float::FloatingPointParams param_fn(
	exp + sig + 1, // Total bitwidth
	sig, // Mantissa
	(1 << (exp - 1)) - 1, // Bias
	lo_float::Rounding_Mode::RoundToNearestEven,
	lo_float::Inf_Behaviors::Extended,
	lo_float::NaN_Behaviors::QuietNaN,
	lo_float::Signedness::Signed,
	FN_InfChecker<exp, sig>(),
	FN_NaNChecker<exp, sig>(),
	1 // stoch_len
);

const char fp32name[] = "fp32";
const char fp16name[] = "fp16";
const char bf16name[] = "bf16";
const char ofp8e5m2name[] = "ofp8e5m2";
const char ofp8e4m3name[] = "ofp8e4m3";
const char fp16Wname[] = "fp16W";
const char bf16Wname[] = "bf16W";
const char ofp8e5m2Wname[] = "ofp8e5m2W";
const char ofp8e4m3Wname[] = "ofp8e4m3W";


template <class T, class W, auto NAME, size_t WSIZE>
void operand_test(T vals_a[], T vals_b[], T vals_c[], const char op_name[], std::function<double(double, double, double)> op_func) {
	std::cout << ".global " << NAME << '_' << op_name << std::endl << ".balign 64" << std::endl << NAME << '_' << op_name << ':' << std::endl;
	for (size_t i = 0; i < COUNT / (4 / WSIZE); i ++) {
		std::cout << "    .word 0x";
		for (int j = 4 / WSIZE - 1; j >= 0; j --) {
			double out_a = static_cast<double>(vals_a[i * (4 / WSIZE) + j]);
			double out_b = static_cast<double>(vals_b[i * (4 / WSIZE) + j]);
			double out_c = static_cast<double>(vals_c[i * (4 / WSIZE) + j]);
			std::cout << std::hex << std::setfill('0') << std::setw(WSIZE * 2) << (int) static_cast<W>(op_func(out_a, out_b, out_c)).rep();
		}
		std::cout << std::endl;
	}
}

template <class T, size_t SIZE, auto NAME, double MIN, double MAX, double SUBNORM_MIN, double SUBNORM_MAX, bool WIDEN, class W>
void test() {

	const size_t WSIZE = WIDEN ? SIZE * 2 : SIZE;

	T vals_a[COUNT];
	T vals_b[COUNT];
	T vals_c[COUNT];

	std::mt19937 e2;
	e2.seed(0);
	std::uniform_real_distribution<> dist(MIN, MAX);
	std::uniform_real_distribution<> subnorm_dist(SUBNORM_MIN, SUBNORM_MAX);

	for (size_t i = 0; i < SPECIAL_COUNT; i ++) {
		vals_a[i] = static_cast<T>(subnorm_dist(e2));
		vals_b[i] = static_cast<T>(subnorm_dist(e2));
		vals_c[i] = static_cast<T>(subnorm_dist(e2));
	}

	for (size_t i = SPECIAL_COUNT; i < COUNT; i ++) {
		vals_a[i] = static_cast<T>(dist(e2));
		vals_b[i] = static_cast<T>(dist(e2));
		vals_c[i] = static_cast<T>(dist(e2));
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
	vals_b[8] = static_cast<T>(0);

	std::cout << ".global " << NAME << "a" << std::endl << ".balign 64" << std::endl << NAME << "a:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (int j = 4 / SIZE - 1; j >= 0; j --) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) vals_a[i * (4 / SIZE) + j].rep();
		}
		std::cout << std::endl;
	}

	std::cout << ".global " << NAME << "b" << std::endl << ".balign 64" << std::endl << NAME << "b:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (int j = 4 / SIZE - 1; j >= 0; j --) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) vals_b[i * (4 / SIZE) + j].rep();
		}
		std::cout << std::endl;
	}

	std::cout << ".global " << NAME << "c" << std::endl << ".balign 64" << std::endl << NAME << "c:" << std::endl;
	for (size_t i = 0; i < COUNT / (4 / SIZE); i ++) {
		std::cout << "    .word 0x";
		for (int j = 4 / SIZE - 1; j >= 0; j --) {
			std::cout << std::hex << std::setfill('0') << std::setw(SIZE * 2) << (int) vals_c[i * (4 / SIZE) + j].rep();
		}
		std::cout << std::endl;
	}

	operand_test<T, W, NAME, WSIZE>(vals_a, vals_b, vals_c, "add", [](double a, double b, double c) -> double {return a + b;});
	operand_test<T, W, NAME, WSIZE>(vals_a, vals_b, vals_c, "mul", [](double a, double b, double c) -> double {return a * b;});
	operand_test<T, W, NAME, WSIZE>(vals_a, vals_b, vals_c, "sub", [](double a, double b, double c) -> double {return a - b;});
	operand_test<T, W, NAME, WSIZE>(vals_a, vals_b, vals_c, "macc", [](double a, double b, double c) -> double {return a * b + c;});
}

int main() {

	using fp32 = lo_float::Templated_Float<param_std<8, 23>>;
	using fp16 = lo_float::Templated_Float<param_std<5, 10>>;
	using bf16 = lo_float::Templated_Float<param_std<8, 7>>;
	using ofp8e5m2 = lo_float::Templated_Float<param_std<5, 2>>;
	using ofp8e4m3 = lo_float::Templated_Float<param_fn<4, 3>>;

	std::cout << ".section .data,\"aw\",@progbits" << std::endl;

	std::cout << ".global N" << std::endl \
		<< ".balign 8" << std::endl \
		<< "N:" << std::endl \
		<< "    .word 0x" << std::hex << std::setfill('0') << std::setw(8) << COUNT << std::endl \
		<< "    .word 0x00000000" << std::endl;
	
	test<fp32, 4, fp32name, -1e2, 1e2, -1e-6, 1e-6, false, fp32>();
	test<fp16, 2, fp16name, -1e2, 1e2, -1e-6, 1e-6, false, fp16>();
	test<bf16, 2, bf16name, -1e15, 1e15, -2e-38, 2e-38, false, bf16>();
	test<ofp8e5m2, 1, ofp8e5m2name, -1e2, 1e2, -1e-4, 1e-4, false, ofp8e5m2>();
	test<ofp8e4m3, 1, ofp8e4m3name, -3e1, 3e1, -1e-1, 1e-1, false, ofp8e4m3>();
	test<fp16, 2, fp16Wname, -1e2, 1e2, -1e-6, 1e-6, true, fp32>();
	test<bf16, 2, bf16Wname, -1e15, 1e15, -2e-38, 2e-38, true, fp32>();
	test<ofp8e5m2, 1, ofp8e5m2Wname, -1e2, 1e2, -1e-4, 1e-4, true, bf16>();
	test<ofp8e4m3, 1, ofp8e4m3Wname, -1e2, 1e2, -1e-3, 1e-3, true, bf16>();

	return 0;
}