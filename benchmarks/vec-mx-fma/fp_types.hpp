#include "lo_float.h"

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