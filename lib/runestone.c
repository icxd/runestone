#include "runestone.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rs_init(rs_t *rs, rs_target_t target) {
  rs->target = target;

  rs->basic_blocks = malloc(sizeof(rs_basic_block_t) * RS_MAX_BB);
  rs->basic_blocks_count = 0;

  rs->current_basic_block = -1;

  for (size_t i = 0; i < 256; i++)
    rs->lifetimes[i] = (rs_lifetime_t){0, 0, -1, -1};

  rs->register_pool = malloc(rs_get_register_count(target));
  for (size_t i = 0; i < rs_get_register_count(target); i++)
    rs->register_pool[i] = false;

  rs_regmap_init(&rs->register_map);

  rs->stack_size = 0;

  rs->next_dst_vreg = 0;
}

void rs_free(rs_t *rs) {
  free(rs->basic_blocks);
  rs_regmap_free(&rs->register_map);
}

size_t rs_append_basic_block(rs_t *rs, const char *name) {
  rs_basic_block_t *bb = malloc(sizeof(rs_basic_block_t));
  bb->instructions = malloc(sizeof(rs_instr_t) * RS_MAX_INSTR);
  bb->instruction_count = 0;

  if (name == NULL) {
    char buffer[20];
    sprintf(buffer, "bb_%zu", rs->basic_blocks_count);
    bb->name = strdup(buffer);
  } else {
    bb->name = strdup(name);
  }

  rs->basic_blocks[rs->basic_blocks_count++] = bb;
  return rs->basic_blocks_count - 1;
}

void rs_position_at_basic_block(rs_t *rs, size_t block_id) {
  rs->current_basic_block = block_id;
}

void rs_build_instr(rs_t *rs, rs_instr_t instr) {
  assert(rs->current_basic_block != -1);
  rs_basic_block_t *bb = rs->basic_blocks[rs->current_basic_block];
  bb->instructions[bb->instruction_count++] = instr;
}

rs_operand_t rs_build_move(rs_t *rs, rs_operand_t src) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_MOVE, dst, src, RS_OPERAND_NULL,
                                  RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_copy(rs_t *rs, rs_operand_t src) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_COPY, dst, src,
                                  RS_OPERAND_REG(RS_TEMPORARY_VREG),
                                  RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_load(rs_t *rs, rs_operand_t src) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_LOAD, dst, src, RS_OPERAND_NULL,
                                  RS_OPERAND_NULL});
  return dst;
}

void rs_build_store(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_STORE, RS_OPERAND_NULL, src1, src2,
                                  RS_OPERAND_NULL});
}

rs_operand_t rs_build_add(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_ADD, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_sub(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_SUB, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_mult(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_MULT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_div(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_DIV, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

void rs_build_ret(rs_t *rs, rs_operand_t src) {
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_RET, RS_OPERAND_NULL, src,
                                  RS_OPERAND_NULL, RS_OPERAND_NULL});
}

void rs_build_br(rs_t *rs, rs_operand_t src) {
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_BR, RS_OPERAND_NULL, src,
                                  RS_OPERAND_NULL, RS_OPERAND_NULL});
}

void rs_build_br_if(rs_t *rs, rs_operand_t src1, rs_operand_t src2,
                    rs_operand_t src3) {
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_BR_IF, RS_OPERAND_NULL, src1, src2, src3});
}

