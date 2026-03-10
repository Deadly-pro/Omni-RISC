// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vsoc_top.h for the primary calling header

#ifndef VERILATED_VSOC_TOP___024ROOT_H_
#define VERILATED_VSOC_TOP___024ROOT_H_  // guard

#include "verilated.h"


class Vsoc_top__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vsoc_top___024root final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    // Anonymous structures to workaround compiler member-count bugs
    struct {
        VL_IN8(clk,0,0);
        CData/*0:0*/ soc_top__DOT__rst;
        CData/*0:0*/ soc_top__DOT__core0__DOT__if_id_clr;
        VL_IN8(rst_n,0,0);
        VL_OUT8(uart_tx,0,0);
        CData/*0:0*/ soc_top__DOT__c0_ibus_ack;
        CData/*0:0*/ soc_top__DOT__c0_dbus_ack;
        CData/*0:0*/ soc_top__DOT__ram_rd_en;
        CData/*0:0*/ soc_top__DOT__ram_wr_en;
        CData/*0:0*/ soc_top__DOT__ram_ack;
        CData/*0:0*/ soc_top__DOT__uart_rd_en;
        CData/*0:0*/ soc_top__DOT__uart_wr_en;
        CData/*0:0*/ soc_top__DOT__uart_ack;
        CData/*0:0*/ soc_top__DOT__core0__DOT__hazard_stall;
        CData/*0:0*/ soc_top__DOT__core0__DOT__mem_stall;
        CData/*0:0*/ soc_top__DOT__core0__DOT__pipe_en_all;
        CData/*0:0*/ soc_top__DOT__core0__DOT__if_id_en;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_ex_clr;
        CData/*0:0*/ soc_top__DOT__core0__DOT__branch_taken;
        CData/*0:0*/ soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in;
        CData/*4:0*/ soc_top__DOT__core0__DOT__ex_rd_addr_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_mem_read_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_reg_write_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_mem_read_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_mem_write_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_reg_write_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_mem_to_reg_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_alu_src_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_branch_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_jump_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__id_write_from_pc_out;
        CData/*3:0*/ soc_top__DOT__core0__DOT__id_alu_ctrl_out;
        CData/*4:0*/ soc_top__DOT__core0__DOT__ex_rs1_addr_in;
        CData/*4:0*/ soc_top__DOT__core0__DOT__ex_rs2_addr_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_mem_write_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_mem_to_reg_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_alu_src_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_branch_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_jump_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ex_write_from_pc_in;
        CData/*3:0*/ soc_top__DOT__core0__DOT__ex_alu_ctrl_in;
        CData/*1:0*/ soc_top__DOT__core0__DOT__forward_a;
        CData/*1:0*/ soc_top__DOT__core0__DOT__forward_b;
        CData/*4:0*/ soc_top__DOT__core0__DOT__ma_rd_addr_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ma_mem_read_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ma_mem_write_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ma_reg_write_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ma_mem_to_reg_out;
        CData/*0:0*/ soc_top__DOT__core0__DOT__ma_write_from_pc_out;
        CData/*4:0*/ soc_top__DOT__core0__DOT__wb_rd_addr_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__wb_reg_write_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__wb_mem_to_reg_in;
        CData/*0:0*/ soc_top__DOT__core0__DOT__wb_write_from_pc_in;
        CData/*0:0*/ soc_top__DOT__arbiter__DOT__dbus_active;
        CData/*0:0*/ soc_top__DOT__uart0__DOT__tx_done;
        CData/*0:0*/ __VstlFirstIteration;
        CData/*0:0*/ __VicoFirstIteration;
        CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
        CData/*0:0*/ __Vtrigprevexpr___TOP__soc_top__DOT__core0__DOT__if_id_clr__0;
        CData/*0:0*/ __Vtrigprevexpr___TOP__soc_top__DOT__rst__0;
        CData/*0:0*/ __VactContinue;
        IData/*31:0*/ soc_top__DOT__c0_ibus_data;
        IData/*31:0*/ soc_top__DOT__c0_dbus_rdata;
        IData/*31:0*/ soc_top__DOT__ram_addr;
    };
    struct {
        IData/*31:0*/ soc_top__DOT__ram_wdata;
        IData/*31:0*/ soc_top__DOT__ram_rdata;
        IData/*31:0*/ soc_top__DOT__uart_addr;
        IData/*31:0*/ soc_top__DOT__uart_wdata;
        IData/*31:0*/ soc_top__DOT__uart_rdata;
        IData/*31:0*/ soc_top__DOT__core0__DOT__next_pc;
        IData/*31:0*/ soc_top__DOT__core0__DOT__curr_pc;
        IData/*31:0*/ soc_top__DOT__core0__DOT__raw_instruction;
        IData/*31:0*/ soc_top__DOT__core0__DOT__decompressed_instruction;
        IData/*31:0*/ soc_top__DOT__core0__DOT__id_instruction_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__id_pc_plus_4_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__id_pc_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__wb_write_data_feedback;
        IData/*31:0*/ soc_top__DOT__core0__DOT__id_immediate_out;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_pc_plus_4_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_pc_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_read_data1_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_read_data2_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_immediate_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_instruction_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ma_pc_plus_4_out;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ma_alu_result_out;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ma_write_data_out;
        IData/*31:0*/ soc_top__DOT__core0__DOT__wb_alu_result_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__wb_read_data_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__wb_pc_plus_4_in;
        IData/*31:0*/ soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__i;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_stage__DOT__op_a;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded;
        IData/*31:0*/ soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b;
        IData/*31:0*/ __VactIterCount;
        VlUnpacked<IData/*31:0*/, 32> soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers;
        VlUnpacked<IData/*31:0*/, 16384> soc_top__DOT__ram0__DOT__mem;
        VlUnpacked<CData/*0:0*/, 5> __Vm_traceActivity;
    };
    VlTriggerVec<1> __VstlTriggered;
    VlTriggerVec<1> __VicoTriggered;
    VlTriggerVec<3> __VactTriggered;
    VlTriggerVec<3> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vsoc_top__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vsoc_top___024root(Vsoc_top__Syms* symsp, const char* v__name);
    ~Vsoc_top___024root();
    VL_UNCOPYABLE(Vsoc_top___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
