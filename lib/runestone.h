/**
 * @file runestone.h
 * @brief Public interface for the Runestone intermediate representation (IR)
 *        and code generation infrastructure.
 * @details Defines instruction encoding, operand types, register lifetimes,
 *          code generation targets, and utility functions.
 */

#ifndef RUNESTONE_H
#define RUNESTONE_H

#include "cvector.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/**
 * @defgroup Runestone Runestone IR API
 * @brief Core types and functions for the Runestone IR.
 * @{
 */

/** Define an ANSI code for terminal color. */
#define _RS_ANSI(CODE) "\033[" #CODE "m"

/** Reset terminal color. */
#define RS_COLOR_RESET _RS_ANSI(0)

/** Red color for terminal output. */
#define RS_COLOR_RED _RS_ANSI(31)

/** Green color for terminal output. */
#define RS_COLOR_GREEN _RS_ANSI(32)

/** Yellow color for terminal output. */
#define RS_COLOR_YELLOW _RS_ANSI(33)

/** Blue color for terminal output. */
#define RS_COLOR_BLUE _RS_ANSI(34)

/** Magenta color for terminal output. */
#define RS_COLOR_MAGENTA _RS_ANSI(35)

/** Cyan color for terminal output. */
#define RS_COLOR_CYAN _RS_ANSI(36)

/** White color for terminal output. */
#define RS_COLOR_WHITE _RS_ANSI(37)

/** Bold text for terminal output. */
#define RS_COLOR_BOLD _RS_ANSI(1)

/** Underlined text for terminal output. */
#define RS_COLOR_UNDERLINE _RS_ANSI(4)

// Configuration constants for the Runestone IR.

/** Maximum number of instructions. */
#define RS_MAX_INSTR 1024
/** Maximum number of basic blocks. */
#define RS_MAX_BB 1024
/** Maximum number of registers. */
#define RS_MAX_REGS 256
/** Initial capacity for register map. */
#define RS_REGMAP_INIT_CAPACITY 16

/** Value representing an invalid virtual register. */
#define RS_INVALID_VREG UINT8_MAX
/** Placeholder for spilled registers.  */
#define RS_REG_SPILL RS_INVALID_VREG
/** Placeholder for temporary registers.  */
#define RS_TEMPORARY_VREG RS_INVALID_VREG

typedef uint8_t rs_register_t;

/**
 * @brief Macro for defining the available operand types.
 *
 * This macro defines the various operand types used in Runestone IR
 * instructions. It is used to generate the `rs_operand_type_t` enum and to
 * define constructor macros for different operand types.
 */
#define RS_OPERAND_TYPES(X)                                                    \
  X(NULL)  /**< No operand (void). */                                          \
  X(INT64) /**< Immediate 64-bit integer. */                                   \
  X(ADDR)  /**< Address (e.g., label or absolute address). */                  \
  X(REG)   /**< Virtual register. */                                           \
  X(BB)    /**< Basic block reference. */

/**
 * @enum rs_operand_type_t
 * @brief Enumerates the possible operand types for an instruction.
 *
 * This enum defines the types of operands that can be used in Runestone IR
 * instructions, such as integer constants, virtual registers, addresses, and
 * basic block references.
 */
typedef enum {

#define X(name) RS_OPERAND_TYPE_##name,
  RS_OPERAND_TYPES(X)
#undef X

      RS_OPERAND_TYPE_COUNT /**< Number of operand types. */
} rs_operand_type_t;

/**
 * @struct rs_operand_t
 * @brief Represents a polymorphic operand used in instructions.
 *
 * This structure represents an operand in the Runestone IR. The operand type
 * is stored in the `type` field, and the actual value of the operand is stored
 * in the union, which can hold an integer, address, register, or basic block
 * ID.
 */
typedef struct {
  rs_operand_type_t type; /**< The type of the operand. */
  union {
    int64_t int64; /**< 64-bit integer value. */
    size_t addr;   /**< Address. */
    uint8_t vreg;  /**< Virtual register index. */
    size_t bb_id;  /**< Basic block ID. */
  };
} rs_operand_t;

/// @brief Operand constructor for null value.
#define RS_OPERAND_NULL ((rs_operand_t){.type = RS_OPERAND_TYPE_NULL})

/// @brief Operand constructor for a 64-bit integer value.
#define RS_OPERAND_INT64(value)                                                \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_INT64, .int64 = (value)})

/// @brief Operand constructor for address.
#define RS_OPERAND_ADDR(value)                                                 \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_ADDR, .addr = (value)})

/// @brief Operand constructor for register.
#define RS_OPERAND_REG(value)                                                  \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_REG, .vreg = (value)})

/// @brief Operand constructor for basic block.
#define RS_OPERAND_BB(value)                                                   \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_BB, .bb_id = (value)})

/**
 * @enum rs_opcode_t
 * @brief The available instructions in the Runestone IR.
 *
 * This enum defines the available opcodes (or instructions) in the Runestone
 * IR.
 * Each opcode corresponds to a basic operation that can be performed in the
 * intermediate representation, such as move, load, add, and branch.
 */
#define RS_OPCODES(X)                                                          \
  X(MOVE, "move")     /**< Move opcode. */                                     \
  X(COPY, "copy")     /**< Copy opcode. */                                     \
  X(LOAD, "load")     /**< Load opcode. */                                     \
  X(STORE, "store")   /**< Store opcode. */                                    \
  X(ADD, "add")       /**< Add opcode. */                                      \
  X(SUB, "sub")       /**< Sub opcode. */                                      \
  X(MULT, "mult")     /**< Mult opcode. */                                     \
  X(DIV, "div")       /**< Div opcode. */                                      \
  X(RET, "ret")       /**< Return opcode. */                                   \
  X(BR, "br")         /**< Branch opcode. */                                   \
  X(BR_IF, "br_if")   /**< Branch if value != 0. */                            \
  X(CMP_EQ, "cmp_eq") /**< result = (a == b) */                                \
  X(CMP_LT, "cmp_lt") /**< result = (a < b) */                                 \
  X(CMP_GT, "cmp_gt") /**< result = (a > b) */

/**
 * @enum rs_opcode_t
 * @brief The available instructions in the Runestone IR.
 */
typedef enum {
#define X(name, str) RS_OPCODE_##name,
  RS_OPCODES(X)
#undef X
      RS_OPCODE_COUNT /**< Number of defined opcodes. */
} rs_opcode_t;

/**
 * @brief Converts an opcode enum value to its corresponding string
 * representation.
 * @details This function maps an `rs_opcode_t` enumeration value to its
 * human-readable string name, such as "move", "add", or "ret". These names are
 * used for debugging, diagnostics, or assembly output.
 * @param[in] opcode The opcode to convert. Must be a valid `rs_opcode_t` value.
 * @return A pointer to a constant string representing the opcode name.
 * If the input value is out of range, the string "unknown" is returned.
 */
static inline const char *rs_opcode_to_str(rs_opcode_t opcode) {
  /// @cond
  static const char *names[] = {
#define X(name, str) [RS_OPCODE_##name] = str,
      RS_OPCODES(X)
#undef X
  };
  /// @endcond
  return (opcode < RS_OPCODE_COUNT) ? names[opcode] : "unknown";
}

/**
 * @struct rs_instr_t
 * @brief Represents a Runestone instruction.
 *
 * This structure represents an instruction in the Runestone IR. It contains
 * the opcode (operation to perform) and up to three operands (destination,
 * and two source operands).
 */
typedef struct {
  rs_opcode_t opcode; /**< The opcode of the instruction. */
  rs_operand_t dest;  /**< Operand for destination . */
  rs_operand_t src1;  /**< Operand for first source. */
  rs_operand_t src2;  /**< Operand for second source. */
  rs_operand_t src3;  /**< Operand for third source. */
} rs_instr_t;

typedef cvector(rs_instr_t) rs_instructions_t;

/**
 * @struct rs_basic_block_t
 * @brief Represents a basic block in the Runestone IR.
 *
 * A basic block is a sequence of instructions that is executed sequentially.
 * This structure contains information about the block's name (optional),
 * the instructions it contains, and the number of instructions.
 */
typedef struct {
  char *name; /**< Optional name of the block. This is used for debugging or
                 labeling purposes. */
  rs_instructions_t instructions; /**< List of instructions in the basic block.
                         The instructions are executed sequentially. */
} rs_basic_block_t;

typedef cvector(rs_basic_block_t *) rs_basic_blocks_t;
typedef cvector(bool) rs_register_pool_t;

/**
 * @struct rs_lifetime_t
 * @brief Represents the lifetime of a virtual register in the Runestone IR.
 *
 * This structure tracks the start and end points of a virtual register's usage.
 * A virtual register is assigned a physical register during allocation, and its
 * lifetime is tracked across basic blocks and instructions.
 */
typedef struct {
  rs_register_t
      reg; /**< Physical register assigned to the virtual register. This maps a
              virtual register to a real hardware register. */
  uint8_t vreg; /**< The index of the virtual register. This uniquely identifies
                   the virtual register within the system. */
  ptrdiff_t start; /**< The index of the first instruction in the block where
                      the virtual register is used. */
  ptrdiff_t end;   /**< The index of the last instruction in the block where the
                      virtual register is used. */
} rs_lifetime_t;

/**
 * @brief Structure representing a mapping entry between a virtual and physical
 * register.
 *
 * This structure defines a single entry in the register map, associating a
 * virtual register ID (`key`) with a physical register (`value`).
 */
typedef struct {
  size_t key;          /**< Virtual register ID. */
  rs_register_t value; /**< Assigned physical register. */
} rs_map_entry_t;

typedef cvector(rs_map_entry_t) rs_map_entries_t;

/**
 * @brief Structure representing the register map.
 *
 * The `rs_register_map_t` structure holds a list of register mappings,
 * maintaining the mappings between virtual registers and their assigned
 * physical registers.
 */
typedef struct {
  rs_map_entries_t entries; /**< List of register mappings. */
} rs_register_map_t;

/**
 * @brief Initialize a register map.
 *
 * This function initializes a register map, allocating memory for the map
 * entries and setting the count and capacity.
 *
 * @param[inout] map Pointer to the register map to initialize.
 *
 * @note The map should be freed later using `rs_regmap_free`.
 */
void rs_regmap_init(rs_register_map_t *map);

/**
 * @brief Free the memory used by the register map.
 *
 * This function frees the memory allocated for the entries in the register
 * map and resets the map to an uninitialized state.
 *
 * @param[inout] map Pointer to the register map to free.
 */
void rs_regmap_free(rs_register_map_t *map);

/**
 * @brief Insert a mapping between a virtual and physical register.
 *
 * This function inserts a new entry into the register map, associating a
 * virtual register ID (`key`) with a physical register (`value`).
 *
 * @param[inout] map Pointer to the register map where the entry will be
 * inserted.
 * @param[in] key The virtual register ID to insert.
 * @param[in] value The physical register to associate with the virtual
 * register.
 */
void rs_regmap_insert(rs_register_map_t *map, size_t key, rs_register_t value);

/**
 * @brief Retrieve the physical register associated with a virtual register.
 *
 * This function returns the physical register associated with a given
 * virtual register ID (`key`).
 *
 * @param[inout] map Pointer to the register map.
 * @param[in] key The virtual register ID whose physical register is to be
 * retrieved.
 * @return The physical register associated with the given virtual register ID.
 *
 * @note If the key does not exist in the map, the behavior is undefined.
 */
rs_register_t rs_regmap_get(rs_register_map_t *map, size_t key);

/**
 * @brief Check if a virtual register ID exists in the register map.
 *
 * This function checks if a mapping exists for the given virtual register ID
 * (`key`).
 *
 * @param[inout] map Pointer to the register map.
 * @param[in] key The virtual register ID to check for.
 * @return `true` if the key exists in the map, `false` otherwise.
 */
bool rs_regmap_contains(rs_register_map_t *map, size_t key);

/**
 * @brief Remove a mapping for a given virtual register ID.
 *
 * This function removes the entry associated with the given virtual register ID
 * (`key`) from the register map.
 *
 * @param[inout] map Pointer to the register map.
 * @param[in] key The virtual register ID whose mapping should be removed.
 */
void rs_regmap_remove(rs_register_map_t *map, size_t key);

/**
 * @brief Define the supported targets and their associated register sets.
 *
 * This macro, `RS_TARGETS`, defines the output targets in the Runestone
 * project. Each target represents a specific combination of architecture,
 * operating system, and assembler, and includes a list of general-purpose
 * registers associated with that target. These registers are used for code
 * generation in assembly format, and each target is configured for a specific
 * assembly syntax (e.g., NASM for x86_64 on Linux, GAS for AArch64 on macOS).
 *
 * @note The list of registers includes general-purpose registers used for
 * assembly operations. Each target may support a different number of registers.
 *
 * @see RS_TARGET
 */
#define RS_TARGETS                                                             \
  RS_TARGET(x86_64_linux_nasm, X86_64_LINUX_NASM, 14, "rax", "rbx", "rcx",     \
            "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13",       \
            "r14", "r15")                                                      \
  RS_TARGET(aarch64_macos_gas, AARCH64_MACOS_GAS, 17, "x9", "x10", "x11",      \
            "x12", "x13", "x14", "x15", "x19", "x20", "x21", "x22", "x23",     \
            "x24", "x25", "x26", "x27", "x28")

/**
 * @brief Define a target's register set.
 *
 * This macro defines the set of register names for a specific target. It
 * associates a target identifier with a list of register names, which will
 * be used during the code generation process to generate assembly code for
 * that target.
 *
 * @param lower The lowercase identifier for the target (e.g.,
 * `x86_64_linux_nasm`).
 * @param count The total number of registers available for the target.
 * @param ... A list of register names that are available for the target.
 *
 * Example usage:
 * @code
 * RS_TARGET(x86_64_linux_nasm, X86_64_LINUX_NASM, 14, "rax", "rbx", ...);
 * @endcode
 */
#define RS_TARGET(lower, _, count, ...)                                        \
  /** Register names for target `lower`.*/                                     \
  static const char *rs_target_##lower##_reg_names[] = {__VA_ARGS__};
RS_TARGETS
#undef RS_TARGET

/**
 * @brief Enumeration of available code generation targets.
 *
 * This enumeration defines the supported Runestone output targets. Each
 * target corresponds to a unique combination of architecture, operating
 * system, and assembler syntax, and is used to generate the appropriate
 * assembly instructions for the selected target.
 *
 * The list of available targets is automatically generated by the `RS_TARGETS`
 * macro.
 *
 * @note The value `RS_TARGET_COUNT` represents the total number of targets
 * available for code generation.
 *
 * @see RS_TARGETS
 */
typedef enum {

#define RS_TARGET(_1, x, _2, ...) RS_TARGET_##x,
  RS_TARGETS
#undef RS_TARGET

      RS_TARGET_COUNT, /**< Total number of supported targets. */
} rs_target_t;

size_t rs_get_register_count(rs_target_t target);
const char **rs_get_register_names(rs_target_t target);

/**
 * @struct rs_t
 * @brief Represents the entire state of the Runestone IR, including target,
 * basic blocks, virtual registers, and the register map.
 *
 * This structure holds the main data for managing the IR, including basic block
 * sequences, register allocation, and the target architecture for code
 * generation.
 */
typedef struct {
  rs_target_t target; /**< The currently selected target architecture. */

  rs_basic_blocks_t basic_blocks; /**< List of all the basic blocks. */
  ptrdiff_t current_basic_block;  /**< Index of the currently selected basic
                                     block, -1 if no block is selected. */

  rs_lifetime_t lifetimes[256]; /**< Array of virtual register lifetimes, used
                                   for register allocation. */

  rs_register_pool_t register_pool; /**< Array of usable hardware registers,
                          representing free and used states. */

  rs_register_map_t
      register_map; /**< The register map used during register allocation. */

  size_t stack_size; /**< The size of the stack. */

  size_t next_dst_vreg; /**< The index for the next destination virtual
                           register. */
} rs_t;

/**
 * @brief Sets debug logging options.
 * @param[in] enabled Whether debug logging should be enabled.
 * @param[in] stream The file stream to write debug messages to (NULL for
 * stderr).
 */
void rs_set_debug(bool enabled, FILE *stream);

/**
 * @brief Initializes the Runestone IR state.
 * @param[inout] rs The Runestone state to initialize.
 * @param[in] target The target architecture for the IR.
 */
void rs_init(rs_t *rs, rs_target_t target);

/**
 * @brief Frees the resources used by the Runestone IR state.
 * @param[inout] rs The Runestone state to free.
 */
void rs_free(rs_t *rs);

/**
 * @brief Appends a new basic block to the IR.
 * @param[inout] rs The Runestone state.
 * @param[in] name The name of the basic block, or NULL to auto-assign.
 * @return The ID of the newly added basic block.
 */
size_t rs_append_basic_block(rs_t *rs, const char *name);

/**
 * @brief Positions the cursor at the specified basic block.
 * @param[inout] rs The Runestone state.
 * @param[in] block_id The ID of the block to position at.
 */
void rs_position_at_basic_block(rs_t *rs, size_t block_id);

/**
 * @brief Builds an instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] instruction The instruction.
 */
void rs_build_instruction(rs_t *rs, rs_instr_t instruction);

/**
 * @brief Builds a move instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src The source operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_move(rs_t *rs, rs_operand_t src);

/**
 * @brief Builds a copy instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src The source operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_copy(rs_t *rs, rs_operand_t src);

/**
 * @brief Builds a load instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src The source operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_load(rs_t *rs, rs_operand_t src);

/**
 * @brief Builds a store instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The source operand for the value.
 * @param[in] src2 The source operand for the address.
 */
void rs_build_store(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds an add instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_add(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a subtract instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_sub(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a multiply instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_mult(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a divide instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_div(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a return instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src The operand to return.
 */
void rs_build_ret(rs_t *rs, rs_operand_t src);

/**
 * @brief Builds a branch instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src The operand representing the target of the branch.
 */
void rs_build_br(rs_t *rs, rs_operand_t src);

/**
 * @brief Builds a conditional branch instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The operand to compare.
 * @param[in] src2 The operand representing the target of the branch if the
 * condition is met.
 * @param[in] src3 The operand representing the target of the branch if the
 * condition is not met.
 */
void rs_build_br_if(rs_t *rs, rs_operand_t src1, rs_operand_t src2,
                    rs_operand_t src3);

/**
 * @brief Builds a equiality comparison instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_cmp_eq(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a less-than comparison instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_cmp_lt(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Builds a greater-than comparison instruction.
 * @param[inout] rs The Runestone state.
 * @param[in] src1 The first operand.
 * @param[in] src2 The second operand.
 * @return The operand used as the destination in the instruction.
 */
rs_operand_t rs_build_cmp_gt(rs_t *rs, rs_operand_t src1, rs_operand_t src2);

/**
 * @brief Checks if an instruction is a terminator.
 * @param[in] instr The instruction to check.
 * @return True if the instruction is a terminator (e.g., branch or return).
 */
bool rs_instr_is_terminator(rs_instr_t instr);

/**
 * @brief Gets a free hardware register from the register pool.
 * @param[inout] rs The Runestone state.
 * @return The index of a free register.
 */
rs_register_t rs_get_free_register(rs_t *rs);
/**
 * @brief Gets the physical register assigned to a virtual register.
 * @param[inout] rs The Runestone state.
 * @param[in] vreg The index of the virtual register.
 * @return The physical register assigned to the virtual register.
 */
rs_register_t rs_get_register(rs_t *rs, size_t vreg);

/**
 * @brief Analyzes and determines the lifetimes of virtual registers for
 * allocation.
 * @param[inout] rs The Runestone state.
 */
void rs_analyze_lifetimes(rs_t *rs);

/**
 * @brief Finalizes the given Runestone instance.
 *
 * This function performs any necessary cleanup and resource deallocation for
 * the given Runestone instance. It is typically called when the Runestone
 * instance is no longer needed to ensure all resources are properly freed and
 * any other finalization tasks are completed.
 *
 * @param rs A pointer to the Runestone instance that is to be finalized.
 */
void rs_finalize(rs_t *rs);

/**
 * @brief Dumps the disassembled representation of a single instruction.
 *
 * This function formats and outputs the disassembled representation of a single
 * instruction from the given Runestone instance. The instruction is written to
 * the provided file stream.
 *
 * @param rs A pointer to the Runestone instance containing the instruction.
 * @param fp A pointer to the `FILE` object where the disassembled instruction
 *           will be written.
 * @param instr The instruction to be dumped, which is part of the Runestone
 *              instance.
 */
void rs_dump_instr(rs_t *rs, FILE *fp, rs_instr_t instr);
/**
 * @brief Dumps the entire Runestone instance.
 *
 * This function outputs a complete dump of the Runestone instance to the
 * specified file stream, including its internal state, instructions, and any
 * relevant information for debugging or analysis.
 *
 * @param rs A pointer to the Runestone instance to be dumped.
 * @param fp A pointer to the `FILE` object where the dump will be written.
 */
void rs_dump(rs_t *rs, FILE *fp);

/**
 * @brief Generates target-specific code.
 * @param[inout] rs The Runestone state.
 * @param[out] fp The file to write the generated code to.
 */
void rs_generate(rs_t *rs, FILE *fp);

#define RS_TARGET(lower, ...)                                                  \
  /**                                                                          \
    @brief Generates code for for target `lower`.                              \
    @param[inout] rs The Runestone state.                                      \
    @param[out] fp The file to write the generated code to.                    \
   */                                                                          \
  void rs_generate_##lower(rs_t *rs, FILE *fp);                                \
  /**                                                                          \
    @brief Generates an instruction for target `lower`.                        \
    @param[inout] rs The Runestone state.                                      \
    @param[out] fp The file to write the instruction to.                       \
    @param[in] instr The instruction to generate.                              \
   */                                                                          \
  void rs_generate_instr_##lower(rs_t *rs, FILE *fp, rs_instr_t instr);        \
  /**                                                                          \
    @brief Generates an operand for for target `lower`.                        \
    @param[inout] rs The Runestone state.                                      \
    @param[out] fp The file to write the operand to.                           \
    @param[in] operand The operand to generate.                                \
    @param[in] dereference Whether to dereference the operand.                 \
   */                                                                          \
  void rs_generate_operand_##lower(rs_t *rs, FILE *fp, rs_operand_t operand,   \
                                   bool dereference);
RS_TARGETS
#undef RS_TARGET

/**
 * @}
 */

#endif // RUNESTONE_H
