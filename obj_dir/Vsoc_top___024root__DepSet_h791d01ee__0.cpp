// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vsoc_top.h for the primary calling header

#include "Vsoc_top__pch.h"
#include "Vsoc_top___024root.h"

VL_INLINE_OPT void Vsoc_top___024root___ico_sequent__TOP__0(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___ico_sequent__TOP__0\n"); );
    // Body
    vlSelf->soc_top__DOT__rst = (1U & (~ (IData)(vlSelf->rst_n)));
    vlSelf->soc_top__DOT__core0__DOT__if_id_clr = ((IData)(vlSelf->soc_top__DOT__rst) 
                                                   | (IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken));
    vlSelf->soc_top__DOT__core0__DOT__id_ex_clr = ((IData)(vlSelf->soc_top__DOT__core0__DOT__if_id_clr) 
                                                   | ((IData)(vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) 
                                                      & (IData)(vlSelf->soc_top__DOT__core0__DOT__hazard_stall)));
}

void Vsoc_top___024root___eval_ico(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_ico\n"); );
    // Body
    if ((1ULL & vlSelf->__VicoTriggered.word(0U))) {
        Vsoc_top___024root___ico_sequent__TOP__0(vlSelf);
    }
}

void Vsoc_top___024root___eval_triggers__ico(Vsoc_top___024root* vlSelf);

bool Vsoc_top___024root___eval_phase__ico(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_phase__ico\n"); );
    // Init
    CData/*0:0*/ __VicoExecute;
    // Body
    Vsoc_top___024root___eval_triggers__ico(vlSelf);
    __VicoExecute = vlSelf->__VicoTriggered.any();
    if (__VicoExecute) {
        Vsoc_top___024root___eval_ico(vlSelf);
    }
    return (__VicoExecute);
}

void Vsoc_top___024root___eval_act(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_act\n"); );
}

VL_INLINE_OPT void Vsoc_top___024root___nba_sequent__TOP__0(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_sequent__TOP__0\n"); );
    // Init
    SData/*13:0*/ __Vdlyvdim0__soc_top__DOT__ram0__DOT__mem__v0;
    __Vdlyvdim0__soc_top__DOT__ram0__DOT__mem__v0 = 0;
    IData/*31:0*/ __Vdlyvval__soc_top__DOT__ram0__DOT__mem__v0;
    __Vdlyvval__soc_top__DOT__ram0__DOT__mem__v0 = 0;
    CData/*0:0*/ __Vdlyvset__soc_top__DOT__ram0__DOT__mem__v0;
    __Vdlyvset__soc_top__DOT__ram0__DOT__mem__v0 = 0;
    // Body
    __Vdlyvset__soc_top__DOT__ram0__DOT__mem__v0 = 0U;
    if (VL_UNLIKELY(((~ (IData)(vlSelf->soc_top__DOT__rst)) 
                     & (IData)(vlSelf->soc_top__DOT__core0__DOT__pipe_en_all)))) {
        VL_WRITEF("T=%0t | PC=0x%x | Instr=0x%x\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->soc_top__DOT__core0__DOT__id_pc_in,
                  32,vlSelf->soc_top__DOT__core0__DOT__id_instruction_in);
    }
    if ((1U & (~ (IData)(vlSelf->soc_top__DOT__rst)))) {
        if (vlSelf->soc_top__DOT__ram_wr_en) {
            __Vdlyvval__soc_top__DOT__ram0__DOT__mem__v0 
                = vlSelf->soc_top__DOT__ram_wdata;
            __Vdlyvset__soc_top__DOT__ram0__DOT__mem__v0 = 1U;
            __Vdlyvdim0__soc_top__DOT__ram0__DOT__mem__v0 
                = (0x3fffU & (vlSelf->soc_top__DOT__ram_addr 
                              >> 2U));
        }
    }
    if (vlSelf->soc_top__DOT__rst) {
        vlSelf->soc_top__DOT__ram_rdata = 0U;
        vlSelf->soc_top__DOT__ram_ack = 0U;
    } else {
        if ((1U & (~ (IData)(vlSelf->soc_top__DOT__ram_wr_en)))) {
            if (vlSelf->soc_top__DOT__ram_rd_en) {
                vlSelf->soc_top__DOT__ram_rdata = vlSelf->soc_top__DOT__ram0__DOT__mem
                    [(0x3fffU & (vlSelf->soc_top__DOT__ram_addr 
                                 >> 2U))];
            }
        }
        vlSelf->soc_top__DOT__ram_ack = 0U;
        if (vlSelf->soc_top__DOT__ram_wr_en) {
            vlSelf->soc_top__DOT__ram_ack = 1U;
        } else if (vlSelf->soc_top__DOT__ram_rd_en) {
            vlSelf->soc_top__DOT__ram_ack = 1U;
        }
    }
    if (__Vdlyvset__soc_top__DOT__ram0__DOT__mem__v0) {
        vlSelf->soc_top__DOT__ram0__DOT__mem[__Vdlyvdim0__soc_top__DOT__ram0__DOT__mem__v0] 
            = __Vdlyvval__soc_top__DOT__ram0__DOT__mem__v0;
    }
}

