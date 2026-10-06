package saturn.exu

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config._
import freechips.rocketchip.rocket._
import freechips.rocketchip.util._
import freechips.rocketchip.tile._
import chisel3.util.experimental.decode._
import saturn.common._
import saturn.backend._

class BDotSequencerControl(implicit p: Parameters) extends CoreBundle()(p) with HasVectorParams {
  val head = Input(Bool())
  val tail_in = Input(Bool())
  val acc_in = Input(UInt((8 * 32).W))

  val altfmt = Input(Bool())
  val signed = Input(Bool())
}

class BDotUnitArbiter(n: Int)(implicit p: Parameters) extends CoreModule()(p) with HasVectorParams {
  
  val io = IO(new Bundle {
    val seqs = Vec(n, Flipped(Decoupled(new BDotSequencerControl)))
    val seq_valid_likely = Input(Vec(n, Bool()))

    val seq_req_rvs1 = Vec(n, Flipped(Decoupled(new VectorReadReq)))
    val seq_req_rvs2 = Vec(n, Flipped(Decoupled(new VectorReadReq)))
    val seq_req_batch_rvs2 = Input(Vec(n, Bool()))
    val seq_req_rvd = Vec(n, Flipped(Decoupled(new VectorReadReq)))

    val seq_write = Vec(n, Flipped(Decoupled(new VectorWrite(dLen))))

    
    val unit = Decoupled(new BDotSequencerControl)
    val tail_out = Input(Bool())
    val sequencer_idx = Output(UInt(n.W))

    val req_rvs1 = Decoupled(new VectorReadReq)
    val req_rvs2 = Decoupled(new VectorReadReq)
    val req_batch_rvs2 = Output(Bool())
    val req_rvd = Decoupled(new VectorReadReq)

    val write = Decoupled(new VectorWrite(dLen))
  })

  val lock = RegInit(0.U(n.W))
  val priority = PriorityEncoderOH(io.seq_valid_likely)
  val choice = Mux(lock.orR, lock, priority.asUInt)

  io.sequencer_idx := choice

  io.unit.valid := false.B
  io.unit.bits := DontCare
  io.req_rvs1.valid := false.B
  io.req_rvs1.bits := DontCare
  io.req_rvs2.valid := false.B
  io.req_rvs2.bits := DontCare
  io.req_rvd.valid := false.B
  io.req_rvd.bits := DontCare

  var req_batch_rvs2 = WireInit(false.B)
  io.req_batch_rvs2 := req_batch_rvs2 && io.req_rvs2.fire

  when (!lock.orR) {
    lock := choice
  }

  choice.asBools.zip(io.seqs).zipWithIndex.foreach { case ((c, s), i) =>
    s.bits := DontCare
    io.seq_req_rvs1(i).ready := false.B
    io.seq_req_rvs2(i).ready := false.B
    io.seq_req_rvs1(i).bits := DontCare
    io.seq_req_rvs2(i).bits := DontCare
    when (c) {
      s.ready := io.unit.ready
      io.unit.valid := s.valid
      s.bits <> io.unit.bits
      io.seq_req_rvs1(i) <> io.req_rvs1
      io.seq_req_rvs2(i) <> io.req_rvs2
      req_batch_rvs2 := io.seq_req_batch_rvs2(i)
      when (s.bits.tail_in && s.fire) {
        lock := 0.U
      }
    } .otherwise {
      s.ready := false.B
    }
  }

  val rvd_req_priority = PriorityEncoderOH(io.seq_req_rvd.map { r => r.valid })
  
  rvd_req_priority.zip(io.seq_req_rvd).foreach { case (c, req) =>
    req.ready := false.B
    req.bits := DontCare
    when (c) {
      req <> io.req_rvd
    }
  }

  val write_priority = PriorityEncoderOH(io.seq_write.map { w => w.valid })
  
  io.write.valid := false.B
  io.write.bits := DontCare

  write_priority.zip(io.seq_write).foreach { case (c, w) =>
    w.ready := false.B
    when (c) {
      io.write <> w
    }
  }
}

