#define _POSIX_C_SOURCE 200809L
#include "runestone.h"
#include "cvector.h"
#include "cvector_utils.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool debug_enabled = false;
static FILE *debug_stream = NULL;

void rs_set_debug(bool enabled, FILE *stream) {
  debug_enabled = enabled;
  debug_stream = stream ? stream : stderr;
}

#define debug_log(...) _debug_log(__FILE__, __LINE__, __VA_ARGS__)
static void _debug_log(const char *file, int line, const char *format, ...) {
  if (!debug_enabled || !debug_stream)
    return;

  fprintf(debug_stream,
          RS_COLOR_BOLD "%s:%d: " RS_COLOR_CYAN "Debug: " RS_COLOR_RESET, file,
          line);
  va_list args;
  va_start(args, format);
  vfprintf(debug_stream, format, args);
  va_end(args);
  fprintf(debug_stream, "\n");
  fflush(debug_stream);
}

static void free_basic_block(void *ptr) {
  if (!ptr)
    return;

  rs_basic_block_t *bb = *(rs_basic_block_t **)ptr;
  if (bb) {
    debug_log("Freeing basic block '%s' with %zu instructions", bb->name,
              cvector_size(bb->instructions));
    cvector_free(bb->instructions);
    free(bb->name);
    free(bb);
  }
}

void rs_init(rs_t *rs, rs_target_t target) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  if (target >= RS_TARGET_COUNT) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                       "Invalid target %d\n",
            target);
    return;
  }

  debug_log("Initializing Runestone state for target %d", target);
  memset(rs, 0, sizeof(rs_t));
  rs->target = target;

  rs->basic_blocks = NULL;
  cvector_init(rs->basic_blocks, RS_MAX_BB, free_basic_block);
  rs->current_basic_block = -1;

  for (size_t i = 0; i < RS_MAX_REGS; i++)
    rs->lifetimes[i] =
        (rs_lifetime_t){.vreg = 0, .start = -1, .end = -1, .reg = RS_REG_SPILL};

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

void rs_free(rs_t *rs) {
  if (!rs)
    return;

  cvector_free(rs->basic_blocks);
  cvector_free(rs->register_pool);
  rs_regmap_free(&rs->register_map);
  memset(rs, 0, sizeof(rs_t));
}

size_t rs_append_basic_block(rs_t *rs, const char *name) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return SIZE_MAX;
  }

  rs_basic_block_t *bb = calloc(1, sizeof(rs_basic_block_t));
  if (!bb) {
    fprintf(stderr, "Failed to allocate memory for basic block: %s\n",
            strerror(errno));
    return SIZE_MAX;
  }

  bb->instructions = NULL;
  cvector_init(bb->instructions, RS_MAX_INSTR, NULL);

  if (name == NULL) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "bb_%zu", cvector_size(rs->basic_blocks));
    bb->name = strdup(buffer);
  } else {
    bb->name = strdup(name);
  }

  if (!bb->name) {
    fprintf(stderr, "Failed to allocate memory for basic block name: %s\n",
            strerror(errno));
    cvector_free(bb->instructions);
    free(bb);
    return SIZE_MAX;
  }

  debug_log("Appending basic block '%s'", bb->name);
  cvector_push_back(rs->basic_blocks, bb);
  return cvector_size(rs->basic_blocks) - 1;
}

void rs_position_at_basic_block(rs_t *rs, size_t block_id) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  if (block_id >= cvector_size(rs->basic_blocks)) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                       "Invalid basic block index %zu\n",
            block_id);
    return;
  }

  rs->current_basic_block = block_id;
}

static bool is_valid_register(rs_t *rs, rs_register_t reg) {
  if (!rs) {
    return (uint16_t)reg < (uint16_t)RS_MAX_REGS;
  }
  return reg < rs_get_register_count(rs->target);
}

