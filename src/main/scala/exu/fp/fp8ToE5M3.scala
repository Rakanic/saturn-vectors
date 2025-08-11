package saturn.exu

import chisel3._

object fp8ToE5M3 {

	def apply(in: Bits, altfmt: Bool) = {
		Mux(altfmt, { // E5M2
			in ## 0.U(1.W)
		}, { // E4M3
			val sign = in(7)
			val exp = in(6, 3)
			val sig = in(2, 0)
			val isNaN = exp.andR && sig.andR
			val isSubnormal = !exp.orR
			val adjustedExp = (isNaN.asUInt ## exp) + Mux(isNaN || isSubnormal, 0.U, 8.U)
			sign ## adjustedExp ## sig
		})
	}
}