VL_INLINE_OPT void Vsoc_top___024root___nba_sequent__TOP__1(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_sequent__TOP__1\n"); );
    // Init
    CData/*0:0*/ __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v0;
    __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v0 = 0;
    CData/*4:0*/ __Vdlyvdim0__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32;
    __Vdlyvdim0__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 = 0;
    IData/*31:0*/ __Vdlyvval__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32;
    __Vdlyvval__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 = 0;
    CData/*0:0*/ __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32;
    __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 = 0;
    CData/*0:0*/ __Vdly__soc_top__DOT__uart0__DOT__tx_done;
    __Vdly__soc_top__DOT__uart0__DOT__tx_done = 0;
    // Body
    __Vdly__soc_top__DOT__uart0__DOT__tx_done = vlSelf->soc_top__DOT__uart0__DOT__tx_done;
    __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v0 = 0U;
    __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 = 0U;
    if (vlSelf->soc_top__DOT__rst) {
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__i = 0x20U;
        __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v0 = 1U;
        vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out = 0U;
    } else {
        if (((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in) 
             & (0U != (IData)(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in)))) {
            __Vdlyvval__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 
                = vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback;
            __Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 = 1U;
            __Vdlyvdim0__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32 
                = vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in;
        }
        if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
            vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out 
                = vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded;
        }
    }
    if (((IData)(vlSelf->soc_top__DOT__rst) | (IData)(vlSelf->soc_top__DOT__core0__DOT__id_ex_clr))) {
        vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_branch_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_jump_in = 0U;
    } else if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
        vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_branch_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_branch_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_jump_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_jump_out;
    }
    if ((1U & (~ ((IData)(vlSelf->soc_top__DOT__rst) 
                  | (IData)(vlSelf->soc_top__DOT__core0__DOT__id_ex_clr))))) {
        if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
            vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                = vlSelf->soc_top__DOT__core0__DOT__id_immediate_out;
        }
    }
    if (vlSelf->soc_top__DOT__rst) {
        vlSelf->soc_top__DOT__uart_ack = 0U;
        vlSelf->soc_top__DOT__uart_rdata = 0U;
        vlSelf->uart_tx = 1U;
        __Vdly__soc_top__DOT__uart0__DOT__tx_done = 0U;
    } else if (vlSelf->soc_top__DOT__uart_wr_en) {
        if ((1U & (~ (IData)(vlSelf->soc_top__DOT__uart0__DOT__tx_done)))) {
            if (VL_UNLIKELY((0U == (0xffU & vlSelf->soc_top__DOT__uart_addr)))) {
                VL_WRITEF("%c",8,(0xffU & vlSelf->soc_top__DOT__uart_wdata));
                Verilated::runFlushCallbacks();
            }
            __Vdly__soc_top__DOT__uart0__DOT__tx_done = 1U;
        }
        vlSelf->soc_top__DOT__uart_ack = 1U;
    } else if (vlSelf->soc_top__DOT__uart_rd_en) {
        vlSelf->soc_top__DOT__uart_ack = 1U;
        vlSelf->soc_top__DOT__uart_rdata = ((4U == 
                                             (0xffU 
                                              & vlSelf->soc_top__DOT__uart_addr))
                                             ? 1U : 0U);
    } else {
        vlSelf->soc_top__DOT__uart_ack = 0U;
        __Vdly__soc_top__DOT__uart0__DOT__tx_done = 0U;
    }
    if (vlSelf->soc_top__DOT__rst) {
        vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_write_from_pc_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_to_reg_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_pc_plus_4_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out = 0U;
    } else if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
        vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_write_from_pc_out;
        vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_mem_to_reg_out;
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_mem_write_in;
        vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_pc_plus_4_out;
        vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out;
        vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in 
            = (((IData)(vlSelf->soc_top__DOT__c0_dbus_ack) 
                & (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out))
                ? vlSelf->soc_top__DOT__c0_dbus_rdata
                : 0U);
        vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out;
        vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in 
            = vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out;
        vlSelf->soc_top__DOT__core0__DOT__ma_write_from_pc_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_write_from_pc_in;
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_to_reg_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_mem_to_reg_in;
        vlSelf->soc_top__DOT__core0__DOT__ma_pc_plus_4_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_pc_plus_4_in;
        vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
            = ((8U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                ? 0U : ((4U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                         ? ((2U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                             ? 0U : ((1U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                      ? vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b
                                      : (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                         ^ vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)))
                         : ((2U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                             ? ((1U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                 ? (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                    | vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)
                                 : (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                    & vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b))
                             : ((1U & (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in))
                                 ? (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                    - vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)
                                 : (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                                    + vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b)))));
        vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in;
        vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_reg_write_in;
        vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out 
            = vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in;
    }
    if ((1U & (~ ((IData)(vlSelf->soc_top__DOT__rst) 
                  | (IData)(vlSelf->soc_top__DOT__core0__DOT__id_ex_clr))))) {
        if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
            vlSelf->soc_top__DOT__core0__DOT__ex_read_data1_in 
                = ((0U == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                    >> 0xfU))) ? 0U
                    : vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers
                   [(0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 0xfU))]);
            vlSelf->soc_top__DOT__core0__DOT__ex_rs1_addr_in 
                = (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                            >> 0xfU));
            vlSelf->soc_top__DOT__core0__DOT__ex_read_data2_in 
                = ((0U == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                    >> 0x14U))) ? 0U
                    : vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers
                   [(0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 0x14U))]);
            vlSelf->soc_top__DOT__core0__DOT__ex_rs2_addr_in 
                = (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                            >> 0x14U));
            vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in 
                = vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out;
            vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in 
                = (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                            >> 7U));
        }
    }
    if (((IData)(vlSelf->soc_top__DOT__rst) | (IData)(vlSelf->soc_top__DOT__core0__DOT__id_ex_clr))) {
        vlSelf->soc_top__DOT__core0__DOT__ex_pc_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_instruction_in = 0x13U;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_write_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_write_from_pc_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_to_reg_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_pc_plus_4_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__ex_reg_write_in = 0U;
    } else if (vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) {
        vlSelf->soc_top__DOT__core0__DOT__ex_pc_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_pc_in;
        vlSelf->soc_top__DOT__core0__DOT__ex_instruction_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_instruction_in;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_write_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_mem_write_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_write_from_pc_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_to_reg_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_mem_to_reg_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_pc_plus_4_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_pc_plus_4_in;
        vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_mem_read_out;
        vlSelf->soc_top__DOT__core0__DOT__ex_reg_write_in 
            = vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out;
    }
    vlSelf->soc_top__DOT__uart0__DOT__tx_done = __Vdly__soc_top__DOT__uart0__DOT__tx_done;
    if (__Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v0) {
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[1U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[2U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[3U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[4U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[5U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[6U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[7U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[8U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[9U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xaU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xbU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xcU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xdU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xeU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0xfU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x10U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x11U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x12U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x13U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x14U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x15U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x16U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x17U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x18U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x19U] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1aU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1bU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1cU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1dU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1eU] = 0U;
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[0x1fU] = 0U;
    }
    if (__Vdlyvset__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32) {
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[__Vdlyvdim0__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32] 
            = __Vdlyvval__soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers__v32;
    }
    vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in)
            ? vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in
            : ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in)
                ? vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in
                : vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in));
    vlSelf->soc_top__DOT__uart_addr = 0U;
    vlSelf->soc_top__DOT__uart_wr_en = 0U;
    vlSelf->soc_top__DOT__uart_wdata = 0U;
    vlSelf->soc_top__DOT__ram_wr_en = 0U;
    vlSelf->soc_top__DOT__ram_wdata = 0U;
    if ((8U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        vlSelf->soc_top__DOT__ram_wr_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out;
        vlSelf->soc_top__DOT__ram_wdata = vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out;
    }
    vlSelf->soc_top__DOT__uart_rd_en = 0U;
    if ((8U != (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        if ((1U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                    >> 0x1cU))) {
            vlSelf->soc_top__DOT__uart_addr = vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out;
            vlSelf->soc_top__DOT__uart_wr_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out;
            vlSelf->soc_top__DOT__uart_wdata = vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out;
            vlSelf->soc_top__DOT__uart_rd_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out;
        }
    }
    vlSelf->soc_top__DOT__arbiter__DOT__dbus_active 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out) 
           | (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out));
    vlSelf->soc_top__DOT__core0__DOT__forward_a = 0U;
    if ((((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out) 
          & (0U != (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out))) 
         & ((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out) 
            == (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rs1_addr_in)))) {
        vlSelf->soc_top__DOT__core0__DOT__forward_a = 2U;
    } else if ((((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in) 
                 & (0U != (IData)(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in))) 
                & ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in) 
                   == (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rs1_addr_in)))) {
        vlSelf->soc_top__DOT__core0__DOT__forward_a = 1U;
    }
    vlSelf->soc_top__DOT__core0__DOT__forward_b = 0U;
    if ((((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out) 
          & (0U != (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out))) 
         & ((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out) 
            == (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rs2_addr_in)))) {
        vlSelf->soc_top__DOT__core0__DOT__forward_b = 2U;
    } else if ((((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in) 
                 & (0U != (IData)(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in))) 
                & ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in) 
                   == (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rs2_addr_in)))) {
        vlSelf->soc_top__DOT__core0__DOT__forward_b = 1U;
    }
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
        = ((0U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_a))
            ? vlSelf->soc_top__DOT__core0__DOT__ex_read_data1_in
            : ((1U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_a))
                ? vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback
                : ((2U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_a))
                    ? vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out
                    : vlSelf->soc_top__DOT__core0__DOT__ex_read_data1_in)));
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded 
        = ((0U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_b))
            ? vlSelf->soc_top__DOT__core0__DOT__ex_read_data2_in
            : ((1U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_b))
                ? vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback
                : ((2U == (IData)(vlSelf->soc_top__DOT__core0__DOT__forward_b))
                    ? vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out
                    : vlSelf->soc_top__DOT__core0__DOT__ex_read_data2_in)));
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in)
            ? ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in)
                ? vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in
                : vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded)
            : vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded);
    vlSelf->soc_top__DOT__core0__DOT__branch_taken 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_jump_in) 
           | ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_branch_in) 
              & (vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a 
                 == vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded)));
    if (VL_UNLIKELY(vlSelf->soc_top__DOT__core0__DOT__branch_taken)) {
        VL_WRITEF("BRANCH/JUMP TAKEN: PC=0x%x, Imm=0x%x, Target=0x%x\n",
                  32,vlSelf->soc_top__DOT__core0__DOT__ex_pc_in,
                  32,vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in,
                  32,(vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                      + vlSelf->soc_top__DOT__core0__DOT__ex_pc_in));
    }
    if (VL_UNLIKELY(((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_branch_in) 
                     | (IData)(vlSelf->soc_top__DOT__core0__DOT__ex_jump_in)))) {
        VL_WRITEF("EX Stage: PC=0x%x, Branch=%b, Jump=%b, Taken=%b, Target=0x%x\n",
                  32,vlSelf->soc_top__DOT__core0__DOT__ex_pc_in,
                  1,(IData)(vlSelf->soc_top__DOT__core0__DOT__ex_branch_in),
                  1,vlSelf->soc_top__DOT__core0__DOT__ex_jump_in,
                  1,(IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken),
                  32,(vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                      + vlSelf->soc_top__DOT__core0__DOT__ex_pc_in));
    }
}