static bool is_valid_operand(rs_t *rs, rs_operand_t operand) {
  switch (operand.type) {
  case RS_OPERAND_TYPE_NULL:
    return true;
  case RS_OPERAND_TYPE_REG:
    return (uint16_t)operand.vreg < (uint16_t)RS_MAX_REGS;
  case RS_OPERAND_TYPE_BB:
    return rs && operand.bb_id < cvector_size(rs->basic_blocks);
  case RS_OPERAND_TYPE_INT64:
  case RS_OPERAND_TYPE_ADDR:
    return true;
  default:
    return false;
  }
}

void rs_build_instr(rs_t *rs, rs_instr_t instr) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  if (rs->current_basic_block == -1) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "No basic block selected\n");
    return;
  }

  if (rs->current_basic_block >= (ptrdiff_t)cvector_size(rs->basic_blocks)) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                       "Invalid basic block index %ld\n",
            rs->current_basic_block);
    return;
  }

  rs_basic_block_t *bb = rs->basic_blocks[rs->current_basic_block];
  if (!bb) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                       "Invalid basic block at index %ld\n",
            rs->current_basic_block);
    return;
  }

  if (!is_valid_operand(rs, instr.dest) || !is_valid_operand(rs, instr.src1) ||
      !is_valid_operand(rs, instr.src2) || !is_valid_operand(rs, instr.src3)) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                       "Invalid operand in instruction %s\n",
            rs_opcode_to_str(instr.opcode));
    return;
  }

  debug_log("Building instruction %s in block '%s'",
            rs_opcode_to_str(instr.opcode), bb->name);
  cvector_push_back(bb->instructions, instr);
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

// Add register allocation hints based on instruction type
static rs_register_t rs_get_preferred_register(rs_t *rs, rs_opcode_t opcode) {
  if (!rs)
    return RS_REG_SPILL;

  // Prefer specific registers for certain operations
  switch (opcode) {
  case RS_OPCODE_ADD:
  case RS_OPCODE_SUB:
  case RS_OPCODE_MULT:
  case RS_OPCODE_DIV:
    // Prefer registers that are good for arithmetic
    for (size_t i = 0; i < rs_get_register_count(rs->target); i++) {
      if (!is_valid_register(rs, i))
        continue;
      if (!rs->register_pool[i])
        return i;
    }
    break;

  case RS_OPCODE_LOAD:
  case RS_OPCODE_STORE:
    // Prefer registers that are good for memory operations
    for (size_t i = rs_get_register_count(rs->target) - 1; i >= 0; i--) {
      if (!is_valid_register(rs, i))
        continue;
      if (!rs->register_pool[i])
        return i;
    }
    break;

  default:
    break;
  }
  return RS_REG_SPILL;
}

typedef struct {
  size_t pressure;       // Current register pressure
  size_t max_pressure;   // Maximum register pressure seen
  size_t spill_count;    // Number of spills performed
  size_t coalesce_count; // Number of coalescing opportunities found
} rs_pressure_stats_t;

static rs_pressure_stats_t pressure_stats = {0};

static void rs_track_register_pressure(rs_t *rs, rs_basic_block_t *bb) {
  if (!rs || !bb)
    return;

  size_t current_pressure = 0;
  size_t max_pressure = 0;

  // First pass: calculate pressure
  for (size_t i = 0; i < cvector_size(bb->instructions); i++) {
    // Count live registers at this point
    for (size_t j = 0; j < RS_MAX_REGS; j++) {
      rs_lifetime_t *lifetime = &rs->lifetimes[j];
      if (lifetime->start != -1 && lifetime->end != -1 &&
          (size_t)lifetime->start <= i && (size_t)lifetime->end > i) {
        current_pressure++;
      }
    }

    if (current_pressure > max_pressure) {
      max_pressure = current_pressure;
    }
  }

  pressure_stats.pressure = current_pressure;
  if (max_pressure > pressure_stats.max_pressure) {
    pressure_stats.max_pressure = max_pressure;
  }

  debug_log("Block '%s' pressure: current=%zu, max=%zu", bb->name,
            current_pressure, max_pressure);
}

