// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vsoc_top.h for the primary calling header

#include "Vsoc_top__pch.h"
#include "Vsoc_top___024root.h"

VL_ATTR_COLD void Vsoc_top___024root___eval_static(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_static\n"); );
}

VL_ATTR_COLD void Vsoc_top___024root___eval_initial__TOP(Vsoc_top___024root* vlSelf);

VL_ATTR_COLD void Vsoc_top___024root___eval_initial(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_initial\n"); );
    // Body
    Vsoc_top___024root___eval_initial__TOP(vlSelf);
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = vlSelf->clk;
    vlSelf->__Vtrigprevexpr___TOP__soc_top__DOT__core0__DOT__if_id_clr__0 
        = vlSelf->soc_top__DOT__core0__DOT__if_id_clr;
    vlSelf->__Vtrigprevexpr___TOP__soc_top__DOT__rst__0 
        = vlSelf->soc_top__DOT__rst;
}

VL_ATTR_COLD void Vsoc_top___024root___eval_initial__TOP(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_initial__TOP\n"); );
    // Init
    VlWide<3>/*95:0*/ __Vtemp_1;
    // Body
    VL_WRITEF("Loading RAM from: program.hex\n");
    __Vtemp_1[0U] = 0x2e686578U;
    __Vtemp_1[1U] = 0x6772616dU;
    __Vtemp_1[2U] = 0x70726fU;
    VL_READMEM_N(true, 32, 16384, 0, VL_CVT_PACK_STR_NW(3, __Vtemp_1)
                 ,  &(vlSelf->soc_top__DOT__ram0__DOT__mem)
                 , 0, ~0ULL);
}

VL_ATTR_COLD void Vsoc_top___024root___eval_final(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_final\n"); );
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__stl(Vsoc_top___024root* vlSelf);
#endif  // VL_DEBUG
VL_ATTR_COLD bool Vsoc_top___024root___eval_phase__stl(Vsoc_top___024root* vlSelf);

VL_ATTR_COLD void Vsoc_top___024root___eval_settle(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_settle\n"); );
    // Init
    IData/*31:0*/ __VstlIterCount;
    CData/*0:0*/ __VstlContinue;
    // Body
    __VstlIterCount = 0U;
    vlSelf->__VstlFirstIteration = 1U;
    __VstlContinue = 1U;
    while (__VstlContinue) {
        if (VL_UNLIKELY((0x64U < __VstlIterCount))) {
#ifdef VL_DEBUG
            Vsoc_top___024root___dump_triggers__stl(vlSelf);
#endif
            VL_FATAL_MT("hardware/rtl/soc_top.v", 6, "", "Settle region did not converge.");
        }
        __VstlIterCount = ((IData)(1U) + __VstlIterCount);
        __VstlContinue = 0U;
        if (Vsoc_top___024root___eval_phase__stl(vlSelf)) {
            __VstlContinue = 1U;
        }
        vlSelf->__VstlFirstIteration = 0U;
    }
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__stl(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___dump_triggers__stl\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VstlTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        VL_DBG_MSGF("         'stl' region trigger index 0 is active: Internal 'stl' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vsoc_top___024root___stl_sequent__TOP__0(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___stl_sequent__TOP__0\n"); );
    // Body
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
    vlSelf->soc_top__DOT__uart_addr = 0U;
    vlSelf->soc_top__DOT__uart_wr_en = 0U;
    vlSelf->soc_top__DOT__uart_rd_en = 0U;
    vlSelf->soc_top__DOT__uart_wdata = 0U;
    if ((8U != (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        if ((1U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                    >> 0x1cU))) {
            vlSelf->soc_top__DOT__uart_addr = vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out;
            vlSelf->soc_top__DOT__uart_wr_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out;
            vlSelf->soc_top__DOT__uart_rd_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out;
            vlSelf->soc_top__DOT__uart_wdata = vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out;
        }
    }
    vlSelf->soc_top__DOT__ram_wr_en = 0U;
    vlSelf->soc_top__DOT__ram_wdata = 0U;
    vlSelf->soc_top__DOT__c0_dbus_rdata = 0U;
    vlSelf->soc_top__DOT__rst = (1U & (~ (IData)(vlSelf->rst_n)));
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
    vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in)
            ? vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in
            : ((IData)(vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in)
                ? vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in
                : vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in));
    vlSelf->soc_top__DOT__core0__DOT__hazard_stall 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in) 
           & (((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in) 
               == (0x1fU & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                            >> 0xfU))) | ((IData)(vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in) 
                                          == (0x1fU 
                                              & (vlSelf->soc_top__DOT__core0__DOT__id_instruction_in 
                                                 >> 0x14U)))));
    vlSelf->soc_top__DOT__c0_dbus_ack = 0U;
    vlSelf->soc_top__DOT__arbiter__DOT__dbus_active 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out) 
           | (IData)(vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out));
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
    if ((8U == (vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out 
                >> 0x1cU))) {
        vlSelf->soc_top__DOT__ram_wr_en = vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out;
        vlSelf->soc_top__DOT__ram_wdata = vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out;
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
    vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in 
        = ((IData)(vlSelf->soc_top__DOT__core0__DOT__hazard_stall) 
           | (IData)(vlSelf->soc_top__DOT__core0__DOT__mem_stall));
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
    vlSelf->soc_top__DOT__core0__DOT__if_id_clr = ((IData)(vlSelf->soc_top__DOT__rst) 
                                                   | (IData)(vlSelf->soc_top__DOT__core0__DOT__branch_taken));
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

VL_ATTR_COLD void Vsoc_top___024root___eval_stl(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_stl\n"); );
    // Body
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        Vsoc_top___024root___stl_sequent__TOP__0(vlSelf);
        vlSelf->__Vm_traceActivity[4U] = 1U;
        vlSelf->__Vm_traceActivity[3U] = 1U;
        vlSelf->__Vm_traceActivity[2U] = 1U;
        vlSelf->__Vm_traceActivity[1U] = 1U;
        vlSelf->__Vm_traceActivity[0U] = 1U;
    }
}

