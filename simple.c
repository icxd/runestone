#include "lib/runestone.h"
#include <stdio.h>

int main(void) {
  rs_t rs;
  rs_init(&rs, RS_TARGET_AARCH64_MACOS_GAS);

  size_t entry_bb = rs_append_basic_block(&rs, "entry");
  size_t exit_bb = rs_append_basic_block(&rs, NULL);

  rs_position_at_basic_block(&rs, entry_bb);
  rs_operand_t load34 = rs_build_load(&rs, RS_OPERAND_INT64(34));
  rs_operand_t load35 = rs_build_load(&rs, RS_OPERAND_INT64(35));
  rs_operand_t add_result = rs_build_add(&rs, load34, load35);
  rs_build_br(&rs, RS_OPERAND_BB(exit_bb));

  rs_position_at_basic_block(&rs, exit_bb);
  rs_build_ret(&rs, add_result);

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