// Check if two virtual registers can be coalesced
static bool rs_can_coalesce(rs_t *rs, size_t vreg1, size_t vreg2) {
  if (!rs || vreg1 >= RS_MAX_REGS || vreg2 >= RS_MAX_REGS)
    return false;

  rs_lifetime_t *lifetime1 = &rs->lifetimes[vreg1];
  rs_lifetime_t *lifetime2 = &rs->lifetimes[vreg2];

  // Check if lifetimes overlap
  if (lifetime1->start == -1 || lifetime1->end == -1 ||
      lifetime2->start == -1 || lifetime2->end == -1) {
    return false;
  }

  // If lifetimes don't overlap, we can coalesce
  return (lifetime1->end <= lifetime2->start) ||
         (lifetime2->end <= lifetime1->start);
}

// Try to coalesce registers
static void rs_try_coalesce(rs_t *rs, rs_basic_block_t *bb) {
  if (!rs || !bb)
    return;

  for (size_t i = 0; i < cvector_size(bb->instructions); i++) {
    rs_instr_t instr = bb->instructions[i];

    // Look for move instructions that can be coalesced
    if (instr.opcode == RS_OPCODE_MOVE &&
        instr.dest.type == RS_OPERAND_TYPE_REG &&
        instr.src1.type == RS_OPERAND_TYPE_REG) {

      size_t dest_vreg = instr.dest.vreg;
      size_t src_vreg = instr.src1.vreg;

      if (rs_can_coalesce(rs, dest_vreg, src_vreg)) {
        // Coalesce the registers
        rs_lifetime_t *dest_lifetime = &rs->lifetimes[dest_vreg];
        rs_lifetime_t *src_lifetime = &rs->lifetimes[src_vreg];

        // Merge lifetimes
        dest_lifetime->start = (dest_lifetime->start < src_lifetime->start)
                                   ? dest_lifetime->start
                                   : src_lifetime->start;
        dest_lifetime->end = (dest_lifetime->end > src_lifetime->end)
                                 ? dest_lifetime->end
                                 : src_lifetime->end;

        // Use the same physical register
        if (src_lifetime->reg != RS_REG_SPILL) {
          dest_lifetime->reg = src_lifetime->reg;
          rs->register_pool[src_lifetime->reg] = true;
        }

        // Clear source lifetime
        src_lifetime->start = -1;
        src_lifetime->end = -1;
        src_lifetime->reg = RS_REG_SPILL;

        pressure_stats.coalesce_count++;
        debug_log("Coalesced registers %zu and %zu", dest_vreg, src_vreg);
      }
    }
  }
}

static rs_register_t rs_allocate_register(rs_t *rs) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return RS_REG_SPILL;
  }

  // Check if we're under pressure
  size_t reg_count = rs_get_register_count(rs->target);
  if (pressure_stats.pressure >= reg_count * 0.8) { // 80% threshold
    debug_log("High register pressure detected: %zu/%zu",
              pressure_stats.pressure, reg_count);
  }

  // First try to find a completely free register
  rs_register_t reg = rs_get_free_register(rs);
  if (reg != RS_REG_SPILL) {
    if (!is_valid_register(rs, reg)) {
      fprintf(stderr,
              RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                         "Invalid register allocation %d\n",
              reg);
      return RS_REG_SPILL;
    }
    debug_log("Found free register %d", reg);
    rs->register_pool[reg] = true;
    return reg;
  }

  // If no free registers, try to find a register whose lifetime has ended
  static size_t last_checked_reg = 0;
  size_t start_reg = last_checked_reg;

  do {
    if (!is_valid_register(rs, last_checked_reg))
      continue;

    bool is_used = false;
    for (size_t j = 0; j < RS_MAX_REGS; j++) {
      rs_lifetime_t *lifetime = &rs->lifetimes[j];
      if (lifetime->start != -1 && lifetime->end != -1 &&
          lifetime->reg == last_checked_reg &&
          lifetime->end > lifetime->start) {
        is_used = true;
        break;
      }
    }
    if (!is_used) {
      debug_log("Reusing register %zu", last_checked_reg);
      rs->register_pool[last_checked_reg] = true;
      return last_checked_reg;
    }

    last_checked_reg = (last_checked_reg + 1) % reg_count;
  } while (last_checked_reg != start_reg);

  // If still no registers available, try to spill the least recently used
  // register
  size_t lru_reg = 0;
  ptrdiff_t lru_end = -1;

  for (size_t i = 0; i < reg_count; i++) {
    if (!is_valid_register(rs, i))
      continue;

    for (size_t j = 0; j < RS_MAX_REGS; j++) {
      rs_lifetime_t *lifetime = &rs->lifetimes[j];
      if (lifetime->start != -1 && lifetime->end != -1 && lifetime->reg == i &&
          lifetime->end > lru_end) {
        lru_reg = i;
        lru_end = lifetime->end;
      }
    }
  }

  if (lru_end > 0) {
    debug_log("Spilling register %zu", lru_reg);
    rs->register_pool[lru_reg] = true;
    pressure_stats.spill_count++;
    return lru_reg;
  }

  fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
          "Error: " RS_COLOR_RESET "No registers available for allocation\n");
  return RS_REG_SPILL;
}