VL_ATTR_COLD void Vsoc_top___024root___eval_triggers__stl(Vsoc_top___024root* vlSelf);

VL_ATTR_COLD bool Vsoc_top___024root___eval_phase__stl(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___eval_phase__stl\n"); );
    // Init
    CData/*0:0*/ __VstlExecute;
    // Body
    Vsoc_top___024root___eval_triggers__stl(vlSelf);
    __VstlExecute = vlSelf->__VstlTriggered.any();
    if (__VstlExecute) {
        Vsoc_top___024root___eval_stl(vlSelf);
    }
    return (__VstlExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__ico(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___dump_triggers__ico\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VicoTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VicoTriggered.word(0U))) {
        VL_DBG_MSGF("         'ico' region trigger index 0 is active: Internal 'ico' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__act(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VactTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 0 is active: @(posedge clk)\n");
    }
    if ((2ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 1 is active: @(posedge clk or posedge soc_top.core0.if_id_clr)\n");
    }
    if ((4ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 2 is active: @(posedge clk or posedge soc_top.rst)\n");
    }
}
#endif  // VL_DEBUG

#ifdef VL_DEBUG
VL_ATTR_COLD void Vsoc_top___024root___dump_triggers__nba(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___dump_triggers__nba\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VnbaTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 0 is active: @(posedge clk)\n");
    }
    if ((2ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 1 is active: @(posedge clk or posedge soc_top.core0.if_id_clr)\n");
    }
    if ((4ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 2 is active: @(posedge clk or posedge soc_top.rst)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vsoc_top___024root___ctor_var_reset(Vsoc_top___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vsoc_top__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vsoc_top___024root___ctor_var_reset\n"); );
    // Body
    vlSelf->clk = VL_RAND_RESET_I(1);
    vlSelf->rst_n = VL_RAND_RESET_I(1);
    vlSelf->uart_tx = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__rst = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__c0_ibus_data = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__c0_ibus_ack = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__c0_dbus_rdata = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__c0_dbus_ack = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__ram_addr = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__ram_wdata = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__ram_rdata = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__ram_rd_en = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__ram_wr_en = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__ram_ack = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__uart_addr = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__uart_wdata = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__uart_rdata = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__uart_rd_en = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__uart_wr_en = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__uart_ack = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__next_pc = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__curr_pc = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__hazard_stall = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__mem_stall = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__pipe_en_all = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__if_id_en = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__if_id_clr = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_ex_clr = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__branch_taken = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__raw_instruction = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT____Vcellinp__fetch_stage__pipeline_stall_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__decompressed_instruction = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__id_instruction_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__id_pc_plus_4_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__id_pc_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__wb_write_data_feedback = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_rd_addr_in = VL_RAND_RESET_I(5);
    vlSelf->soc_top__DOT__core0__DOT__ex_mem_read_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_reg_write_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_immediate_out = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__id_mem_read_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_mem_write_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_reg_write_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_mem_to_reg_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_alu_src_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_branch_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_jump_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_write_from_pc_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__id_alu_ctrl_out = VL_RAND_RESET_I(4);
    vlSelf->soc_top__DOT__core0__DOT__ex_pc_plus_4_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_pc_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_read_data1_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_read_data2_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_immediate_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_instruction_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_rs1_addr_in = VL_RAND_RESET_I(5);
    vlSelf->soc_top__DOT__core0__DOT__ex_rs2_addr_in = VL_RAND_RESET_I(5);
    vlSelf->soc_top__DOT__core0__DOT__ex_mem_write_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_mem_to_reg_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_alu_src_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_branch_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_jump_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_write_from_pc_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ex_alu_ctrl_in = VL_RAND_RESET_I(4);
    vlSelf->soc_top__DOT__core0__DOT__forward_a = VL_RAND_RESET_I(2);
    vlSelf->soc_top__DOT__core0__DOT__forward_b = VL_RAND_RESET_I(2);
    vlSelf->soc_top__DOT__core0__DOT__ma_pc_plus_4_out = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ma_alu_result_out = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ma_write_data_out = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ma_rd_addr_out = VL_RAND_RESET_I(5);
    vlSelf->soc_top__DOT__core0__DOT__ma_mem_read_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ma_mem_write_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ma_reg_write_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ma_mem_to_reg_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__ma_write_from_pc_out = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__wb_alu_result_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__wb_read_data_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__wb_pc_plus_4_in = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__wb_rd_addr_in = VL_RAND_RESET_I(5);
    vlSelf->soc_top__DOT__core0__DOT__wb_reg_write_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__wb_mem_to_reg_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__wb_write_from_pc_in = VL_RAND_RESET_I(1);
    vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__i = VL_RAND_RESET_I(32);
    for (int __Vi0 = 0; __Vi0 < 32; ++__Vi0) {
        vlSelf->soc_top__DOT__core0__DOT__decode_stage__DOT__rf__DOT__registers[__Vi0] = VL_RAND_RESET_I(32);
    }
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_a = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__op_b_forwarded = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__core0__DOT__ex_stage__DOT__alu_op_b = VL_RAND_RESET_I(32);
    vlSelf->soc_top__DOT__arbiter__DOT__dbus_active = VL_RAND_RESET_I(1);
    for (int __Vi0 = 0; __Vi0 < 16384; ++__Vi0) {
        vlSelf->soc_top__DOT__ram0__DOT__mem[__Vi0] = VL_RAND_RESET_I(32);
    }
    vlSelf->soc_top__DOT__uart0__DOT__tx_done = VL_RAND_RESET_I(1);
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = VL_RAND_RESET_I(1);
    vlSelf->__Vtrigprevexpr___TOP__soc_top__DOT__core0__DOT__if_id_clr__0 = VL_RAND_RESET_I(1);
    vlSelf->__Vtrigprevexpr___TOP__soc_top__DOT__rst__0 = VL_RAND_RESET_I(1);
    for (int __Vi0 = 0; __Vi0 < 5; ++__Vi0) {
        vlSelf->__Vm_traceActivity[__Vi0] = 0;
    }
}
