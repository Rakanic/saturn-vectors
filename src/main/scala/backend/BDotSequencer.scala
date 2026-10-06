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

class BDotSequencerIO()(implicit p: Parameters) extends SequencerIO(new BDotSequencerControl) with HasVectorParams {
  val rvs1 = Decoupled(new VectorReadReq)
  val rvs2 = Decoupled(new VectorReadReq)
  val rvd = Decoupled(new VectorReadReq)
  val batch_read_vs2 = Output(Bool())
  val tail = Output(Bool())
  val rvd_data = Input(UInt(dLen.W))
  val write = Decoupled(new VectorWrite(dLen))

  val valid_likely = Output(Bool())
  val acc_out = Input(UInt((8 * 32).W))
  val acc_out_valid = Input(Bool())
}

class BDotSequencer()(implicit p: Parameters) extends Sequencer[BDotSequencerControl]()(p) with HasVectorParams with HasCoreParameters {

  val bdot_insns = vParams.bdotInsns

  def accepts(inst: VectorIssueInst) = !inst.vmu && new VectorDecoder(inst, bdot_insns, Nil).matched

  val io = IO(new BDotSequencerIO)

  val valid = RegInit(false.B)
  val inst = Reg(new BackendIssueInst)

  val rvs1_mask = Reg(UInt(egsTotal.W))
  val rvs2_mask = Reg(UInt(egsTotal.W))
  val rvd_mask = Reg(UInt(egsTotal.W))
  val ci = Reg(UInt(3.W))

  val altfmt = Reg(Bool())
  val signed = Reg(Bool())

  val data_egs = egsPerVReg
  val data_eg_idx = RegInit(0.U((log2Ceil(egsPerVReg) + 1).W))
  val data_eg_idx_next = data_eg_idx + 1.U
  val data_load_done = data_eg_idx >= data_egs.U
  val data_load_tail = data_eg_idx_next >= data_egs.U

  val acc_egs = 32 * 8 / dLen
  val acc_eg_idx = RegInit(0.U((log2Ceil(acc_egs) + 1).W))
  val acc_eg_idx_next = acc_eg_idx + 1.U
  val acc_load_done = acc_eg_idx >= acc_egs.U
  val acc_load_tail = acc_eg_idx_next >= acc_egs.U

  val wb_eg_idx = RegInit(0.U((log2Ceil(acc_egs) + 1).W))
  val wb_eg_idx_next = wb_eg_idx + 1.U
  val wb_store_done = wb_eg_idx >= acc_egs.U
  val wb_store_tail = wb_eg_idx_next >= acc_egs.U
  val start_wb = RegInit(false.B)

  val acc_buffer = Reg(Vec(acc_egs, UInt(dLen.W)))

  val head = valid && acc_eg_idx === 0.U
  val tail = valid && wb_store_tail && !wb_store_done
  val busy = valid

  io.dis.ready := (!busy || tail) && !io.dis_stall

  when (io.dis.fire) {
    val dis_inst = io.dis.bits

    assert(dis_inst.vstart === 0.U)

    val dis_ctrl = new VectorDecoder(dis_inst, bdot_insns, Seq(BDotSigned))

    altfmt := dis_inst.vconfig.vtype.altfmt
    signed := dis_ctrl.bool(BDotSigned)

    val dis_inst_ci = dis_inst.rs2(2, 0)
    val dis_vs1_arch_mask = get_arch_mask(dis_inst.rs1, 0.U)
    val dis_vs2_arch_mask = get_arch_mask(dis_inst.rs2(4, 3) & "h18".U(5.W), 3.U)
    val dis_vd_arch_mask = get_arch_mask(dis_inst.rd, 0.U)

    valid := true.B
    inst := io.dis.bits

    data_eg_idx := 0.U
    acc_eg_idx := 0.U
    wb_eg_idx := 0.U
    start_wb := false.B

    rvs1_mask := FillInterleaved(egsPerVReg, dis_vs1_arch_mask)
    rvs2_mask := FillInterleaved(egsPerVReg, dis_vs2_arch_mask)
    // rvd_mask := FillInterleaved(egsPerVReg, dis_vd_arch_mask)
    val rvd_shift = (dis_inst_ci * ((8 * 32) / dLen).U)
    val rvd_width = (1 << ((8 * 32) / dLen)) - 1
    rvd_mask := VecInit(dis_vd_arch_mask.asBools.map { case b => Mux(b, (rvd_width.U << rvd_shift)(egsPerVReg-1, 0), 0.U(egsPerVReg.W)) }).asUInt
    ci := dis_inst_ci
  } .elsewhen (tail) {
    valid := false.B
  }

