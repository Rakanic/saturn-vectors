package saturn.exu

import chisel3._
import freechips.rocketchip.tile._

object rawUnroundedToFp8 {

	def apply(unroundedType: FType, unroundedIn: hardfloat.RawFloat, unroundedInvalidExc: Bool, altfmt: Bool, roundingMode: Bits, saturate: Bool) = {
		val e5m2Narrower = Module(new hardfloat.RoundAnyRawFNToRecFN(unroundedType.exp, unroundedType.sig + 2, FType.E5M2.exp, FType.E5M2.sig, 0))
		e5m2Narrower.io.in := unroundedIn
		e5m2Narrower.io.roundingMode := roundingMode
		e5m2Narrower.io.detectTininess := hardfloat.consts.tininess_afterRounding
		e5m2Narrower.io.invalidExc := unroundedInvalidExc
		e5m2Narrower.io.infiniteExc := false.B

		val e5m3Narrower = Module(new hardfloat.RoundAnyRawFNToRecFN(unroundedType.exp, unroundedType.sig + 2, FType.E5M3.exp, FType.E5M3.sig, 0))
		e5m3Narrower.io.in := unroundedIn
		e5m3Narrower.io.roundingMode := roundingMode
		e5m3Narrower.io.detectTininess := hardfloat.consts.tininess_afterRounding
		e5m3Narrower.io.invalidExc := unroundedInvalidExc
		e5m3Narrower.io.infiniteExc := false.B

		val e4m3Narrower = Module(new hardfloat.RoundAnyRawFNToRecFN(unroundedType.exp, unroundedType.sig + 2, FType.E4M3.exp, FType.E5M3.sig, 0))
		e4m3Narrower.io.in := unroundedIn
		e4m3Narrower.io.roundingMode := roundingMode
		e4m3Narrower.io.detectTininess := hardfloat.consts.tininess_afterRounding
		e4m3Narrower.io.invalidExc := unroundedInvalidExc
		e4m3Narrower.io.infiniteExc := false.B

		val outBits = Wire(UInt(8.W))
		val exceptionFlags = Wire(UInt(5.W))
		outBits := DontCare
		exceptionFlags := DontCare

		when (altfmt) { // E5M2
			val e5m2Ieee = FType.E5M2.ieee(e5m2Narrower.io.out)
			outBits := saturateE5M2(e5m2Ieee, saturate)
			exceptionFlags := e5m2Narrower.io.exceptionFlags
		} .otherwise { // E4M3
			val e5m3Ieee = FType.E5M3.ieee(e5m3Narrower.io.out)
			val e4m3Ieee = FType.E4M3.ieee(e4m3Narrower.io.out)
			outBits := assembleOFPE4M3(e5m3Ieee, e4m3Ieee, saturate)
			exceptionFlags := 0.U(5.W)
		}

		(outBits, exceptionFlags)
	}
}

object saturateE5M2 {

	def apply(in: UInt, saturate: Bool) = {
		val sign = in(7)
		val rest = in(6, 0)
		Mux(saturate && rest === "b1111100".U(7.W), sign ## "b1111011".U(7.W), in)
	}
}

object assembleOFPE4M3 {
	
	def apply(ieeeE5M3: UInt, ieeeE4M3: UInt, saturate: Bool) = {
		val sign = ieeeE4M3(7)
		val expE4M3 = ieeeE4M3(6, 3)
		val sigE4M3 = ieeeE4M3(2, 0)
		val expE5M3 = ieeeE5M3(7, 3)
		val sigE5M3 = ieeeE5M3(2, 0)
		val special = expE4M3 === "b1111".U(4.W)
		val possibleInf = expE5M3(4, 3) === "b11".U(2.W) || sigE5M3 === "b111".U(3.W)
		val outValue = Mux(special, // Possible NaN or Inf
			sign ## Mux(sigE4M3 =/= "b000".U(3.W), // NaN
				"b1111111".U(7.W),
				Mux(possibleInf, // Inf
					"b111111".U(6.W) ## !saturate,
					"b1111".U(4.W) ## sigE5M3
				)
			),
			ieeeE4M3
		)
		outValue
	}
}