VL_INLINE_OPT void Vsoc_top___024root___nba_sequent__TOP__2(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_sequent__TOP__2\n"); );
    // Body
    if (vlSelf->soc_top__DOT__core0__DOT__if_id_clr) {
        vlSelf->soc_top__DOT__core0__DOT__id_pc_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__id_pc_plus_4_in = 0U;
        vlSelf->soc_top__DOT__core0__DOT__id_instruction_in = 0x13U;
    } else if (vlSelf->soc_top__DOT__core0__DOT__if_id_en) {
        vlSelf->soc_top__DOT__core0__DOT__id_pc_in 
            = vlSelf->soc_top__DOT__core0__DOT__curr_pc;
        vlSelf->soc_top__DOT__core0__DOT__id_pc_plus_4_in 
            = ((IData)(4U) + vlSelf->soc_top__DOT__core0__DOT__curr_pc);
        vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
            = vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction;
    }
    vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_mem_to_reg_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_mem_read_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_mem_write_out = 0U;
    if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                  >> 6U)))) {
        if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                      >> 5U)))) {
            if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                          >> 4U)))) {
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 3U)))) {
                    if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                  >> 2U)))) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_mem_to_reg_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_mem_read_out = 1U;
                            }
                        }
                    }
                }
            }
        }
        if ((0x20U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
            if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                          >> 4U)))) {
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 3U)))) {
                    if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                  >> 2U)))) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_mem_write_out = 1U;
                            }
                        }
                    }
                }
            }
        }
    }
    vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_branch_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out = 0U;
    vlSelf->soc_top__DOT__core0__DOT__id_jump_out = 0U;
    if ((0x40U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
        if ((0x20U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
            if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                          >> 4U)))) {
                if ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                    if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_jump_out = 1U;
                            }
                        }
                    }
                } else if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                    if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_jump_out = 1U;
                        }
                    }
                }
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 3U)))) {
                    if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                            }
                        }
                    }
                    if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                  >> 2U)))) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_branch_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 1U;
                            }
                        }
                    }
                }
            }
        }
    } else {
        if ((0x20U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
            if ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 3U)))) {
                    if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                                vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 5U;
                            }
                        }
                    } else if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out 
                                = ((0x4000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                    ? ((0x2000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                        ? ((0x1000U 
                                            & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                            ? 2U : 3U)
                                        : ((0x1000U 
                                            & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                            ? ((0x40000000U 
                                                & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                ? 0xaU
                                                : 9U)
                                            : 4U)) : 
                                   ((0x2000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                     ? ((0x1000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                         ? 8U : 7U)
                                     : ((0x1000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                         ? 6U : ((0x40000000U 
                                                  & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                  ? 1U
                                                  : 0U))));
                        }
                    }
                }
            } else if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                 >> 3U)))) {
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 2U)))) {
                    if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 0U;
                        }
                    }
                }
            }
        } else if ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
            if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                          >> 3U)))) {
                if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                    if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                            vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 0U;
                        }
                    }
                } else if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                    if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                        vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                        vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out 
                            = ((0x4000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                ? ((0x2000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                    ? ((0x1000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                        ? 2U : 3U) : 
                                   ((0x1000U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                     ? ((0x40000000U 
                                         & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                         ? 0xaU : 9U)
                                     : 4U)) : ((0x2000U 
                                                & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                ? (
                                                   (0x1000U 
                                                    & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                    ? 8U
                                                    : 7U)
                                                : (
                                                   (0x1000U 
                                                    & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                    ? 6U
                                                    : 0U)));
                    }
                }
            }
        } else if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                             >> 3U)))) {
            if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                          >> 2U)))) {
                if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                    if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = 1U;
                        vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = 1U;
                        vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = 0U;
                    }
                }
            }
        }
        if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                      >> 5U)))) {
            if ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                if ((1U & (~ (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                              >> 3U)))) {
                    if ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                        if ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                            if ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)) {
                                vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out = 1U;
                            }
                        }
                    }
                }
            }
        }
    }
    if (VL_UNLIKELY((0x6fU == (0x7fU & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)))) {
        VL_WRITEF("DECODE JAL: Opcode=0x%x, Jump=%b\n",
                  7,(0x7fU & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in),
                  1,(IData)(vlSelf->soc_top__DOT__core0__DOT__id_jump_out));
    }
    vlSelf->soc_top__DOT__core0__DOT__id_immediate_out 
        = ((0x40U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
            ? ((0x20U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                ? ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                    ? 0U : ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                             ? ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                 ? ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                     ? ((1U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                         ? (((- (IData)(
                                                        (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                         >> 0x1fU))) 
                                             << 0x15U) 
                                            | ((0x100000U 
                                                & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                   >> 0xbU)) 
                                               | ((0xff000U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in) 
                                                  | ((0x800U 
                                                      & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                         >> 9U)) 
                                                     | (0x7feU 
                                                        & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                           >> 0x14U))))))
                                         : 0U) : 0U)
                                 : 0U) : ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                           ? ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                               ? ((1U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   ? 
                                                  (((- (IData)(
                                                               (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                >> 0x1fU))) 
                                                    << 0xcU) 
                                                   | (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                      >> 0x14U))
                                                   : 0U)
                                               : 0U)
                                           : ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                               ? ((1U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   ? 
                                                  (((- (IData)(
                                                               (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                >> 0x1fU))) 
                                                    << 0xdU) 
                                                   | ((0x1000U 
                                                       & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                          >> 0x13U)) 
                                                      | ((0x800U 
                                                          & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                             << 4U)) 
                                                         | ((0x7e0U 
                                                             & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                >> 0x14U)) 
                                                            | (0x1eU 
                                                               & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                  >> 7U))))))
                                                   : 0U)
                                               : 0U))))
                : 0U) : ((0x20U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                          ? ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                              ? ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                  ? 0U : ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                           ? ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                               ? ((1U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   ? 
                                                  (0xfffff000U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   : 0U)
                                               : 0U)
                                           : 0U)) : 
                             ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                               ? 0U : ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                        ? 0U : ((2U 
                                                 & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                 ? 
                                                ((1U 
                                                  & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                  ? 
                                                 (((- (IData)(
                                                              (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                               >> 0x1fU))) 
                                                   << 0xcU) 
                                                  | ((0xfe0U 
                                                      & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                         >> 0x14U)) 
                                                     | (0x1fU 
                                                        & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                           >> 7U))))
                                                  : 0U)
                                                 : 0U))))
                          : ((0x10U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                              ? ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                  ? 0U : ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                           ? ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                               ? ((1U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   ? 
                                                  (0xfffff000U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   : 0U)
                                               : 0U)
                                           : ((2U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                               ? ((1U 
                                                   & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                   ? 
                                                  (((- (IData)(
                                                               (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                >> 0x1fU))) 
                                                    << 0xcU) 
                                                   | (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                      >> 0x14U))
                                                   : 0U)
                                               : 0U)))
                              : ((8U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                  ? 0U : ((4U & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                           ? 0U : (
                                                   (2U 
                                                    & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                    ? 
                                                   ((1U 
                                                     & vlSelf->soc_top__DOT__core0__DOT__id_instruction_in)
                                                     ? 
                                                    (((- (IData)(
                                                                 (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                                  >> 0x1fU))) 
                                                      << 0xcU) 
                                                     | (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                        >> 0x14U))
                                                     : 0U)
                                                    : 0U))))));
}

VL_INLINE_OPT void Vsoc_top___024root___nba_comb__TOP__0(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_comb__TOP__0\n"); );
    // Body
    vlSelf->soc_top__DOT__c0_dbus_rdata = 0U;
    vlSelf->soc_top__DOT__c0_dbus_ack = 0U;
    if ((8U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        vlSelf->soc_top__DOT__c0_dbus_rdata = vlSelf->soc_top__DOT__ram_rdata;
        vlSelf->soc_top__DOT__c0_dbus_ack = vlSelf->soc_top__DOT__ram_ack;
    } else if ((1U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                       >> 0x1cU))) {
        vlSelf->soc_top__DOT__c0_dbus_rdata = vlSelf->soc_top__DOT__uart_rdata;
        vlSelf->soc_top__DOT__c0_dbus_ack = vlSelf->soc_top__DOT__uart_ack;
    }
    vlSelf->soc_top__DOT__core0__DOT__mem_stall = (
                                                   (~ (IData)(vlSelf->soc_top__DOT__c0_dbus_ack)) 
                                                   & (IData)(vlSelf->soc_top__DOT__arbiter__DOT__dbus_active));
}

VL_INLINE_OPT void Vsoc_top___024root___nba_sequent__TOP__3(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_sequent__TOP__3\n"); );
    // Body
    vlSelf->soc_top__DOT__core0__DOT__curr_pc = ((IData)(vlSelf->soc_top__DOT__rst)
                                                  ? 0x80000000U
                                                  : vlSelf->soc_top__DOT__core0__DOT__next_pc);
}

VL_INLINE_OPT void Vsoc_top___024root___nba_sequent__TOP__4(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_sequent__TOP__4\n"); );
    // Body
    vlSelf->soc_top__DOT__core0__DOT__if_id_clr = ((IData)(vlSelf->soc_top__DOT__rst) 
                                                   | (IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken));
}

VL_INLINE_OPT void Vsoc_top___024root___nba_comb__TOP__1(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_comb__TOP__1\n"); );
    // Body
    vlSelf->soc_top__DOT__core0__DOT__hazard_stall 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in) 
           & (((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in) 
               == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                            >> 0xfU))) | ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in) 
                                          == (0x1fU 
                                              & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                 >> 0x14U)))));
}

