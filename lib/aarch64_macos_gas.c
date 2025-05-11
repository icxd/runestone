#include "runestone.h"
#include <assert.h>
#include "cvector_utils.h"

void rs_generate_aarch64_macos_gas(rs_t *rs, FILE *fp) {
  fprintf(fp, ".text\n");
  fprintf(fp, ".global _start\n");
  fprintf(fp, "_start:\n");

  rs_basic_block_t **bb_it;
  cvector_for_each_in(bb_it, rs->basic_blocks)
  {
    fprintf(fp, ".%s:\n", (*bb_it)->name);

    rs_instr_t *instr_it;
    cvector_for_each_in(instr_it, (*bb_it)->instructions)
    {
      rs_generate_instr_aarch64_macos_gas(rs, fp, *instr_it);
    }
  }
}

void rs_generate_instr_aarch64_macos_gas(rs_t *rs, FILE *fp, rs_instr_t instr) {
  fprintf(fp, "  ; ");
  rs_dump_instr(rs, fp, instr);
  fprintf(fp, "\n");
  switch (instr.opcode) {
  case RS_OPCODE_MOVE:
    assert(false && "unimplemented");
    break;

  case RS_OPCODE_COPY:
    assert(false && "unimplemented");
    break;

  case RS_OPCODE_LOAD:
    fprintf(fp, "  mov ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, true);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_STORE:
    assert(false && "unimplemented");
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
  case RS_OPCODE_MULT:
  case RS_OPCODE_DIV:
    assert(false && "unimplemented");
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

  case RS_OPCODE_BR_IF:
    fprintf(fp, "  cbnz ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    fprintf(fp, "  b ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src3, false);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_CMP_EQ:
    fprintf(fp, "  cmp ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    fprintf(fp, "  cset ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", eq\n");
    break;

  case RS_OPCODE_CMP_LT:
    fprintf(fp, "  cmp ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    fprintf(fp, "  cset ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", lt\n");
    break;

  case RS_OPCODE_CMP_GT:
    fprintf(fp, "  cmp ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src1, false);
    fprintf(fp, ", ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    fprintf(fp, "  cset ");
    rs_generate_operand_aarch64_macos_gas(rs, fp, instr.dest, false);
    fprintf(fp, ", gt\n");
    break;

  case RS_OPCODE_COUNT:
    assert(false && "unreachable");
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