  when (io.rvd.fire) {
    when (!data_load_done) {
      acc_eg_idx := acc_eg_idx_next
      acc_buffer(acc_eg_idx) := io.rvd_data
    }
  }

  when (io.iss.fire) {
    data_eg_idx := data_eg_idx_next
  }

  when (io.acc_out_valid && data_load_done) {
    acc_buffer := io.acc_out.asTypeOf(acc_buffer)
    start_wb := true.B
  }

  when (io.write.fire && !io.dis.fire) {
    wb_eg_idx := wb_eg_idx_next
  }

  val renv1 = valid && !data_load_done
  val renv2 = valid && !data_load_done
  val renvd = valid && !acc_load_done
  val wenvd = valid && (start_wb || (io.acc_out_valid && data_load_done))

  io.vat := inst.vat
  io.seq_hazard.valid := valid
  io.seq_hazard.bits.rintent := hazardMultiply(rvs1_mask | rvs2_mask | rvd_mask)
  io.seq_hazard.bits.wintent := hazardMultiply(rvd_mask)
  io.seq_hazard.bits.vat := inst.vat

  val vs1_read_oh = Mux(renv1, UIntToOH(io.rvs1.bits.eg), 0.U)
  val vs2_oh = UIntToOH(io.rvs2.bits.eg)
  val vs2_oh_batched = vs2_oh.asTypeOf(Vec(4, Vec(8, UInt(egsPerVReg.W)))).map { grp =>
    Fill(8, grp(0))
  }.asUInt
  val vs2_read_oh = Mux(renv2, vs2_oh_batched, 0.U)
  val vd_read_oh  = Mux(renvd, UIntToOH(io.rvd.bits.eg), 0.U)
  val vd_write_oh = Mux(inst.wvd, UIntToOH(io.write.bits.eg), 0.U)

  val raw_hazard_acc = vd_read_oh & io.older_writes
  val data_hazard_acc = raw_hazard_acc

  val raw_hazard_data = ((vs1_read_oh | vs2_read_oh) & io.older_writes) =/= 0.U
  val data_hazard_data = raw_hazard_data

  val waw_hazard_wb = (vd_write_oh & io.older_writes) =/= 0.U
  val war_hazard_wb = (vd_write_oh & io.older_reads) =/= 0.U
  val data_hazard_wb = waw_hazard_wb || war_hazard_wb

  val oldest = inst.vat === io.vat_head

  val current_rvs1 = (inst.rs1 << log2Ceil(egsPerVReg)) + data_eg_idx
  val current_rvd = (inst.rd << log2Ceil(egsPerVReg)) + acc_eg_idx + (ci * ((8 * 32) / dLen).U)
  val current_wvd = (inst.rd << log2Ceil(egsPerVReg)) + wb_eg_idx + (ci * ((8 * 32) / dLen).U)

  io.rvs1.valid := valid && renv1 && acc_load_done
  io.rvs1.bits.eg := current_rvs1
  io.rvs2.valid := valid && renv2 && acc_load_done
  io.rvs2.bits.eg := ((inst.rs2 & "h18".U(5.W)) << log2Ceil(egsPerVReg)) + data_eg_idx
  io.rvd.valid := valid && renvd && !data_hazard_acc
  io.rvd.bits.eg := current_rvd
  io.batch_read_vs2 := valid && renv2 && acc_load_done

  io.rvs1.bits.oldest := oldest
  io.rvs2.bits.oldest := oldest
  io.rvd.bits.oldest := oldest

  io.head := head
  io.tail := tail
  io.busy := busy

  io.write.valid := valid && wenvd && !data_hazard_wb
  io.write.bits.eg := current_wvd
  io.write.bits.data := Mux(start_wb, acc_buffer(wb_eg_idx), io.acc_out(dLen - 1, 0))
  io.write.bits.mask := ~(0.U(dLen.W))

  val valid_likely = valid && !data_hazard_data && acc_load_done && !data_load_done
  io.valid_likely := valid_likely
  io.iss.valid := (valid_likely &&
    !(renv1 && !io.rvs1.ready) &&
    !(renv2 && !io.rvs2.ready)
  )
  io.iss.bits.head := data_eg_idx === 0.U
  io.iss.bits.tail_in := data_load_tail
  io.iss.bits.acc_in := acc_buffer.asUInt
  io.iss.bits.altfmt := altfmt
  io.iss.bits.signed := signed
}