#include "runestone.h"
#include <assert.h>

void rs_generate_x86_64_linux_nasm(rs_t *rs, FILE *fp) {
  fprintf(fp, "section .text\n");
  fprintf(fp, "global _start:\n");
  fprintf(fp, "_start:\n");
  for (size_t i = 0; i < rs->basic_blocks_count; i++) {
    rs_basic_block_t *bb = rs->basic_blocks[i];
    fprintf(fp, ".%s:\n", bb->name);

    for (size_t j = 0; j < bb->instruction_count; j++) {
      rs_instr_t instr = bb->instructions[j];
      rs_generate_instr_x86_64_linux_nasm(rs, fp, instr);
    }
  }
}
void rs_generate_instr_x86_64_linux_nasm(rs_t *rs, FILE *fp, rs_instr_t instr) {
  fprintf(fp, "  ; ");
  rs_dump_instr(rs, fp, instr);
  fprintf(fp, "\n");
  switch (instr.opcode) {
  /*
   * mov dst, src
   **/
  case RS_OPCODE_MOVE:
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, false);
    fprintf(fp, "\n");
    break;

  /*
   * mov %0, [src]
   * mov [dst], %0
   **/
  case RS_OPCODE_COPY:
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src2, false);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, true);
    fprintf(fp, "\n");
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, true);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    break;

  /*
   * mov dst, [src]
   **/
  case RS_OPCODE_LOAD:
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, true);
    fprintf(fp, "\n");
    break;

  /*
   * mov [dst], src
   **/
  case RS_OPCODE_STORE:
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, true);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, false);
    fprintf(fp, "\n");
    break;

  /*
   * mov dst, src1
   * add dst, src2
   **/
  case RS_OPCODE_ADD:
    fprintf(fp, "  mov ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, false);
    fprintf(fp, "\n");
    fprintf(fp, "  add ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.dest, false);
    fprintf(fp, ", ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src2, false);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_SUB:
  case RS_OPCODE_MULT:
  case RS_OPCODE_DIV:
    assert(false && "unimplemented");
    break;

  case RS_OPCODE_RET:
    if (instr.src1.type != RS_OPERAND_TYPE_NULL) {
      fprintf(fp, "  mov rax, ");
      rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, false);
      fprintf(fp, "\n");
    }
    fprintf(fp, "  ret\n");
    break;

  case RS_OPCODE_BR:
    fprintf(fp, "  jmp ");
    rs_generate_operand_x86_64_linux_nasm(rs, fp, instr.src1, false);
    fprintf(fp, "\n");
    break;

  case RS_OPCODE_BR_IF:
  case RS_OPCODE_CMP_EQ:
  case RS_OPCODE_CMP_LT:
  case RS_OPCODE_CMP_GT:
    assert(false && "unimplemented");
    break;

  case RS_OPCODE_COUNT:
    assert(false && "unreachable");
    break;
  }
}
void rs_generate_operand_x86_64_linux_nasm(rs_t *rs, FILE *fp,
                                           rs_operand_t operand,
                                           bool dereference) {
  switch (operand.type) {
  case RS_OPERAND_TYPE_NULL:
    break;
  case RS_OPERAND_TYPE_INT64:
    fprintf(fp, "%lld", operand.int64);
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
