#include "runestone.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cvector_utils.h"
#include "cvector.h"
#include <stdarg.h>

static bool debug_enabled = false;
static FILE *debug_stream = NULL;

void rs_set_debug(bool enabled, FILE *stream)
{
  debug_enabled = enabled;
  debug_stream = stream ? stream : stderr;
}

static void debug_log(const char *format, ...)
{
  if (!debug_enabled || !debug_stream)
    return;

  va_list args;
  va_start(args, format);
  vfprintf(debug_stream, format, args);
  va_end(args);
  fprintf(debug_stream, "\n");
}

static void free_basic_block(void *ptr)
{
  rs_basic_block_t *bb = *(rs_basic_block_t **)ptr;
  if (bb)
  {
    debug_log("Freeing basic block '%s' with %zu instructions",
              bb->name, cvector_size(bb->instructions));
    cvector_free(bb->instructions);
    free(bb->name);
    free(bb);
  }
}

void rs_init(rs_t *rs, rs_target_t target)
{
  if (!rs)
  {
    fprintf(stderr, "Error: NULL Runestone state pointer\n");
    return;
  }

  debug_log("Initializing Runestone state for target %d", target);
  rs->target = target;

  rs->basic_blocks = NULL;
  cvector_init(rs->basic_blocks, RS_MAX_BB, free_basic_block);
  rs->current_basic_block = -1;

  for (size_t i = 0; i < 256; i++)
    rs->lifetimes[i] = (rs_lifetime_t){0, -1, -1};

  rs->register_pool = NULL;
  size_t reg_count = rs_get_register_count(target);
  debug_log("Initializing register pool with %zu registers", reg_count);
  cvector_init(rs->register_pool, reg_count, NULL);
  for (size_t i = 0; i < reg_count; i++)
    cvector_push_back(rs->register_pool, false);

  rs_regmap_init(&rs->register_map);

  rs->stack_size = 0;
  rs->next_dst_vreg = 0;
}

void rs_free(rs_t *rs)
{
  cvector_free(rs->basic_blocks);
  cvector_free(rs->register_pool);
  rs_regmap_free(&rs->register_map);
}

size_t rs_append_basic_block(rs_t *rs, const char *name)
{
  rs_basic_block_t *bb = malloc(sizeof(rs_basic_block_t));
  if (!bb)
  {
    fprintf(stderr, "Failed to allocate memory for basic block\n");
    return SIZE_MAX;
  }

  bb->instructions = NULL;
  cvector_init(bb->instructions, RS_MAX_INSTR, NULL);

  if (name == NULL)
  {
    char buffer[20];
    sprintf(buffer, "bb_%zu", cvector_size(rs->basic_blocks));
    bb->name = strdup(buffer);
  }
  else
  {
    bb->name = strdup(name);
  }

  if (!bb->name)
  {
    fprintf(stderr, "Failed to allocate memory for basic block name\n");
    cvector_free(bb->instructions);
    free(bb);
    return SIZE_MAX;
  }

  debug_log("Appending basic block '%s'", bb->name);
  cvector_push_back(rs->basic_blocks, bb);
  return cvector_size(rs->basic_blocks) - 1;
}

void rs_position_at_basic_block(rs_t *rs, size_t block_id)
{
  rs->current_basic_block = block_id;
}

static bool is_valid_register(rs_t *rs, rs_register_t reg)
{
  return reg < rs_get_register_count(rs->target);
}

static bool is_valid_operand(rs_t *rs, rs_operand_t operand)
{
  switch (operand.type)
  {
  case RS_OPERAND_TYPE_NULL:
    return true;
  case RS_OPERAND_TYPE_REG:
    return operand.vreg < RS_MAX_REGS;
  case RS_OPERAND_TYPE_BB:
    return operand.bb_id < cvector_size(rs->basic_blocks);
  case RS_OPERAND_TYPE_INT64:
  case RS_OPERAND_TYPE_ADDR:
    return true;
  default:
    return false;
  }
}

void rs_build_instr(rs_t *rs, rs_instr_t instr)
{
  assert(rs->current_basic_block != -1);
  rs_basic_block_t *bb = rs->basic_blocks[rs->current_basic_block];
  if (!bb)
  {
    fprintf(stderr, "Error: Invalid basic block at index %ld\n", rs->current_basic_block);
    return;
  }

  if (!is_valid_operand(rs, instr.dest) ||
      !is_valid_operand(rs, instr.src1) ||
      !is_valid_operand(rs, instr.src2))
  {
    fprintf(stderr, "Error: Invalid operand in instruction %s\n", rs_opcode_to_str(instr.opcode));
    return;
  }

  debug_log("Building instruction %s in block '%s'",
            rs_opcode_to_str(instr.opcode), bb->name);
  cvector_push_back(bb->instructions, instr);
}

