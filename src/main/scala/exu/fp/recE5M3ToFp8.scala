package saturn.exu

import chisel3._
import freechips.rocketchip.tile._

object recE5M3ToFp8 {

	def apply(in: UInt, unroundedIn: hardfloat.RawFloat, unroundedInvalidExc: Bool, altfmt: Bool, roundingMode: Bits, saturate: Bool) = {
		val e5m2Narrower = Module(new hardfloat.RoundAnyRawFNToRecFN(FType.E5M3.exp, FType.E5M3.sig + 2, FType.E5M2.exp, FType.E5M2.sig, 0))
		e5m2Narrower.io.in := unroundedIn
		e5m2Narrower.io.roundingMode := roundingMode
		e5m2Narrower.io.detectTininess := hardfloat.consts.tininess_afterRounding
		e5m2Narrower.io.invalidExc := unroundedInvalidExc
		e5m2Narrower.io.infiniteExc := false.B

		val outBits = Wire(UInt(8.W))
		val exceptionFlags = Wire(UInt(5.W))
		outBits := DontCare
		exceptionFlags := DontCare

		when (altfmt) { // E5M2
			val e5m2Ieee = e5m2Narrower.io.out
			val sign = e5m2Ieee(7)
			val rest = e5m2Ieee(6, 0)
			val out = Mux(saturate && rest === "b1111100".U(7.W), sign ## "b1111011".U(7.W), e5m2Ieee)
			outBits := FType.E5M2.ieee(e5m2Narrower.io.out)
			exceptionFlags := e5m2Narrower.io.exceptionFlags
		} .otherwise { // E4M3
			val e5m3Ieee = FType.E5M3.ieee(in)
			val sign = e5m3Ieee(8)
			val exp = e5m3Ieee(7, 3)
			val sig = e5m3Ieee(2, 0)
			val isNaN = exp.andR && sig.orR
			val isSubnormal = !exp.orR
			val adjustedExp = exp(4, 0) - Mux(isNaN || isSubnormal, 0.U, 8.U)
			val overflow = adjustedExp(4)
			val out = Mux(isNaN, "h7f".U(8.W), Mux(
				overflow, Mux(saturate, sign ## "b1111110".U(7.W), sign ## "b1111111".U(7.W)), sign ## adjustedExp(3, 0) ## sig
			))
			outBits := out
			exceptionFlags := 0.U(5.W)
		}

		(outBits, exceptionFlags)
	}
}