static void rs_free_register(rs_t *rs, rs_register_t reg) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  if (!is_valid_register(rs, reg)) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "Attempting to free invalid register %d\n",
            reg);
    return;
  }

  // Check if register is actually allocated
  if (!rs->register_pool[reg]) {
    fprintf(stderr, "Warning: Attempting to free unallocated register %d\n",
            reg);
    return;
  }

  rs->register_pool[reg] = false;
  debug_log("Freed register %d", reg);
}

rs_register_t rs_get_free_register(rs_t *rs) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return RS_REG_SPILL;
  }

  for (size_t i = 0; i < rs_get_register_count(rs->target); i++) {
    if (!is_valid_register(rs, i))
      continue;
    if (!rs->register_pool[i]) {
      debug_log("Found free register %zu", i);
      return i;
    }
  }
  return RS_REG_SPILL;
}

rs_register_t rs_get_register(rs_t *rs, size_t vreg) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return RS_REG_SPILL;
  }

  if (vreg >= RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %zu out of bounds (max: %d)\n",
            vreg, RS_MAX_REGS - 1);
    return RS_REG_SPILL;
  }

  if (rs_regmap_contains(&rs->register_map, vreg)) {
    rs_register_t reg = rs_regmap_get(&rs->register_map, vreg);
    debug_log("Found existing mapping for vreg %zu -> preg %d", vreg, reg);
    return reg;
  }

  rs_register_t reg = rs_allocate_register(rs);
  if (reg == RS_REG_SPILL) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Failed to allocate register for vreg %zu\n",
            vreg);
    return RS_REG_SPILL;
  }

  rs_regmap_insert(&rs->register_map, vreg, reg);
  debug_log("Created new mapping for vreg %zu -> preg %d", vreg, reg);
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
  if (!rs || !block) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Invalid parameters for lifetime analysis\n");
    return;
  }

  debug_log("Analyzing lifetimes in block '%s'", block->name);

  for (size_t ip = 0; ip < cvector_size(block->instructions); ip++) {
    // First free registers that are no longer needed
    for (size_t lifetime_index = 0; lifetime_index < RS_MAX_REGS;
         lifetime_index++) {
      rs_lifetime_t *lifetime = &rs->lifetimes[lifetime_index];
      if (lifetime->start == -1 || lifetime->end == -1)
        continue;

      if ((size_t)lifetime->end == ip) {
        rs_free_register(rs, lifetime->reg);
        debug_log("Freed register %d for vreg %zu at instruction %zu",
                  lifetime->reg, lifetime_index, ip);
      }
    }

    // Then allocate registers for new values
    for (size_t lifetime_index = 0; lifetime_index < RS_MAX_REGS;
         lifetime_index++) {
      rs_lifetime_t *lifetime = &rs->lifetimes[lifetime_index];
      if (lifetime->start == -1 || lifetime->end == -1)
        continue;

      if ((size_t)lifetime->start == ip) {
        rs_register_t reg = rs_get_register(rs, lifetime->vreg);
        if (reg == RS_REG_SPILL) {
          fprintf(stderr,
                  RS_COLOR_RED RS_COLOR_BOLD
                  "Error: " RS_COLOR_RESET
                  "Register allocation failed for vreg %zu at "
                  "instruction %zu\n",
                  lifetime_index, ip);
          return;
        }

        rs_regmap_insert(&rs->register_map, lifetime_index, reg);
        lifetime->reg = reg;
        debug_log("Allocated register %d for vreg %zu at instruction %zu", reg,
                  lifetime_index, ip);
      }
    }
  }
}