VL_INLINE_OPT void Vsoc_top___024root___nba_comb__TOP__2(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___nba_comb__TOP__2\n"); );
    // Body
    vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__hazard_stall) 
           | (IData)(vlSelf->soc_top__DOT__core0__DOT__mem_stall));
    vlSelf->soc_top__DOT__ram_addr = 0U;
    if ((8U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        vlSelf->soc_top__DOT__ram_addr = vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out;
        vlSelf->soc_top__DOT__ram_rd_en = 0U;
        vlSelf->soc_top__DOT__ram_rd_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out;
    } else {
        vlSelf->soc_top__DOT__ram_rd_en = 0U;
    }
    vlSelf->soc_top__DOT__c0_ibus_data = 0U;
    vlSelf->soc_top__DOT__c0_ibus_ack = 0U;
    if ((1U & (~ (IData)(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in)))) {
        if ((8U == (vlSelf->soc_top__DOT__core0__DOT__curr_pc 
                    >> 0x1cU))) {
            if ((1U & ((8U != (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                               >> 0x1cU)) | (~ (IData)(vlSelf->soc_top__DOT__arbiter__DOT__dbus_active))))) {
                vlSelf->soc_top__DOT__ram_addr = vlSelf->soc_top__DOT__core0__DOT__curr_pc;
                vlSelf->soc_top__DOT__ram_rd_en = 1U;
                vlSelf->soc_top__DOT__c0_ibus_data 
                    = vlSelf->soc_top__DOT__ram_rdata;
                vlSelf->soc_top__DOT__c0_ibus_ack = vlSelf->soc_top__DOT__ram_ack;
            }
        }
    }
    vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
        = ((IData)(vlSelf->soc_top__DOT__c0_ibus_ack)
            ? vlSelf->soc_top__DOT__c0_ibus_data : 0x13U);
    vlSelf->soc_top__DOT__core0__DOT__pipe_en_all = 
        (1U & (~ (((~ (IData)(vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in)) 
                   & (~ (IData)(vlSelf->soc_top__DOT__c0_ibus_ack))) 
                  | (IData)(vlSelf->soc_top__DOT__core0__DOT__mem_stall))));
    if ((3U != ((IData)(vlSelf->soc_top__DOT__c0_ibus_ack)
                 ? (3U & vlSelf->soc_top__DOT__c0_ibus_data)
                 : 3U))) {
        vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction = 0x13U;
        if ((0U == (3U & vlSelf->soc_top__DOT__core0__DOT__raw_instruction))) {
            if ((2U == (7U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                              >> 0xdU)))) {
                vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction 
                    = (0x82403U | ((0x8000000U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                  << 0x16U)) 
                                   | ((0x7000000U & 
                                       (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                        << 0xeU)) | 
                                      ((0x800000U & 
                                        (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                         << 0x11U)) 
                                       | ((0x38000U 
                                           & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                              << 8U)) 
                                          | (0x380U 
                                             & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                << 5U)))))));
            }
        } else if ((1U == (3U & vlSelf->soc_top__DOT__core0__DOT__raw_instruction))) {
            if ((0U == (7U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                              >> 0xdU)))) {
                vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction 
                    = (0x13U | (((- (IData)((1U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                   >> 0xcU)))) 
                                 << 0x1aU) | ((0x2000000U 
                                               & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                  << 0xdU)) 
                                              | ((0x1f00000U 
                                                  & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                     << 0x12U)) 
                                                 | ((0xf8000U 
                                                     & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                        << 8U)) 
                                                    | (0xf80U 
                                                       & vlSelf->soc_top__DOT__core0__DOT__raw_instruction))))));
            }
        } else if ((2U == (3U & vlSelf->soc_top__DOT__core0__DOT__raw_instruction))) {
            if ((0U == (7U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                              >> 0xdU)))) {
                vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction 
                    = (0x1013U | ((0x1f00000U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                 << 0x12U)) 
                                  | ((0xf8000U & (vlSelf->soc_top__DOT__core0__DOT__raw_instruction 
                                                  << 8U)) 
                                     | (0xf80U & vlSelf->soc_top__DOT__core0__DOT__raw_instruction))));
            }
        }
    } else {
        vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction 
            = vlSelf->soc_top__DOT__core0__DOT__raw_instruction;
    }
    vlSelf->soc_top__DOT__core0__DOT__id_ex_clr = ((IData)(vlSelf->soc_top__DOT__core0__DOT__if_id_clr) 
                                                   | ((IData)(vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) 
                                                      & (IData)(vlSelf->soc_top__DOT__core0__DOT__hazard_stall)));
    vlSelf->soc_top__DOT__core0__DOT__if_id_en = ((IData)(vlSelf->soc_top__DOT__core0__DOT__pipe_en_all) 
                                                  & (~ (IData)(vlSelf->soc_top__DOT__core0__DOT__hazard_stall)));
    vlSelf->soc_top__DOT__core0__DOT__next_pc = ((IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken)
                                                  ? 
                                                 (vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in 
                                                  + vlSelf->soc_top__DOT__core0__DOT__ex_pc_in)
                                                  : 
                                                 ((IData)(vlSelf->soc_top__DOT__core0__DOT__if_id_en)
                                                   ? 
                                                  ((IData)(4U) 
                                                   + vlSelf->soc_top__DOT__core0__DOT__curr_pc)
                                                   : vlSelf->soc_top__DOT__core0__DOT__curr_pc));
}