rs_operand_t rs_build_move(rs_t *rs, rs_operand_t src)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_MOVE, dst, src, RS_OPERAND_NULL,
                                  RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_copy(rs_t *rs, rs_operand_t src)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_COPY, dst, src,
                                  RS_OPERAND_REG(RS_TEMPORARY_VREG),
                                  RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_load(rs_t *rs, rs_operand_t src)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_LOAD, dst, src, RS_OPERAND_NULL,
                                  RS_OPERAND_NULL});
  return dst;
}

void rs_build_store(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_STORE, RS_OPERAND_NULL, src1, src2,
                                  RS_OPERAND_NULL});
}

rs_operand_t rs_build_add(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_ADD, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_sub(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_SUB, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_mult(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_MULT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_div(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(rs,
                 (rs_instr_t){RS_OPCODE_DIV, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

void rs_build_ret(rs_t *rs, rs_operand_t src)
{
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_RET, RS_OPERAND_NULL, src,
                                  RS_OPERAND_NULL, RS_OPERAND_NULL});
}

void rs_build_br(rs_t *rs, rs_operand_t src)
{
  rs_build_instr(rs, (rs_instr_t){RS_OPCODE_BR, RS_OPERAND_NULL, src,
                                  RS_OPERAND_NULL, RS_OPERAND_NULL});
}

void rs_build_br_if(rs_t *rs, rs_operand_t src1, rs_operand_t src2,
                    rs_operand_t src3)
{
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_BR_IF, RS_OPERAND_NULL, src1, src2, src3});
}

rs_operand_t rs_build_cmp_eq(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_EQ, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_cmp_lt(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_LT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

rs_operand_t rs_build_cmp_gt(rs_t *rs, rs_operand_t src1, rs_operand_t src2)
{
  rs_operand_t dst = RS_OPERAND_REG(rs->next_dst_vreg++);
  rs_build_instr(
      rs, (rs_instr_t){RS_OPCODE_CMP_GT, dst, src1, src2, RS_OPERAND_NULL});
  return dst;
}

bool rs_instr_is_terminator(rs_instr_t instr)
{
  return instr.opcode == RS_OPCODE_RET || instr.opcode == RS_OPCODE_BR ||
         instr.opcode == RS_OPCODE_BR_IF;
}

static rs_register_t rs_allocate_register(rs_t *rs)
{
  rs_register_t reg = rs_get_free_register(rs);
  if (reg == RS_REG_SPILL)
  {
    fprintf(stderr, "Error: No free registers available\n");
    return RS_REG_SPILL;
  }
  if (!is_valid_register(rs, reg))
  {
    fprintf(stderr, "Error: Invalid register allocation %d\n", reg);
    return RS_REG_SPILL;
  }
  rs->register_pool[reg] = true;
  return reg;
}

static void rs_free_register(rs_t *rs, rs_register_t reg)
{
  rs->register_pool[reg] = false;
}

rs_register_t rs_get_free_register(rs_t *rs)
{
  for (size_t i = 0; i < rs_get_register_count(rs->target); i++)
  {
    if (!rs->register_pool[i])
    {
      return i;
    }
  }
  return RS_REG_SPILL;
}

static void rs_analyze_operand(rs_t *rs, size_t i, rs_operand_t operand)
{
  rs_lifetime_t lifetime = rs->lifetimes[operand.vreg];
  lifetime.vreg = operand.vreg;
  if (lifetime.start == -1)
    lifetime.start = i;
  lifetime.end = i + 1;
  rs->lifetimes[operand.vreg] = lifetime;
}

rs_register_t rs_get_register(rs_t *rs, size_t vreg)
{
  if (vreg >= RS_MAX_REGS)
  {
    fprintf(stderr, "Error: Virtual register %zu out of bounds (max: %d)\n", vreg, RS_MAX_REGS - 1);
    return RS_REG_SPILL;
  }

  if (rs_regmap_contains(&rs->register_map, vreg))
    return rs_regmap_get(&rs->register_map, vreg);

  rs_register_t reg = rs_allocate_register(rs);
  if (reg == RS_REG_SPILL)
    return RS_REG_SPILL;
  rs_regmap_insert(&rs->register_map, vreg, reg);
  return reg;
}

size_t rs_get_register_count(rs_target_t target)
{
  switch (target)
  {
#define RS_TARGET(_, upper, count, ...) \
  case RS_TARGET_##upper:               \
    return count;
    RS_TARGETS
#undef RS_TARGET

  case RS_TARGET_COUNT:
    assert(false && "unreachable");
  }
}

const char **rs_get_register_names(rs_target_t target)
{
  switch (target)
  {
#define RS_TARGET(lower, upper, ...) \
  case RS_TARGET_##upper:            \
    return rs_target_##lower##_reg_names;
    RS_TARGETS
#undef RS_TARGET

  case RS_TARGET_COUNT:
    assert(false && "unreachable");
  }
}

static void rs_alloc_and_free_lifetimes(rs_t *rs, rs_basic_block_t *block)
{
  for (size_t ip = 0; ip < cvector_size(block->instructions); ip++)
  {
    for (size_t lifetime_index = 0; lifetime_index < 256; lifetime_index++)
    {
      rs_lifetime_t lifetime = rs->lifetimes[lifetime_index];
      if (lifetime.start == -1 || lifetime.end == -1)
        continue;

      if ((size_t)lifetime.start == ip)
      {
        rs_register_t reg = rs_get_register(rs, lifetime.vreg);
        if (reg == RS_REG_SPILL)
        {
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

void rs_analyze_lifetimes(rs_t *rs)
{
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks); block_id++)
  {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];

    for (size_t i = 0; i < cvector_size(bb->instructions); i++)
    {
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

void rs_finalize(rs_t *rs)
{
  bool has_error = false;
  for (size_t i = 0; i < cvector_size(rs->basic_blocks); i++)
  {
    rs_basic_block_t *bb = rs->basic_blocks[i];
    if (!rs_instr_is_terminator(bb->instructions[cvector_size(bb->instructions) - 1]))
    {
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

static void rs_operand_print(rs_t *rs, FILE *fp, rs_operand_t operand)
{
  (void)rs;
  switch (operand.type)
  {
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

void rs_dump_instr(rs_t *rs, FILE *fp, rs_instr_t instr)
{
  if (instr.dest.type != RS_OPERAND_TYPE_NULL)
  {
    rs_operand_print(rs, fp, instr.dest);
    fprintf(fp, " = ");
  }
  fprintf(fp, "%s ", rs_opcode_to_str(instr.opcode));
  if (instr.src1.type != RS_OPERAND_TYPE_NULL)
    rs_operand_print(rs, fp, instr.src1);
  if (instr.src2.type != RS_OPERAND_TYPE_NULL)
  {
    fprintf(fp, ", ");
    rs_operand_print(rs, fp, instr.src2);
  }
  if (instr.src3.type != RS_OPERAND_TYPE_NULL)
  {
    fprintf(fp, ", ");
    rs_operand_print(rs, fp, instr.src3);
  }
}

void rs_dump(rs_t *rs, FILE *fp)
{
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks); block_id++)
  {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    fprintf(fp, "%s:\n", bb->name);

    for (size_t i = 0; i < cvector_size(bb->instructions); i++)
    {
      rs_instr_t instr = bb->instructions[i];
      fprintf(fp, "  ");
      rs_dump_instr(rs, fp, instr);
      fprintf(fp, "\n");
    }
  }
}

void rs_generate(rs_t *rs, FILE *fp)
{
  rs_finalize(rs);
  rs_analyze_lifetimes(rs);

  switch (rs->target)
  {
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

void rs_regmap_init(rs_register_map_t *map)
{
  map->entries = NULL;
  cvector_init(map->entries, RS_REGMAP_INIT_CAPACITY, NULL);
}

void rs_regmap_free(rs_register_map_t *map)
{
  cvector_free(map->entries);
}

void rs_regmap_insert(rs_register_map_t *map, size_t key, rs_register_t value)
{
  rs_map_entry_t entry = {.key = key, .value = value};
  cvector_push_back(map->entries, entry);
}

rs_register_t rs_regmap_get(rs_register_map_t *map, size_t key)
{
  rs_map_entry_t *entry_it;
  cvector_for_each_in(entry_it, map->entries)
  {
    if (entry_it->key == key)
    {
      return entry_it->value;
    }
  }
  return RS_REG_SPILL;
}

bool rs_regmap_contains(rs_register_map_t *map, size_t key)
{
  rs_map_entry_t *entry_it;
  cvector_for_each_in(entry_it, map->entries)
  {
    if (entry_it->key == key)
    {
      return true;
    }
  }
  return false;
}

void rs_regmap_remove(rs_register_map_t *map, size_t key)
{
  rs_map_entry_t *entry_it;
  size_t i = 0;
  cvector_for_each_in(entry_it, map->entries)
  {
    if (entry_it->key == key)
    {
      cvector_erase(map->entries, i);
      return;
    }
    i++;
  }
}
