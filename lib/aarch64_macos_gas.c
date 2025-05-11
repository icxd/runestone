#include "runestone.h"

void rs_generate_aarch64_macos_gas(rs_t *rs, FILE *fp) {
  fprintf(fp, ".text\n");
  fprintf(fp, ".global _start\n");
  fprintf(fp, "_start:\n");
  for (size_t i = 0; i < rs->basic_blocks_count; i++) {
    rs_basic_block_t *bb = rs->basic_blocks[i];
    fprintf(fp, ".%s:\n", bb->name);

    for (size_t j = 0; j < bb->instruction_count; j++) {
      rs_instr_t instr = bb->instructions[j];
      rs_generate_instr_aarch64_macos_gas(rs, fp, instr);
    }
  }
}

void rs_generate_instr_aarch64_macos_gas(rs_t *rs, FILE *fp, rs_instr_t instr) {
  fprintf(fp, "  ; ");
  rs_dump_instr(rs, fp, instr);
  fprintf(fp, "\n");
  switch (instr.opcode) {
  case RS_OPCODE_MOVE:
    break;

  case RS_OPCODE_COPY:
    break;

  case RS_OPCODE_LOAD:
    fprintf(fp, "  mov ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, true);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_STORE:
    break;

  case RS_OPCODE_ADD:
    fprintf(fp, "  add ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_SUB:
    break;

  case RS_OPCODE_MULT:
    break;

  case RS_OPCODE_DIV:
    break;

  case RS_OPCODE_RET:
    if (instr.src1.type != RS_OPERAND_TYPE_NULL) {
      fprintf(fp, "  mov x0, ");
      rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
      fprintf(fp, "\n");
    }
    fprintf(fp, "  ret\n");
    break;

  case RS_OPCODE_BR:
    fprintf(fp, "  b ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, "\n");
    break;
  case RS_OPCODE_COUNT:
    break;
  }
}

void rs_generate_operand_aarch64_macos_gas(rs_t *rs, FILE *fp,
                                           rs_operand_t operand,
                                           bool dereference) {
  switch (operand.type) {
  case RS_OPERAND_TYPE_NULL:
    break;
  case RS_OPERAND_TYPE_INT64:
    fprintf(fp, "#%lld", operand.int64);
    break;
  case RS_OPERAND_TYPE_ADDR:
    fprintf(fp, "%zu", operand.addr);
    break;
  case RS_OPERAND_TYPE_REG:
    fprintf(
        fp, "%s%s%s", dereference ? "[" : "",
        rs_get_register_names(rs->target)[rs_get_register(rs, operand.vreg)],
        dereference ? "]" : "");
    break;
  case RS_OPERAND_TYPE_BB:
    fprintf(fp, ".%s", rs->basic_blocks[operand.bb_id]->name);
    break;
  case RS_OPERAND_TYPE_COUNT:
    break;
  }
}