void Vsoc_top___024root___eval_nba(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_nba\n"); );
    // Body
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_sequent__TOP__0(vlSelf);
    }
    if ((4ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_sequent__TOP__1(vlSelf);
        vlSelf->__Vm_traceActivity[1U] = 1U;
    }
    if ((2ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_sequent__TOP__2(vlSelf);
        vlSelf->__Vm_traceActivity[2U] = 1U;
    }
    if ((5ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_comb__TOP__0(vlSelf);
        vlSelf->__Vm_traceActivity[3U] = 1U;
    }
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_sequent__TOP__3(vlSelf);
    }
    if ((4ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_sequent__TOP__4(vlSelf);
    }
    if ((6ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_comb__TOP__1(vlSelf);
    }
    if ((7ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vsoc_top___024root___nba_comb__TOP__2(vlSelf);
        vlSelf->__Vm_traceActivity[4U] = 1U;
    }
}

void Vsoc_top___024root___eval_triggers__act(Vsoc_top___024root* vlSelf);

bool Vsoc_top___024root___eval_phase__act(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_phase__act\n"); );
    // Init
    VlTriggerVec<3> __VpreTriggered;
    CData/*0:0*/ __VactExecute;
    // Body
    Vsoc_top___024root___eval_triggers__act(vlSelf);
    __VactExecute = vlSelf->__VactTriggered.any();
    if (__VactExecute) {
        __VpreTriggered.andNot(vlSelf->__VactTriggered, vlSelf->__VnbaTriggered);
        vlSelf->__VnbaTriggered.thisOr(vlSelf->__VactTriggered);
        Vsoc_top___024root___eval_act(vlSelf);
    }
    return (__VactExecute);
}

bool Vsoc_top___024root___eval_phase__nba(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_phase__nba\n"); );
    // Init
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = vlSelf->__VnbaTriggered.any();
    if (__VnbaExecute) {
        Vsoc_top___024root___eval_nba(vlSelf);
        vlSelf->__VnbaTriggered.clear();
    }
    return (__VnbaExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__ico(Vsoc_top___024root* vlSelf);
#endif  // VL_DEBUG
#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__nba(Vsoc_top___024root* vlSelf);
#endif  // VL_DEBUG
#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__act(Vsoc_top___024root* vlSelf);
#endif  // VL_DEBUG

void Vsoc_top___024root___eval(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval\n"); );
    // Init
    IData/*31:0*/ __VicoIterCount;
    CData/*0:0*/ __VicoContinue;
    IData/*31:0*/ __VnbaIterCount;
    CData/*0:0*/ __VnbaContinue;
    // Body
    __VicoIterCount = 0U;
    vlSelf->__VicoFirstIteration = 1U;
    __VicoContinue = 1U;
    while (__VicoContinue) {
        if (VL_UNLIKELY((0x64U < __VicoIterCount))) {
#ifdef VL_DEBUG
            Vsoc_top___024root___dump_triggers__ico(vlSelf);
#endif
            VL_FATAL_MT("hardware/rtl/soc_top.v", 6, "", "Input combinational region did not converge.");
        }
        __VicoIterCount = ((IData)(1U) + __VicoIterCount);
        __VicoContinue = 0U;
        if (Vsoc_top___024root___eval_phase__ico(vlSelf)) {
            __VicoContinue = 1U;
        }
        vlSelf->__VicoFirstIteration = 0U;
    }
    __VnbaIterCount = 0U;
    __VnbaContinue = 1U;
    while (__VnbaContinue) {
        if (VL_UNLIKELY((0x64U < __VnbaIterCount))) {
#ifdef VL_DEBUG
            Vsoc_top___024root___dump_triggers__nba(vlSelf);
#endif
            VL_FATAL_MT("hardware/rtl/soc_top.v", 6, "", "NBA region did not converge.");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        __VnbaContinue = 0U;
        vlSelf->__VactIterCount = 0U;
        vlSelf->__VactContinue = 1U;
        while (vlSelf->__VactContinue) {
            if (VL_UNLIKELY((0x64U < vlSelf->__VactIterCount))) {
#ifdef VL_DEBUG
                Vsoc_top___024root___dump_triggers__act(vlSelf);
#endif
                VL_FATAL_MT("hardware/rtl/soc_top.v", 6, "", "Active region did not converge.");
            }
            vlSelf->__VactIterCount = ((IData)(1U) 
                                       + vlSelf->__VactIterCount);
            vlSelf->__VactContinue = 0U;
            if (Vsoc_top___024root___eval_phase__act(vlSelf)) {
                vlSelf->__VactContinue = 1U;
            }
        }
        if (Vsoc_top___024root___eval_phase__nba(vlSelf)) {
            __VnbaContinue = 1U;
        }
    }
}

#ifdef VL_DEBUG
void Vsoc_top___024root___eval_debug_assertions(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_debug_assertions\n"); );
    // Body
    if (VL_UNLIKELY((vlSelf->clk & 0xfeU))) {
        Verilated::overWidthError("clk");}
    if (VL_UNLIKELY((vlSelf->rst_n & 0xfeU))) {
        Verilated::overWidthError("rst_n");}
}
#endif  // VL_DEBUG