static void rs_analyze_operand(rs_t *rs, size_t i, rs_operand_t operand,
                               rs_opcode_t opcode) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  if (operand.type != RS_OPERAND_TYPE_REG)
    return;

  if ((uint16_t)operand.vreg >= (uint16_t)RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %d out of bounds (max: %d)\n",
            operand.vreg, RS_MAX_REGS - 1);
    return;
  }

  rs_lifetime_t *lifetime = &rs->lifetimes[operand.vreg];
  lifetime->vreg = operand.vreg;

  // Optimize lifetime tracking by only updating when necessary
  if (lifetime->start == -1) {
    lifetime->start = i;
    // Try to get a preferred register based on the operation
    rs_register_t preferred_reg = rs_get_preferred_register(rs, opcode);
    if (preferred_reg != RS_REG_SPILL) {
      lifetime->reg = preferred_reg;
      rs->register_pool[preferred_reg] = true;
    }
  }
  if ((ptrdiff_t)(i + 1) > lifetime->end) {
    lifetime->end = i + 1;
  }

  debug_log("Updated lifetime for vreg %d: start=%d, end=%d", operand.vreg,
            lifetime->start, lifetime->end);
}

void rs_analyze_lifetimes(rs_t *rs) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  memset(&pressure_stats, 0, sizeof(pressure_stats));

  memset(rs->lifetimes, 0, sizeof(rs->lifetimes));
  for (size_t i = 0; i < RS_MAX_REGS; i++) {
    rs->lifetimes[i].start = -1;
    rs->lifetimes[i].end = -1;
    rs->lifetimes[i].reg = RS_REG_SPILL;
  }

  // Reset register pool using memset
  memset(rs->register_pool, 0, cvector_size(rs->register_pool) * sizeof(bool));

  // Clear register map
  rs_regmap_free(&rs->register_map);
  rs_regmap_init(&rs->register_map);

  debug_log("Starting lifetime analysis");

  // First pass: analyze all lifetimes
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks);
       block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    if (!bb) {
      fprintf(stderr,
              RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                         "Invalid basic block at index %zu\n",
              block_id);
      continue;
    }

    debug_log("Analyzing lifetimes in block '%s'", bb->name);

    // Process all instructions in the block
    for (size_t i = 0; i < cvector_size(bb->instructions); i++) {
      rs_instr_t instr = bb->instructions[i];

      // Process all operands in a single loop
      rs_operand_t operands[] = {instr.dest, instr.src1, instr.src2,
                                 instr.src3};
      for (size_t j = 0; j < 4; j++) {
        if (operands[j].type == RS_OPERAND_TYPE_REG) {
          rs_analyze_operand(rs, i, operands[j], instr.opcode);
        }
      }
    }

    rs_track_register_pressure(rs, bb);
  }

  // Try to coalesce registers
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks);
       block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    if (!bb)
      continue;
    rs_try_coalesce(rs, bb);
  }

  // Second pass: allocate registers
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks);
       block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    if (!bb)
      continue;
    rs_alloc_and_free_lifetimes(rs, bb);
  }

  debug_log("Lifetime analysis complete");
  debug_log("Register pressure stats: max=%zu, spills=%zu, coalesces=%zu",
            pressure_stats.max_pressure, pressure_stats.spill_count,
            pressure_stats.coalesce_count);
}

