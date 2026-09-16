package saturn.backend

import chisel3._
import chisel3.util._
import chisel3.experimental.dataview._
import org.chipsalliance.cde.config._
import freechips.rocketchip.rocket._
import freechips.rocketchip.util._
import freechips.rocketchip.tile._
import saturn.common._
import saturn.insns._
import scala.math._
import saturn.exu._

class BDotWBSequencerIO(pipe_depth: Int, acc_delay: Int)(implicit p: Parameters) extends SequencerIO(new BDotWBSequencerControl) with HasVectorParams {
  val pipe_write_req = new VectorPipeWriteReqIO(1)
  val tail = Output(Bool())
  val in_flight = Input(UInt(32.W))
  val acc_intent = Output(UInt(32.W))
  val vbs_acc_intent = Input(UInt(32.W))
  val dis_acc_sel = Input(Bool())
}

class BDotWBSequencer(pipe_depth: Int, acc_delay: Int)(implicit p: Parameters) extends Sequencer[BDotWBSequencerControl]()(p) with HasVectorParams with HasCoreParameters {

  def accepts(inst: VectorIssueInst) = false.B

  val io = IO(new BDotWBSequencerIO(pipe_depth, acc_delay))

  // Current instruction
  val valid = RegInit(false.B)
  val busy = RegInit(false.B)
  val inst = Reg(new BackendIssueInst)
  
  val eg_idx = RegInit(0.U(log2Ceil(maxVLMax).W))
  val head = Reg(Bool())

  val writeback = RegInit(false.B)
  val acc_sel = Reg(Bool())

  val wvd_mask = Reg(UInt(egsTotal.W))
  val acc_mask = Reg(UInt(2.W))

  val eg_idx_max = (vLen / dLen).U

  val next_eg_idx = eg_idx +& 1.U

  val tail = (next_eg_idx === eg_idx_max) && io.iss.fire

  io.dis.ready := (!busy || tail) && !io.dis_stall && io.iss.ready

  // New instruction
  when (io.dis.fire) {
    val dis_inst = io.dis.bits

    assert(dis_inst.vstart === 0.U)

    writeback := true.B

    val dis_vd_arch_mask  = get_arch_mask(dis_inst.rd, 0.U) // TODO: Multi-vd outputs

    valid := true.B
    busy := true.B
    inst := io.dis.bits

    wvd_mask      := FillInterleaved(egsPerVReg, dis_vd_arch_mask)
    acc_mask      := 1.U << io.dis_acc_sel

    acc_sel := io.dis_acc_sel

    head := true.B
  } .elsewhen (io.iss.fire) {
    valid := !tail
    head := false.B
  }

  when (tail && !io.dis.fire) {
    busy := false.B
  }
  io.tail := tail

  io.vat := inst.vat
  io.seq_hazard.valid := busy
  io.seq_hazard.bits.rintent := 0.U
  io.seq_hazard.bits.wintent := hazardMultiply(wvd_mask)
  io.seq_hazard.bits.vat := inst.vat
  
  val wvd_eg = (inst.rd << log2Ceil(egsPerVReg)) + eg_idx

  val vd_write_oh = Mux(writeback, UIntToOH(wvd_eg), 0.U)

  val waw_hazard = (vd_write_oh & io.older_writes) =/= 0.U
  val war_hazard = (vd_write_oh & io.older_reads) =/= 0.U
  val data_hazard = waw_hazard | war_hazard

  val oldest = inst.vat === io.vat_head

  val current_rvd = (inst.rd << log2Ceil(egsPerVReg)) + eg_idx

  io.pipe_write_req.request := valid && writeback
  io.pipe_write_req.bank_sel := (if (vrfBankBits == 0) 1.U else UIntToOH(wvd_eg(vrfBankBits+log2Ceil(egsPerVReg)-1,log2Ceil(egsPerVReg))))
  io.pipe_write_req.pipe_depth := 0.U
  io.pipe_write_req.oldest := oldest
  io.pipe_write_req.fire := io.iss.fire

  when (io.iss.fire) {
    when (!tail) {
      eg_idx := next_eg_idx
      wvd_mask := wvd_mask & ~UIntToOH(current_rvd)
    } .otherwise {
      eg_idx := 0.U
    }
  }

  io.acc_intent := acc_mask

  io.iss.valid := (valid &&
    !data_hazard &&
    !io.in_flight(acc_sel) &&
    !io.vbs_acc_intent(acc_sel)
  )
  io.iss.bits.base_eg := wvd_eg
  io.iss.bits.writeback := writeback
  io.iss.bits.acc_sel := acc_sel
  io.iss.bits.acc_eidx := eg_idx

  io.busy := busy
  io.head := head
}