class DotPipe(output_width: Int)(implicit p: Parameters) extends CoreModule()(p) with HasVectorParams {
  val io = IO(new Bundle {
    val valid = Input(Bool())
    val signed_a = Input(Bool())
    val signed_b = Input(Bool())
    val in_a = Input(UInt(dLen.W))
    val in_b = Input(UInt(dLen.W))
    val acc = Input(UInt(output_width.W))
    val set_acc = Input(Bool())
    val set_acc_value = Input(UInt(output_width.W))
    val out = Output(UInt(output_width.W))
    val out_en = Output(Bool())
  })
}

class IntegerDotPipe(pipe_depth: Int, acc_delay: Int, input_width: Int, output_width: Int)(implicit p: Parameters) extends DotPipe(output_width)(p) {

  val a = io.in_a.asTypeOf(Vec(dLen / input_width, UInt(input_width.W)))
  val b = io.in_b.asTypeOf(Vec(dLen / input_width, UInt(input_width.W)))

  val a_signs = a.map { e => io.signed_a && e(input_width - 1) }
  val b_signs = b.map { e => io.signed_b && e(input_width - 1) }
  val a_unsigned = a.zip(a_signs).map { case (e, signed) => Mux(signed, ~e + 1.U, e) }
  val b_unsigned = b.zip(b_signs).map { case (e, signed) => Mux(signed, ~e + 1.U, e) }

  val prods_unsigned = a_unsigned.zip(b_unsigned).map { case (x, y) => x * y }
  val prod_signs = a_signs.zip(b_signs).map { case (x, y) => x ^ y }
  val prods_short = prods_unsigned.zip(prod_signs).map { case (e, signed) => Mux(signed, ~e + 1.U, e) }
  val prods = prods_short.zip(prod_signs).map { case (e, sign) => Fill(output_width - e.getWidth, sign) ## e }
  val sum = prods.foldLeft(0.U) { (x, y) => x + y }
  
  val sum_pipe = Pipe(io.valid, sum, pipe_depth)
  val set_acc_pipe = Pipe(io.valid && io.set_acc, io.set_acc_value, pipe_depth)
  val acc_pipe = Pipe(sum_pipe.valid, sum_pipe.bits +& Mux(set_acc_pipe.valid, set_acc_pipe.bits, io.acc), acc_delay - 1)

  io.out := acc_pipe.bits
  io.out_en := acc_pipe.valid
}

class BDotUnit(pipe_depth: Int, acc_delay: Int)(implicit p: Parameters) extends CoreModule()(p) with HasVectorParams {
  
  val io = IO(new Bundle {
    val op = Flipped(Decoupled(new BDotSequencerControl))
    val tail_out = Output(Bool())
    val acc_out = Output(UInt((8 * 32).W))
    val rvs1_data = Input(UInt(dLen.W))
    val batch_rvs2_data = Input(Vec(vParams.vrfBanking, UInt(dLen.W)))
    val sequencer_idx = Input(UInt(nBDotSeqs.W))
    val sequencer_idx_out = Output(UInt(nBDotSeqs.W))
  })

  val acc_buffer = Reg(Vec(8, UInt(32.W)))

  for (i <- 0 until 8) {
    val int8_pipe = Module(new IntegerDotPipe(pipe_depth, acc_delay, 8, 32))
    int8_pipe.io.valid := io.op.fire
    int8_pipe.io.signed_a := io.op.bits.altfmt
    int8_pipe.io.signed_b := io.op.bits.signed
    int8_pipe.io.in_a := io.rvs1_data
    int8_pipe.io.in_b := io.batch_rvs2_data(i)
    int8_pipe.io.acc := acc_buffer(i)
    int8_pipe.io.set_acc := io.op.bits.head
    int8_pipe.io.set_acc_value := io.op.bits.acc_in.asTypeOf(acc_buffer)(i)

    when (int8_pipe.io.out_en) {
      acc_buffer(i) := int8_pipe.io.out
    }
  }

  val tail_pipe = Pipe(io.op.fire && io.op.bits.tail_in, io.sequencer_idx, pipe_depth + acc_delay)

  io.op.ready := true.B
  io.tail_out := tail_pipe.valid
  io.sequencer_idx_out := tail_pipe.bits
  io.acc_out := acc_buffer.asTypeOf(UInt((8 * 32).W))
}