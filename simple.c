#include "lib/runestone.h"
#include <stdio.h>

int main(void) {
  rs_set_debug(true, stderr);

  rs_t rs;
  rs_init(&rs, RS_TARGET_AARCH64_MACOS_GAS);

  size_t entry_bb = rs_append_basic_block(&rs, "entry");
  size_t then_bb = rs_append_basic_block(&rs, NULL);
  size_t else_bb = rs_append_basic_block(&rs, NULL);

  rs_position_at_basic_block(&rs, entry_bb);
  rs_operand_t load34 = rs_build_load(&rs, RS_OPERAND_INT64(34));
  rs_operand_t load35 = rs_build_load(&rs, RS_OPERAND_INT64(36));
  rs_operand_t add_result = rs_build_add(&rs, load34, load35);
  rs_operand_t cmp_result =
      rs_build_cmp_eq(&rs, add_result, RS_OPERAND_INT64(69));
  rs_build_br_if(&rs, cmp_result, RS_OPERAND_BB(then_bb),
                 RS_OPERAND_BB(else_bb));

  rs_position_at_basic_block(&rs, then_bb);
  rs_build_ret(&rs, add_result);

  rs_position_at_basic_block(&rs, else_bb);
  rs_build_ret(&rs, RS_OPERAND_INT64(123));

  {
    FILE *fp = fopen("simple.S", "w");
    if (!fp) {
      perror("fopen() failed");
      return 1;
    }

    rs_generate(&rs, fp);

    fclose(fp);
  }

  rs_free(&rs);

  return 0;
}
