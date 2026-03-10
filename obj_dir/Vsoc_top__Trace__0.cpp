// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Tracing implementation internals
#include "verilated_vcd_c.h"
#include "Vsoc_top__Syms.h"


void Vsoc_top___024root__trace_chg_0_sub_0(Vsoc_top___024root* vlSelf, VerilatedVcd::Buffer* bufp);

void Vsoc_top___024root__trace_chg_0(void* voidSelf, VerilatedVcd::Buffer* bufp) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root__trace_chg_0\n"); );
    // Init
    Vsoc_top___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vsoc_top___024root*>(voidSelf);
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    if (VL_UNLIKELY(!vlSymsp->__Vm_activity)) return;
    // Body
    Vsoc_top___024root__trace_chg_0_sub_0((&vlSymsp->TOP), bufp);
}

void Vsoc_top___024root__trace_chg_0_sub_0(Vsoc_top___024root* vlSelf, VerilatedVcd::Buffer* bufp) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root__trace_chg_0_sub_0\n"); );
    // Init
    uint32_t* const oldp VL_ATTR_UNUSED = bufp->oldp(vlSymsp->__Vm_baseCode + 1);
    // Body
    if (VL_UNLIKELY(vlSelf->__Vm_traceActivity[1U])) {
        bufp->chgIData(oldp+0,(vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out),32);
        bufp->chgIData(oldp+1,(vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out),32);
        bufp->chgBit(oldp+2,(vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out));
        bufp->chgBit(oldp+3,(vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out));
        bufp->chgIData(oldp+4,(vlSelf->soc_top__DOT__ram_wdata),32);
        bufp->chgBit(oldp+5,(vlSelf->soc_top__DOT__ram_wr_en));
        bufp->chgIData(oldp+6,(vlSelf->soc_top__DOT__uart_addr),32);
        bufp->chgIData(oldp+7,(vlSelf->soc_top__DOT__uart_wdata),32);
        bufp->chgIData(oldp+8,(vlSelf->soc_top__DOT__uart_rdata),32);
        bufp->chgBit(oldp+9,(vlSelf->soc_top__DOT__uart_rd_en));
        bufp->chgBit(oldp+10,(vlSelf->soc_top__DOT__uart_wr_en));
        bufp->chgBit(oldp+11,(vlSelf->soc_top__DOT__uart_ack));
        bufp->chgBit(oldp+12,(vlSelf->soc_top__DOT__arbiter__DOT__dbus_active));
        bufp->chgBit(oldp+13,(vlSelf->soc_top__DOT__core0__DOT__branch_taken));
        bufp->chgIData(oldp+14,((vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                                 + vlSelf->soc_top__DOT__core0__DOT__ex_pc_in)),32);
        bufp->chgCData(oldp+15,(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in),5);
        bufp->chgIData(oldp+16,(vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback),32);
        bufp->chgBit(oldp+17,(vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in));
        bufp->chgCData(oldp+18,(vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in),5);
        bufp->chgBit(oldp+19,(vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in));
        bufp->chgBit(oldp+20,(vlSelf->soc_top__DOT__core0__DOT__ex_reg_write_in));
        bufp->chgIData(oldp+21,(vlSelf->soc_top__DOT__core0__DOT__ex_pc_plus_4_in),32);
        bufp->chgIData(oldp+22,(vlSelf->soc_top__DOT__core0__DOT__ex_pc_in),32);
        bufp->chgIData(oldp+23,(vlSelf->soc_top__DOT__core0__DOT__ex_read_data1_in),32);
        bufp->chgIData(oldp+24,(vlSelf->soc_top__DOT__core0__DOT__ex_read_data2_in),32);
        bufp->chgIData(oldp+25,(vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in),32);
        bufp->chgIData(oldp+26,(vlSelf->soc_top__DOT__core0__DOT__ex_instruction_in),32);
        bufp->chgCData(oldp+27,(vlSelf->soc_top__DOT__core0__DOT__ex_rs1_addr_in),5);
        bufp->chgCData(oldp+28,(vlSelf->soc_top__DOT__core0__DOT__ex_rs2_addr_in),5);
        bufp->chgBit(oldp+29,(vlSelf->soc_top__DOT__core0__DOT__ex_mem_write_in));
        bufp->chgBit(oldp+30,(vlSelf->soc_top__DOT__core0__DOT__ex_mem_to_reg_in));
        bufp->chgBit(oldp+31,(vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in));
        bufp->chgBit(oldp+32,(vlSelf->soc_top__DOT__core0__DOT__ex_branch_in));
        bufp->chgBit(oldp+33,(vlSelf->soc_top__DOT__core0__DOT__ex_jump_in));
        bufp->chgBit(oldp+34,(vlSelf->soc_top__DOT__core0__DOT__ex_write_from_pc_in));
        bufp->chgCData(oldp+35,(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in),4);
        bufp->chgIData(oldp+36,(((8U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                  ? 0U : ((4U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                           ? ((2U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                               ? 0U
                                               : ((1U 
                                                   & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                                   ? vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b
                                                   : 
                                                  (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                                   ^ vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)))
                                           : ((2U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                               ? ((1U 
                                                   & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                                   ? 
                                                  (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                                   | vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)
                                                   : 
                                                  (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                                   & vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b))
                                               : ((1U 
                                                   & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                                   ? 
                                                  (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                                   - vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)
                                                   : 
                                                  (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                                   + vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)))))),32);
        bufp->chgIData(oldp+37,(vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded),32);
        bufp->chgCData(oldp+38,(vlSelf->soc_top__DOT__core0__DOT__forward_a),2);
        bufp->chgCData(oldp+39,(vlSelf->soc_top__DOT__core0__DOT__forward_b),2);
        bufp->chgIData(oldp+40,(vlSelf->soc_top__DOT__core0__DOT__ma_pc_plus_4_out),32);
        bufp->chgCData(oldp+41,(vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out),5);
        bufp->chgBit(oldp+42,(vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out));
        bufp->chgBit(oldp+43,(vlSelf->soc_top__DOT__core0__DOT__ma_mem_to_reg_out));
        bufp->chgBit(oldp+44,(vlSelf->soc_top__DOT__core0__DOT__ma_write_from_pc_out));
        bufp->chgIData(oldp+45,(vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in),32);
        bufp->chgIData(oldp+46,(vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in),32);
        bufp->chgIData(oldp+47,(vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in),32);
        bufp->chgBit(oldp+48,(vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in));
        bufp->chgBit(oldp+49,(vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in));
        bufp->chgIData(oldp+50,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__i),32);
        bufp->chgIData(oldp+51,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0]),32);
        bufp->chgIData(oldp+52,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[1]),32);
        bufp->chgIData(oldp+53,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[2]),32);
        bufp->chgIData(oldp+54,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[3]),32);
        bufp->chgIData(oldp+55,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[4]),32);
        bufp->chgIData(oldp+56,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[5]),32);
        bufp->chgIData(oldp+57,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[6]),32);
        bufp->chgIData(oldp+58,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[7]),32);
        bufp->chgIData(oldp+59,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[8]),32);
        bufp->chgIData(oldp+60,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[9]),32);
        bufp->chgIData(oldp+61,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[10]),32);
        bufp->chgIData(oldp+62,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[11]),32);
        bufp->chgIData(oldp+63,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[12]),32);
        bufp->chgIData(oldp+64,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[13]),32);
        bufp->chgIData(oldp+65,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[14]),32);
        bufp->chgIData(oldp+66,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[15]),32);
        bufp->chgIData(oldp+67,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[16]),32);
        bufp->chgIData(oldp+68,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[17]),32);
        bufp->chgIData(oldp+69,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[18]),32);
        bufp->chgIData(oldp+70,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[19]),32);
        bufp->chgIData(oldp+71,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[20]),32);
        bufp->chgIData(oldp+72,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[21]),32);
        bufp->chgIData(oldp+73,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[22]),32);
        bufp->chgIData(oldp+74,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[23]),32);
        bufp->chgIData(oldp+75,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[24]),32);
        bufp->chgIData(oldp+76,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[25]),32);
        bufp->chgIData(oldp+77,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[26]),32);
        bufp->chgIData(oldp+78,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[27]),32);
        bufp->chgIData(oldp+79,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[28]),32);
        bufp->chgIData(oldp+80,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[29]),32);
        bufp->chgIData(oldp+81,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[30]),32);
        bufp->chgIData(oldp+82,(vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[31]),32);
        bufp->chgIData(oldp+83,(vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a),32);
        bufp->chgIData(oldp+84,(vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b),32);
        bufp->chgBit(oldp+85,((vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                               == vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded)));
        bufp->chgBit(oldp+86,(vlSelf->soc_top__DOT__uart0__DOT__tx_done));
    }
    if (VL_UNLIKELY(vlSelf->__Vm_traceActivity[2U])) {
        bufp->chgIData(oldp+87,(vlSelf->soc_top__DOT__core0__DOT__id_instruction_in),32);
        bufp->chgIData(oldp+88,(vlSelf->soc_top__DOT__core0__DOT__id_pc_plus_4_in),32);
        bufp->chgIData(oldp+89,(vlSelf->soc_top__DOT__core0__DOT__id_pc_in),32);
        bufp->chgIData(oldp+90,(vlSelf->soc_top__DOT__core0__DOT__id_immediate_out),32);
        bufp->chgCData(oldp+91,((0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                          >> 0xfU))),5);
        bufp->chgCData(oldp+92,((0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                          >> 0x14U))),5);
        bufp->chgCData(oldp+93,((0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                          >> 7U))),5);
        bufp->chgBit(oldp+94,(vlSelf->soc_top__DOT__core0__DOT__id_mem_read_out));
        bufp->chgBit(oldp+95,(vlSelf->soc_top__DOT__core0__DOT__id_mem_write_out));
        bufp->chgBit(oldp+96,(vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out));
        bufp->chgBit(oldp+97,(vlSelf->soc_top__DOT__core0__DOT__id_mem_to_reg_out));
        bufp->chgBit(oldp+98,(vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out));
        bufp->chgBit(oldp+99,(vlSelf->soc_top__DOT__core0__DOT__id_branch_out));
        bufp->chgBit(oldp+100,(vlSelf->soc_top__DOT__core0__DOT__id_jump_out));
        bufp->chgBit(oldp+101,(vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out));
        bufp->chgCData(oldp+102,(vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out),4);
        bufp->chgCData(oldp+103,((0x7fU & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)),7);
        bufp->chgCData(oldp+104,((7U & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                        >> 0xcU))),3);
        bufp->chgCData(oldp+105,((vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                  >> 0x19U)),7);
    }
    if (VL_UNLIKELY(vlSelf->__Vm_traceActivity[3U])) {
        bufp->chgIData(oldp+106,(vlSelf->soc_top__DOT__c0_dbus_rdata),32);
        bufp->chgBit(oldp+107,(vlSelf->soc_top__DOT__c0_dbus_ack));
        bufp->chgBit(oldp+108,(vlSelf->soc_top__DOT__core0__DOT__mem_stall));
    }
    if (VL_UNLIKELY(vlSelf->__Vm_traceActivity[4U])) {
        bufp->chgBit(oldp+109,((1U & (~ (IData)(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in)))));
        bufp->chgIData(oldp+110,(vlSelf->soc_top__DOT__c0_ibus_data),32);
        bufp->chgBit(oldp+111,(vlSelf->soc_top__DOT__c0_ibus_ack));
        bufp->chgIData(oldp+112,(vlSelf->soc_top__DOT__ram_addr),32);
        bufp->chgBit(oldp+113,(vlSelf->soc_top__DOT__ram_rd_en));
        bufp->chgBit(oldp+114,((1U & ((~ (IData)(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in)) 
                                      & (~ (IData)(vlSelf->soc_top__DOT__c0_ibus_ack))))));
        bufp->chgBit(oldp+115,(vlSelf->soc_top__DOT__core0__DOT__pipe_en_all));
        bufp->chgBit(oldp+116,(vlSelf->soc_top__DOT__core0__DOT__if_id_en));
        bufp->chgIData(oldp+117,(vlSelf->soc_top__DOT__core0__DOT__raw_instruction),32);
        bufp->chgIData(oldp+118,(vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction),32);
        bufp->chgBit(oldp+119,((3U != ((IData)(vlSelf->soc_top__DOT__c0_ibus_ack)
                                        ? (3U & vlSelf->soc_top__DOT__c0_ibus_data)
                                        : 3U))));
        bufp->chgBit(oldp+120,(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in));
        bufp->chgBit(oldp+121,((1U & (~ (IData)(vlSelf->soc_top__DOT__core0__DOT__if_id_en)))));
    }
    bufp->chgBit(oldp+122,(vlSelf->clk));
    bufp->chgBit(oldp+123,(vlSelf->rst_n));
    bufp->chgBit(oldp+124,(vlSelf->uart_tx));
    bufp->chgBit(oldp+125,(vlSelf->soc_top__DOT__rst));
    bufp->chgIData(oldp+126,(vlSelf->soc_top__DOT__core0__DOT__curr_pc),32);
    bufp->chgIData(oldp+127,(vlSelf->soc_top__DOT__ram_rdata),32);
    bufp->chgBit(oldp+128,(vlSelf->soc_top__DOT__ram_ack));
    bufp->chgBit(oldp+129,((1U & ((~ (IData)(vlSelf->soc_top__DOT__arbiter__DOT__dbus_active)) 
                                  & (~ (IData)(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in))))));
    bufp->chgIData(oldp+130,(((IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken)
                               ? (vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                                  + vlSelf->soc_top__DOT__core0__DOT__ex_pc_in)
                               : ((IData)(vlSelf->soc_top__DOT__core0__DOT__if_id_en)
                                   ? ((IData)(4U) + vlSelf->soc_top__DOT__core0__DOT__curr_pc)
                                   : vlSelf->soc_top__DOT__core0__DOT__curr_pc))),32);
    bufp->chgIData(oldp+131,(((IData)(4U) + vlSelf->soc_top__DOT__core0__DOT__curr_pc)),32);
    bufp->chgBit(oldp+132,(vlSelf->soc_top__DOT__core0__DOT__hazard_stall));
    bufp->chgBit(oldp+133,(vlSelf->soc_top__DOT__core0__DOT__if_id_clr));
    bufp->chgBit(oldp+134,(vlSelf->soc_top__DOT__core0__DOT__id_ex_clr));
    bufp->chgIData(oldp+135,(((0U == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                               >> 0xfU)))
                               ? 0U : vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers
                              [(0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                         >> 0xfU))])),32);
    bufp->chgIData(oldp+136,(((0U == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                               >> 0x14U)))
                               ? 0U : vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers
                              [(0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                         >> 0x14U))])),32);
    bufp->chgIData(oldp+137,((((IData)(vlSelf->soc_top__DOT__c0_dbus_ack) 
                               & (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out))
                               ? vlSelf->soc_top__DOT__c0_dbus_rdata
                               : 0U)),32);
}

void Vsoc_top___024root__trace_cleanup(void* voidSelf, VerilatedVcd* /*unused*/) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root__trace_cleanup\n"); );
    // Init
    Vsoc_top___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vsoc_top___024root*>(voidSelf);
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    // Body
    vlSymsp->__Vm_activity = false;
    vlSymsp->TOP.__Vm_traceActivity[0U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[1U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[2U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[3U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[4U] = 0U;
}
