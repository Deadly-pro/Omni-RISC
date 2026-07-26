
  module hazard_unit (
      // --- consumer currently in ID (still being decoded) ---
      input  [4:0] if_id_rs1,        // source regs of the instr in decode
      input  [4:0] if_id_rs2,
      // --- producer currently in EX (the potential load) ---
      input  [4:0] id_ex_rd,         // its destination
      input        id_ex_mem_read,   // is it a load? (only loads have the N+1 latency)

      output       stall,            // 1 = freeze PC + IF/ID, bubble ID/EX for one cycle
      output       bubble            // usually == stall; separate port keeps intent explicit
  );
  assign stall=((if_id_rs1==id_ex_rd) | (if_id_rs2==id_ex_rd))&& id_ex_mem_read && id_ex_rd!=5'b00000;
  assign bubble=stall;
  endmodule 