void rs_finalize(rs_t *rs) {
  if (!rs) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET "NULL Runestone state pointer\n");
    return;
  }

  bool has_error = false;
  for (size_t i = 0; i < cvector_size(rs->basic_blocks); i++) {
    rs_basic_block_t *bb = rs->basic_blocks[i];
    if (!bb) {
      fprintf(stderr,
              RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                         "Invalid basic block at index %zu\n",
              i);
      has_error = true;
      continue;
    }

    if (cvector_size(bb->instructions) == 0) {
      fprintf(stderr,
              RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                         "Empty basic block '%s'\n",
              bb->name);
      has_error = true;
      continue;
    }

    if (!rs_instr_is_terminator(
            bb->instructions[cvector_size(bb->instructions) - 1])) {
      fprintf(stderr,
              RS_COLOR_RED RS_COLOR_BOLD
              "Error: " RS_COLOR_RESET
              "Missing terminator instruction in basic block '%s'\n",
              bb->name);
      has_error = true;
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
  for (size_t block_id = 0; block_id < cvector_size(rs->basic_blocks);
       block_id++) {
    rs_basic_block_t *bb = rs->basic_blocks[block_id];
    fprintf(fp, "%s:\n", bb->name);

    for (size_t i = 0; i < cvector_size(bb->instructions); i++) {
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
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return;
  }

  map->entries = NULL;
  cvector_init(map->entries, RS_REGMAP_INIT_CAPACITY, NULL);
  debug_log("Initialized register map with capacity %d",
            RS_REGMAP_INIT_CAPACITY);
}

void rs_regmap_free(rs_register_map_t *map) {
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return;
  }

  cvector_free(map->entries);
  debug_log("Freed register map");
}

void rs_regmap_insert(rs_register_map_t *map, size_t key, rs_register_t value) {
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return;
  }

  if (key >= RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %zu out of bounds (max: %d)\n",
            key, RS_MAX_REGS - 1);
    return;
  }

  if ((uint16_t)value >= (uint16_t)RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Physical register %d out of bounds (max: %d)\n",
            value, RS_MAX_REGS - 1);
    return;
  }

  // Remove existing mapping if any
  rs_regmap_remove(map, key);

  rs_map_entry_t entry = {.key = key, .value = value};
  cvector_push_back(map->entries, entry);
  debug_log("Inserted mapping vreg %zu -> preg %d", key, value);
}

rs_register_t rs_regmap_get(rs_register_map_t *map, size_t key) {
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return RS_REG_SPILL;
  }

  if (key >= RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %zu out of bounds (max: %d)\n",
            key, RS_MAX_REGS - 1);
    return RS_REG_SPILL;
  }

  rs_map_entry_t *entry_it;
  cvector_for_each_in(entry_it, map->entries) {
    if (entry_it->key == key) {
      debug_log("Found mapping vreg %zu -> preg %d", key, entry_it->value);
      return entry_it->value;
    }
  }

  debug_log("No mapping found for vreg %zu", key);
  return RS_REG_SPILL;
}

bool rs_regmap_contains(rs_register_map_t *map, size_t key) {
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return false;
  }

  if (key >= RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %zu out of bounds (max: %d)\n",
            key, RS_MAX_REGS - 1);
    return false;
  }

  rs_map_entry_t *entry_it;
  cvector_for_each_in(entry_it, map->entries) {
    if (entry_it->key == key) {
      debug_log("Found mapping for vreg %zu", key);
      return true;
    }
  }

  debug_log("No mapping found for vreg %zu", key);
  return false;
}

void rs_regmap_remove(rs_register_map_t *map, size_t key) {
  if (!map) {
    fprintf(stderr, RS_COLOR_RED RS_COLOR_BOLD "Error: " RS_COLOR_RESET
                                               "NULL register map pointer\n");
    return;
  }

  if (key >= RS_MAX_REGS) {
    fprintf(stderr,
            RS_COLOR_RED RS_COLOR_BOLD
            "Error: " RS_COLOR_RESET
            "Virtual register %zu out of bounds (max: %d)\n",
            key, RS_MAX_REGS - 1);
    return;
  }

  rs_map_entry_t *entry_it;
  size_t i = 0;
  cvector_for_each_in(entry_it, map->entries) {
    if (entry_it->key == key) {
      debug_log("Removing mapping vreg %zu -> preg %d", key, entry_it->value);
      cvector_erase(map->entries, i);
      return;
    }
    i++;
  }
}