rs_operand_t rs_build_cmp_eq(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_EQ, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_cmp_lt(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_LT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_cmp_gt(rs_t *rs, rs_operand_t src1, rs_operand_t src2) {
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_GT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

bool rs_instr_is_terminator(rs_instr_t instr) {
  return instr.opcode == RS_OPCODE_RET || instr.opcode == RS_OPCODE_BR ||
         instr.opcode == RS_OPCODE_BR_IF;
}

static rs_register_t rs_allocate_register(rs_t *rs) {
  rs_register_t reg = rs_get_free_register(rs);
  if (reg == RS_REG_SPILL)
    return RS_REG_SPILL;
  rs->register_pool[reg] = true;
  return reg;
}

static void rs_free_register(rs_t *rs, rs_register_t reg) {
  rs->register_pool[reg] = false;
}

rs_register_t rs_get_free_register(rs_t *rs) {
  for (size_t i = 0; i < rs_get_register_count(rs->target); i++) {
    if (!rs->register_pool[i]) {
      return i;
    }
  }
  return RS_REG_SPILL;
}

static void rs_analyze_operand(rs_t *rs, size_t i, rs_operand_t operand) {
  rs_lifetime_t lifetime = rs->lifetimes[operand.vreg];
  lifetime.vreg = operand.vreg;
  if (lifetime.start == -1)
    lifetime.start = i;
  lifetime.end = i + 1;
  rs->lifetimes[operand.vreg] = lifetime;
}

rs_register_t rs_get_register(rs_t *rs, size_t vreg) {
  if (rs_regmap_contains(&rs->register_map, vreg))
    return rs_regmap_get(&rs->register_map, vreg);

  rs_register_t reg = rs_allocate_register(rs);
  if (reg == RS_REG_SPILL)
    return RS_REG_SPILL;
  rs_regmap_insert(&rs->register_map, vreg, reg);
  return reg;
}

size_t rs_get_register_count(rs_target_t target) {
  switch (target) {
#define RS_TARGET(_, upper, count, ...)                                        \
  case RS_TARGET_##upper:                                                      \
    return count;
    RS_TARGETS
#undef RS_TARGET

  case RS_TARGET_COUNT:
    assert(false && "unreachable");
  }
}

const char **rs_get_register_names(rs_target_t target) {
  switch (target) {
#define RS_TARGET(lower, upper, ...)                                           \
  case RS_TARGET_##upper:                                                      \
    return rs_target_##lower##_reg_names;
    RS_TARGETS
#undef RS_TARGET

  case RS_TARGET_COUNT:
    assert(false && "unreachable");
  }
}

static void rs_alloc_and_free_lifetimes(rs_t *rs, rs_basic_block_t *block) {
  for (size_t ip = 0; ip < block->instruction_count; ip++) {
    for (size_t lifetime_index = 0; lifetime_index < 256; lifetime_index++) {
      rs_lifetime_t lifetime = rs->lifetimes[lifetime_index];
      if (lifetime.start == -1 || lifetime.end == -1)
        continue;

      if ((size_t)lifetime.start == ip) {
        rs_register_t reg = rs_get_register(rs, lifetime.vreg);
        if (reg == RS_REG_SPILL) {
          printf("REGALLOC [ip: %zu]: spilling vreg %zu\n", ip, lifetime_index);
          abort();
        }

        rs_regmap_insert(&rs->register_map, lifetime_index, reg);
        rs->lifetimes[lifetime_index].reg = reg;
      }

      if ((size_t)lifetime.end == ip)
        rs_free_register(rs, rs->lifetimes[lifetime_index].reg);
    }
  }
}

void rs_analyze_lifetimes(rs_t *rs) {
  for (size_t block_id = 0; block_id < rs->basic_blocks_count; block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];

    for (size_t i = 0; i < bb->instruction_count; i++) {
      rs_instr_t instr = bb->instructions[i];
      if (instr.dest.type == RS_OPERAND_TYPE_REG)
        rs_analyze_operand(rs, i, instr.dest);
      if (instr.src1.type == RS_OPERAND_TYPE_REG)
        rs_analyze_operand(rs, i, instr.src1);
      if (instr.src2.type == RS_OPERAND_TYPE_REG)
        rs_analyze_operand(rs, i, instr.src2);
      if (instr.src3.type == RS_OPERAND_TYPE_REG)
        rs_analyze_operand(rs, i, instr.src3);
    }

    rs_alloc_and_free_lifetimes(rs, bb);
  }
}

void rs_finalize(rs_t *rs) {
  bool has_error = false;
  for (size_t i = 0; i < rs->basic_blocks_count; i++) {
    rs_basic_block_t *bb = rs->basic_blocks[i];
    if (!rs_instr_is_terminator(bb->instructions[bb->instruction_count - 1])) {
      fprintf(stderr,
              "ERROR: missing terminator instruction in basic block `%s`\n",
              bb->name);
      has_error = true;
      continue;
    }
  }

  if (has_error)
    exit(1);
}

static void rs_operand_print(rs_t *rs, FILE *fp, rs_operand_t operand) {
  (void)rs;
  switch (operand.type) {
  case RS_OPERAND_TYPE_NULL:
    fprintf(fp, "<null>");
    break;
  case RS_OPERAND_TYPE_INT64:
    fprintf(fp, "%lld", operand.int64);
    break;
  case RS_OPERAND_TYPE_ADDR:
    fprintf(fp, "%p", (void *)operand.addr);
    break;
  case RS_OPERAND_TYPE_REG:
    fprintf(fp, "%%%d", operand.vreg);
    break;
  case RS_OPERAND_TYPE_BB:
    fprintf(fp, "bb_%zu", operand.bb_id);
    break;
  default:
    abort();
  }
}

void rs_dump_instr(rs_t *rs, FILE *fp, rs_instr_t instr) {
  if (instr.dest.type != RS_OPERAND_TYPE_NULL) {
    rs_operand_print(rs, fp, instr.dest);
    fprintf(fp, " = ");
  }
  fprintf(fp, "%s ", rs_opcode_to_str(instr.opcode));
  if (instr.src1.type != RS_OPERAND_TYPE_NULL)
    rs_operand_print(rs, fp, instr.src1);
  if (instr.src2.type != RS_OPERAND_TYPE_NULL) {
    fprintf(fp, ", ");
    rs_operand_print(rs, fp, instr.src2);
  }
  if (instr.src3.type != RS_OPERAND_TYPE_NULL) {
    fprintf(fp, ", ");
    rs_operand_print(rs, fp, instr.src3);
  }
}

void rs_dump(rs_t *rs, FILE *fp) {
  for (size_t block_id = 0; block_id < rs->basic_blocks_count; block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    fprintf(fp, "%s:\n", bb->name);

    for (size_t i = 0; i < bb->instruction_count; i++) {
      rs_instr_t instr = bb->instructions[i];
      fprintf(fp, "  ");
      rs_dump_instr(rs, fp, instr);
      fprintf(fp, "\n");
    }
  }
}

void rs_generate(rs_t *rs, FILE *fp) {
  rs_finalize(rs);
  rs_analyze_lifetimes(rs);

  switch (rs->target) {
  case RS_TARGET_X86_64_LINUX_NASM:
    rs_generate_x86_64_linux_nasm(rs, fp);
    break;
  case RS_TARGET_AARCH64_MACOS_GAS:
    rs_generate_aarch64_macos_gas(rs, fp);
    break;
  case RS_TARGET_COUNT:
    break;
  }
}

void rs_regmap_init(rs_register_map_t *map) {
  map->entries = malloc(sizeof(rs_map_entry_t) * RS_REGMAP_INIT_CAPACITY);
  map->count = 0;
  map->capacity = RS_REGMAP_INIT_CAPACITY;
}

void rs_regmap_free(rs_register_map_t *map) { free(map->entries); }

void rs_regmap_insert(rs_register_map_t *map, size_t key, rs_register_t value) {
  if (map->count == map->capacity) {
    map->capacity *= 2;
    map->entries =
        realloc(map->entries, sizeof(rs_map_entry_t) * map->capacity);
  }
  map->entries[map->count++] = (rs_map_entry_t){key, value};
}

rs_register_t rs_regmap_get(rs_register_map_t *map, size_t key) {
  for (size_t i = 0; i < map->count; i++) {
    if (map->entries[i].key == key) {
      return map->entries[i].value;
    }
  }
  return RS_REG_SPILL;
}

bool rs_regmap_contains(rs_register_map_t *map, size_t key) {
  for (size_t i = 0; i < map->count; i++) {
    if (map->entries[i].key == key) {
      return true;
    }
  }
  return false;
}

void rs_regmap_remove(rs_register_map_t *map, size_t key) {
  for (size_t i = 0; i < map->count; i++) {
    if (map->entries[i].key == key) {
      map->entries[i] = map->entries[--map->count];
      return;
    }
  }
}
