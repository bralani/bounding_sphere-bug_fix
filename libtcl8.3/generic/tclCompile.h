/*
 * tclCompile.h --
 *
 * Copyright (c) 1996-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TCLCOMPILATION
#define _TCLCOMPILATION 1

#ifndef _TCLINT
#include "tclInt.h"
#endif /* _TCLINT */

#ifdef BUILD_tcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 *------------------------------------------------------------------------
 * Variables related to compilation. These are used in tclCompile.c,
 * tclExecute.c, tclBasic.c, and their clients.
 *------------------------------------------------------------------------
 */

/*
 * Variable that denotes the command name Tcl object type. Objects of this
 * type cache the Command pointer that results from looking up command names
 * in the command hashtable.
 */

extern Tcl_ObjType	tclCmdNameType;

/*
 * Variable that controls whether compilation tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no compilation tracing
 *    1: summarize compilation of top level cmds and proc bodies
 *    2: display all instructions of each ByteCode compiled
 * This variable is linked to the Tcl variable "tcl_traceCompile".
 */

extern int 		tclTraceCompile;

/*
 * Variable that controls whether execution tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no execution tracing
 *    1: trace invocations of Tcl procs only
 *    2: trace invocations of all (not compiled away) commands
 *    3: display each instruction executed
 * This variable is linked to the Tcl variable "tcl_traceExec".
 */

extern int 		tclTraceExec;

/*
 *------------------------------------------------------------------------
 * Data structures related to compilation.
 *------------------------------------------------------------------------
 */

/*
 * The structure used to implement Tcl "exceptions" (exceptional returns):
 * for example, those generated in loops by the break and continue commands,
 * and those generated by scripts and caught by the catch command. This
 * ExceptionRange structure describes a range of code (e.g., a loop body),
 * the kind of exceptions (e.g., a break or continue) that might occur, and
 * the PC offsets to jump to if a matching exception does occur. Exception
 * ranges can nest so this structure includes a nesting level that is used
 * at runtime to find the closest exception range surrounding a PC. For
 * example, when a break command is executed, the ExceptionRange structure
 * for the most deeply nested loop, if any, is found and used. These
 * structures are also generated for the "next" subcommands of for loops
 * since a break there terminates the for command. This means a for command
 * actually generates two LoopInfo structures.
 */

typedef enum {
    LOOP_EXCEPTION_RANGE,	/* Exception's range is part of a loop.
				 * Break and continue "exceptions" cause
				 * jumps to appropriate PC offsets. */
    CATCH_EXCEPTION_RANGE	/* Exception's range is controlled by a
				 * catch command. Errors in the range cause
				 * a jump to a catch PC offset. */
} ExceptionRangeType;

typedef struct ExceptionRange {
    ExceptionRangeType type;	/* The kind of ExceptionRange. */
    int nestingLevel;		/* Static depth of the exception range.
				 * Used to find the most deeply-nested
				 * range surrounding a PC at runtime. */
    int codeOffset;		/* Offset of the first instruction byte of
				 * the code range. */
    int numCodeBytes;		/* Number of bytes in the code range. */
    int breakOffset;		/* If LOOP_EXCEPTION_RANGE, the target PC
				 * offset for a break command in the range. */
    int continueOffset;		/* If LOOP_EXCEPTION_RANGE and not -1, the
				 * target PC offset for a continue command in
				 * the code range. Otherwise, ignore this range
				 * when processing a continue command. */
    int catchOffset;		/* If a CATCH_EXCEPTION_RANGE, the target PC
				 * offset for any "exception" in range. */
} ExceptionRange;

/*
 * Structure used to map between instruction pc and source locations. It
 * defines for each compiled Tcl command its code's starting offset and 
 * its source's starting offset and length. Note that the code offset
 * increases monotonically: that is, the table is sorted in code offset
 * order. The source offset is not monotonic.
 */

typedef struct CmdLocation {
    int codeOffset;		/* Offset of first byte of command code. */
    int numCodeBytes;		/* Number of bytes for command's code. */
    int srcOffset;		/* Offset of first char of the command. */
    int numSrcBytes;		/* Number of command source chars. */
} CmdLocation;

/*
 * CompileProcs need the ability to record information during compilation
 * that can be used by bytecode instructions during execution. The AuxData
 * structure provides this "auxiliary data" mechanism. An arbitrary number
 * of these structures can be stored in the ByteCode record (during
 * compilation they are stored in a CompileEnv structure). Each AuxData
 * record holds one word of client-specified data (often a pointer) and is
 * given an index that instructions can later use to look up the structure
 * and its data.
 *
 * The following definitions declare the types of procedures that are called
 * to duplicate or free this auxiliary data when the containing ByteCode
 * objects are duplicated and freed. Pointers to these procedures are kept
 * in the AuxData structure.
 */

typedef ClientData (AuxDataDupProc)  _ANSI_ARGS_((ClientData clientData));
typedef void       (AuxDataFreeProc) _ANSI_ARGS_((ClientData clientData));

/*
 * We define a separate AuxDataType struct to hold type-related information
 * for the AuxData structure. This separation makes it possible for clients
 * outside of the TCL core to manipulate (in a limited fashion!) AuxData;
 * for example, it makes it possible to pickle and unpickle AuxData structs.
 */

typedef struct AuxDataType {
    char *name;					/* the name of the type. Types can be
                                 * registered and found by name */
    AuxDataDupProc *dupProc;	/* Callback procedure to invoke when the
                                 * aux data is duplicated (e.g., when the
                                 * ByteCode structure containing the aux
                                 * data is duplicated). NULL means just
                                 * copy the source clientData bits; no
                                 * proc need be called. */
    AuxDataFreeProc *freeProc;	/* Callback procedure to invoke when the
                                 * aux data is freed. NULL means no
                                 * proc need be called. */
} AuxDataType;

/*
 * The definition of the AuxData structure that holds information created
 * during compilation by CompileProcs and used by instructions during
 * execution.
 */

typedef struct AuxData {
    AuxDataType *type;		/* pointer to the AuxData type associated with
                             * this ClientData. */
    ClientData clientData;	/* The compilation data itself. */
} AuxData;

/*
 * Structure defining the compilation environment. After compilation, fields
 * describing bytecode instructions are copied out into the more compact
 * ByteCode structure defined below.
 */

#define COMPILEENV_INIT_CODE_BYTES    250
#define COMPILEENV_INIT_NUM_OBJECTS    60
#define COMPILEENV_INIT_EXCEPT_RANGES   5
#define COMPILEENV_INIT_CMD_MAP_SIZE   40
#define COMPILEENV_INIT_AUX_DATA_SIZE   5

typedef struct CompileEnv {
    Interp *iPtr;		/* Interpreter containing the code being
				 * compiled. Commands and their compile
				 * procs are specific to an interpreter so
				 * the code emitted will depend on the
				 * interpreter. */
    char *source;		/* The source string being compiled by
				 * SetByteCodeFromAny. This pointer is not
				 * owned by the CompileEnv and must not be
				 * freed or changed by it. */
    int numSrcBytes;		/* Number of bytes in source. */
    Proc *procPtr;		/* If a procedure is being compiled, a
				 * pointer to its Proc structure; otherwise
				 * NULL. Used to compile local variables.
				 * Set from information provided by
				 * ObjInterpProc in tclProc.c. */
    int numCommands;		/* Number of commands compiled. */
    int exceptDepth;		/* Current exception range nesting level;
				 * -1 if not in any range currently. */
    int maxExceptDepth;		/* Max nesting level of exception ranges;
				 * -1 if no ranges have been compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed
				 * to execute the code. Set by compilation
				 * procedures before returning. */
    LiteralTable localLitTable;	/* Contains LiteralEntry's describing
				 * all Tcl objects referenced by this
				 * compiled code. Indexed by the string
				 * representations of the literals. Used to
				 * avoid creating duplicate objects. */
    int exprIsJustVarRef;	/* Set 1 if the expression last compiled by
				 * TclCompileExpr consisted of just a
				 * variable reference as in the expression
				 * of "if $b then...". Otherwise 0. Used
				 * to implement expr's 2 level substitution
				 * semantics properly. */
    int exprIsComparison;	/* Set 1 if the top-level operator in the
				 * expression last compiled is a comparison.
				 * Otherwise 0. If 1, since the operands
				 * might be strings, the expr is compiled
				 * out-of-line to implement expr's 2 level
				 * substitution semantics properly. */
    unsigned char *codeStart;	/* Points to the first byte of the code. */
    unsigned char *codeNext;	/* Points to next code array byte to use. */
    unsigned char *codeEnd;	/* Points just after the last allocated
				 * code array byte. */
    int mallocedCodeArray;      /* Set 1 if code array was expanded 
				 * and codeStart points into the heap.*/
    LiteralEntry *literalArrayPtr;
    				/* Points to start of LiteralEntry array. */
    int literalArrayNext;	/* Index of next free object array entry. */
    int literalArrayEnd;	/* Index just after last obj array entry. */
    int mallocedLiteralArray;   /* 1 if object array was expanded and
                                 * objArray points into the heap, else 0. */
    ExceptionRange *exceptArrayPtr;
    				/* Points to start of the ExceptionRange
				 * array. */
    int exceptArrayNext;	/* Next free ExceptionRange array index.
				 * exceptArrayNext is the number of ranges
				 * and (exceptArrayNext-1) is the index of
				 * the current range's array entry. */
    int exceptArrayEnd;		/* Index after the last ExceptionRange
				 * array entry. */
    int mallocedExceptArray;	/* 1 if ExceptionRange array was expanded
				 * and exceptArrayPtr points in heap,
				 * else 0. */
    CmdLocation *cmdMapPtr;	/* Points to start of CmdLocation array.
				 * numCommands is the index of the next
				 * entry to use; (numCommands-1) is the
				 * entry index for the last command. */
    int cmdMapEnd;		/* Index after last CmdLocation entry. */
    int mallocedCmdMap;		/* 1 if command map array was expanded and
				 * cmdMapPtr points in the heap, else 0. */
    AuxData *auxDataArrayPtr;   /* Points to auxiliary data array start. */
    int auxDataArrayNext;	/* Next free compile aux data array index.
				 * auxDataArrayNext is the number of aux
				 * data items and (auxDataArrayNext-1) is
				 * index of current aux data array entry. */
    int auxDataArrayEnd;	/* Index after last aux data array entry. */
    int mallocedAuxDataArray;	/* 1 if aux data array was expanded and
				 * auxDataArrayPtr points in heap else 0. */
    unsigned char staticCodeSpace[COMPILEENV_INIT_CODE_BYTES];
                                /* Initial storage for code. */
    LiteralEntry staticLiteralSpace[COMPILEENV_INIT_NUM_OBJECTS];
                                /* Initial storage of LiteralEntry array. */
    ExceptionRange staticExceptArraySpace[COMPILEENV_INIT_EXCEPT_RANGES];
                                /* Initial ExceptionRange array storage. */
    CmdLocation staticCmdMapSpace[COMPILEENV_INIT_CMD_MAP_SIZE];
                                /* Initial storage for cmd location map. */
    AuxData staticAuxDataArraySpace[COMPILEENV_INIT_AUX_DATA_SIZE];
                                /* Initial storage for aux data array. */
} CompileEnv;

/*
 * The structure defining the bytecode instructions resulting from compiling
 * a Tcl script. Note that this structure is variable length: a single heap
 * object is allocated to hold the ByteCode structure immediately followed
 * by the code bytes, the literal object array, the ExceptionRange array,
 * the CmdLocation map, and the compilation AuxData array.
 */

/*
 * A PRECOMPILED bytecode struct is one that was generated from a compiled
 * image rather than implicitly compiled from source
 */
#define TCL_BYTECODE_PRECOMPILED		0x0001

typedef struct ByteCode {
    TclHandle interpHandle;	/* Handle for interpreter containing the
				 * compiled code.  Commands and their compile
				 * procs are specific to an interpreter so the
				 * code emitted will depend on the
				 * interpreter. */
    int compileEpoch;		/* Value of iPtr->compileEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when, e.g., commands with compile
				 * procs are redefined. */
    Namespace *nsPtr;		/* Namespace context in which this code
				 * was compiled. If the code is executed
				 * if a different namespace, it must be
				 * recompiled. */
    int nsEpoch;		/* Value of nsPtr->resolverEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when new namespace resolution rules
				 * are put into effect. */
    int refCount;		/* Reference count: set 1 when created
				 * plus 1 for each execution of the code
				 * currently active. This structure can be
				 * freed when refCount becomes zero. */
    unsigned int flags;		/* flags describing state for the codebyte.
                                 * this variable holds ORed values from the
                                 * TCL_BYTECODE_ masks defined above */
    char *source;		/* The source string from which this
				 * ByteCode was compiled. Note that this
				 * pointer is not owned by the ByteCode and
				 * must not be freed or modified by it. */
    Proc *procPtr;		/* If the ByteCode was compiled from a
				 * procedure body, this is a pointer to its
				 * Proc structure; otherwise NULL. This
				 * pointer is also not owned by the ByteCode
				 * and must not be freed by it. */
    size_t structureSize;	/* Number of bytes in the ByteCode structure
				 * itself. Does not include heap space for
				 * literal Tcl objects or storage referenced
				 * by AuxData entries. */
    int numCommands;		/* Number of commands compiled. */
    int numSrcBytes;		/* Number of source bytes compiled. */
    int numCodeBytes;		/* Number of code bytes. */
    int numLitObjects;		/* Number of objects in literal array. */
    int numExceptRanges;	/* Number of ExceptionRange array elems. */
    int numAuxDataItems;	/* Number of AuxData items. */
    int numCmdLocBytes;		/* Number of bytes needed for encoded
				 * command location information. */
    int maxExceptDepth;		/* Maximum nesting level of ExceptionRanges;
				 * -1 if no ranges were compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed
				 * to execute the code. */
    unsigned char *codeStart;	/* Points to the first byte of the code.
				 * This is just after the final ByteCode
				 * member cmdMapPtr. */
    Tcl_Obj **objArrayPtr;	/* Points to the start of the literal
				 * object array. This is just after the
				 * last code byte. */
    ExceptionRange *exceptArrayPtr;
    				/* Points to the start of the ExceptionRange
				 * array. This is just after the last
				 * object in the object array. */
    AuxData *auxDataArrayPtr;   /* Points to the start of the auxiliary data
				 * array. This is just after the last entry
				 * in the ExceptionRange array. */
    unsigned char *codeDeltaStart;
				/* Points to the first of a sequence of
				 * bytes that encode the change in the
				 * starting offset of each command's code.
				 * If -127<=delta<=127, it is encoded as 1
				 * byte, otherwise 0xFF (128) appears and
				 * the delta is encoded by the next 4 bytes.
				 * Code deltas are always positive. This
				 * sequence is just after the last entry in
				 * the AuxData array. */
    unsigned char *codeLengthStart;
				/* Points to the first of a sequence of
				 * bytes that encode the length of each
				 * command's code. The encoding is the same
				 * as for code deltas. Code lengths are
				 * always positive. This sequence is just
				 * after the last entry in the code delta
				 * sequence. */
    unsigned char *srcDeltaStart;
				/* Points to the first of a sequence of
				 * bytes that encode the change in the
				 * starting offset of each command's source.
				 * The encoding is the same as for code
				 * deltas. Source deltas can be negative.
				 * This sequence is just after the last byte
				 * in the code length sequence. */
    unsigned char *srcLengthStart;
				/* Points to the first of a sequence of
				 * bytes that encode the length of each
				 * command's source. The encoding is the
				 * same as for code deltas. Source lengths
				 * are always positive. This sequence is
				 * just after the last byte in the source
				 * delta sequence. */
#ifdef TCL_COMPILE_STATS
    Tcl_Time createTime;	/* Absolute time when the ByteCode was
				 * created. */
#endif /* TCL_COMPILE_STATS */
} ByteCode;

/*
 * Opcodes for the Tcl bytecode instructions. These must correspond to the
 * entries in the table of instruction descriptions, instructionTable, in
 * tclCompile.c. Also, the order and number of the expression opcodes
 * (e.g., INST_LOR) must match the entries in the array operatorStrings in
 * tclExecute.c.
 */

/* Opcodes 0 to 9 */
#define INST_DONE			0
#define INST_PUSH1			1
#define INST_PUSH4			2
#define INST_POP			3
#define INST_DUP			4
#define INST_CONCAT1			5
#define INST_INVOKE_STK1		6
#define INST_INVOKE_STK4		7
#define INST_EVAL_STK			8
#define INST_EXPR_STK			9

/* Opcodes 10 to 23 */
#define INST_LOAD_SCALAR1		10
#define INST_LOAD_SCALAR4		11
#define INST_LOAD_SCALAR_STK		12
#define INST_LOAD_ARRAY1		13
#define INST_LOAD_ARRAY4		14
#define INST_LOAD_ARRAY_STK		15
#define INST_LOAD_STK			16
#define INST_STORE_SCALAR1		17
#define INST_STORE_SCALAR4		18
#define INST_STORE_SCALAR_STK		19
#define INST_STORE_ARRAY1		20
#define INST_STORE_ARRAY4		21
#define INST_STORE_ARRAY_STK		22
#define INST_STORE_STK			23

/* Opcodes 24 to 33 */
#define INST_INCR_SCALAR1		24
#define INST_INCR_SCALAR_STK		25
#define INST_INCR_ARRAY1		26
#define INST_INCR_ARRAY_STK		27
#define INST_INCR_STK			28
#define INST_INCR_SCALAR1_IMM		29
#define INST_INCR_SCALAR_STK_IMM	30
#define INST_INCR_ARRAY1_IMM		31
#define INST_INCR_ARRAY_STK_IMM		32
#define INST_INCR_STK_IMM		33

/* Opcodes 34 to 39 */
#define INST_JUMP1			34
#define INST_JUMP4			35
#define INST_JUMP_TRUE1			36
#define INST_JUMP_TRUE4			37
#define INST_JUMP_FALSE1		38
#define INST_JUMP_FALSE4	        39

/* Opcodes 40 to 64 */
#define INST_LOR			40
#define INST_LAND			41
#define INST_BITOR			42
#define INST_BITXOR			43
#define INST_BITAND			44
#define INST_EQ				45
#define INST_NEQ			46
#define INST_LT				47
#define INST_GT				48
#define INST_LE				49
#define INST_GE				50
#define INST_LSHIFT			51
#define INST_RSHIFT			52
#define INST_ADD			53
#define INST_SUB			54
#define INST_MULT			55
#define INST_DIV			56
#define INST_MOD			57
#define INST_UPLUS			58
#define INST_UMINUS			59
#define INST_BITNOT			60
#define INST_LNOT			61
#define INST_CALL_BUILTIN_FUNC1		62
#define INST_CALL_FUNC1			63
#define INST_TRY_CVT_TO_NUMERIC		64

/* Opcodes 65 to 66 */
#define INST_BREAK			65
#define INST_CONTINUE			66

/* Opcodes 67 to 68 */
#define INST_FOREACH_START4		67
#define INST_FOREACH_STEP4		68

/* Opcodes 69 to 72 */
#define INST_BEGIN_CATCH4		69
#define INST_END_CATCH			70
#define INST_PUSH_RESULT		71
#define INST_PUSH_RETURN_CODE		72

/* The last opcode */
#define LAST_INST_OPCODE        	72

/*
 * Table describing the Tcl bytecode instructions: their name (for
 * displaying code), total number of code bytes required (including
 * operand bytes), and a description of the type of each operand.
 * These operand types include signed and unsigned integers of length
 * one and four bytes. The unsigned integers are used for indexes or
 * for, e.g., the count of objects to push in a "push" instruction.
 */

#define MAX_INSTRUCTION_OPERANDS 2

typedef enum InstOperandType {
    OPERAND_NONE,
    OPERAND_INT1,		/* One byte signed integer. */
    OPERAND_INT4,		/* Four byte signed integer. */
    OPERAND_UINT1,		/* One byte unsigned integer. */
    OPERAND_UINT4		/* Four byte unsigned integer. */
} InstOperandType;

typedef struct InstructionDesc {
    char *name;			/* Name of instruction. */
    int numBytes;		/* Total number of bytes for instruction. */
    int numOperands;		/* Number of operands. */
    InstOperandType opTypes[MAX_INSTRUCTION_OPERANDS];
				/* The type of each operand. */
} InstructionDesc;

extern InstructionDesc instructionTable[];

/*
 * Definitions of the values of the INST_CALL_BUILTIN_FUNC instruction's
 * operand byte. Each value denotes a builtin Tcl math function. These
 * values must correspond to the entries in the builtinFuncTable array
 * below and to the values stored in the tclInt.h MathFunc structure's
 * builtinFuncIndex field.
 */

#define BUILTIN_FUNC_ACOS		0
#define BUILTIN_FUNC_ASIN		1
#define BUILTIN_FUNC_ATAN		2
#define BUILTIN_FUNC_ATAN2		3
#define BUILTIN_FUNC_CEIL		4
#define BUILTIN_FUNC_COS		5
#define BUILTIN_FUNC_COSH		6
#define BUILTIN_FUNC_EXP		7
#define BUILTIN_FUNC_FLOOR		8
#define BUILTIN_FUNC_FMOD		9
#define BUILTIN_FUNC_HYPOT		10
#define BUILTIN_FUNC_LOG		11
#define BUILTIN_FUNC_LOG10		12
#define BUILTIN_FUNC_POW		13
#define BUILTIN_FUNC_SIN		14
#define BUILTIN_FUNC_SINH		15
#define BUILTIN_FUNC_SQRT		16
#define BUILTIN_FUNC_TAN		17
#define BUILTIN_FUNC_TANH		18
#define BUILTIN_FUNC_ABS		19
#define BUILTIN_FUNC_DOUBLE		20
#define BUILTIN_FUNC_INT		21
#define BUILTIN_FUNC_RAND		22
#define BUILTIN_FUNC_ROUND		23
#define BUILTIN_FUNC_SRAND		24

#define LAST_BUILTIN_FUNC        	24

/*
 * Table describing the built-in math functions. Entries in this table are
 * indexed by the values of the INST_CALL_BUILTIN_FUNC instruction's
 * operand byte.
 */

typedef int (CallBuiltinFuncProc) _ANSI_ARGS_((Tcl_Interp *interp,
        ExecEnv *eePtr, ClientData clientData));

typedef struct {
    char *name;			/* Name of function. */
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    CallBuiltinFuncProc *proc;	/* Procedure implementing this function. */
    ClientData clientData;	/* Additional argument to pass to the
				 * function when invoking it. */
} BuiltinFunc;

extern BuiltinFunc builtinFuncTable[];

/*
 * Compilation of some Tcl constructs such as if commands and the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps. Since the PC target of these jumps isn't
 * known when the jumps are emitted, we record the offset of each jump in an
 * array of JumpFixup structures. There is one array for each sequence of
 * jumps to one target PC. When we learn the target PC, we update the jumps
 * with the correct distance. Also, if the distance is too great (> 127
 * bytes), we replace the single-byte jump with a four byte jump
 * instruction, move the instructions after the jump down, and update the
 * code offsets for any commands between the jump and the target.
 */

typedef enum {
    TCL_UNCONDITIONAL_JUMP,
    TCL_TRUE_JUMP,
    TCL_FALSE_JUMP
} TclJumpType;

typedef struct JumpFixup {
    TclJumpType jumpType;	/* Indicates the kind of jump. */
    int codeOffset;		/* Offset of the first byte of the one-byte
				 * forward jump's code. */
    int cmdIndex;		/* Index of the first command after the one
				 * for which the jump was emitted. Used to
				 * update the code offsets for subsequent
				 * commands if the two-byte jump at jumpPc
				 * must be replaced with a five-byte one. */
    int exceptIndex;		/* Index of the first range entry in the
				 * ExceptionRange array after the current
				 * one. This field is used to adjust the
				 * code offsets in subsequent ExceptionRange
				 * records when a jump is grown from 2 bytes
				 * to 5 bytes. */
} JumpFixup;

#define JUMPFIXUP_INIT_ENTRIES    10

typedef struct JumpFixupArray {
    JumpFixup *fixup;		/* Points to start of jump fixup array. */
    int next;			/* Index of next free array entry. */
    int end;			/* Index of last usable entry in array. */
    int mallocedArray;		/* 1 if array was expanded and fixups points
				 * into the heap, else 0. */
    JumpFixup staticFixupSpace[JUMPFIXUP_INIT_ENTRIES];
				/* Initial storage for jump fixup array. */
} JumpFixupArray;

/*
 * The structure describing one variable list of a foreach command. Note
 * that only foreach commands inside procedure bodies are compiled inline so
 * a ForeachVarList structure always describes local variables. Furthermore,
 * only scalar variables are supported for inline-compiled foreach loops.
 */

typedef struct ForeachVarList {
    int numVars;		/* The number of variables in the list. */
    int varIndexes[1];		/* An array of the indexes ("slot numbers")
				 * for each variable in the procedure's
				 * array of local variables. Only scalar
				 * variables are supported. The actual
				 * size of this field will be large enough
				 * to numVars indexes. THIS MUST BE THE
				 * LAST FIELD IN THE STRUCTURE! */
} ForeachVarList;

/*
 * Structure used to hold information about a foreach command that is needed
 * during program execution. These structures are stored in CompileEnv and
 * ByteCode structures as auxiliary data.
 */

typedef struct ForeachInfo {
    int numLists;		/* The number of both the variable and value
				 * lists of the foreach command. */
    int firstValueTemp;		/* Index of the first temp var in a proc
				 * frame used to point to a value list. */
    int loopCtTemp;		/* Index of temp var in a proc frame
				 * holding the loop's iteration count. Used
				 * to determine next value list element to
				 * assign each loop var. */
    ForeachVarList *varLists[1];/* An array of pointers to ForeachVarList
				 * structures describing each var list. The
				 * actual size of this field will be large
				 * enough to numVars indexes. THIS MUST BE
				 * THE LAST FIELD IN THE STRUCTURE! */
} ForeachInfo;

extern AuxDataType		tclForeachInfoType;

/*
 * Structure containing a cached pointer to a command that is the result
 * of resolving the command's name in some namespace. It is the internal
 * representation for a cmdName object. It contains the pointer along
 * with some information that is used to check the pointer's validity.
 */

typedef struct ResolvedCmdName {
    Command *cmdPtr;		/* A cached Command pointer. */
    Namespace *refNsPtr;	/* Points to the namespace containing the
				 * reference (not the namespace that
				 * contains the referenced command). */
    long refNsId;		/* refNsPtr's unique namespace id. Used to
				 * verify that refNsPtr is still valid
				 * (e.g., it's possible that the cmd's
				 * containing namespace was deleted and a
				 * new one created at the same address). */
    int refNsCmdEpoch;		/* Value of the referencing namespace's
				 * cmdRefEpoch when the pointer was cached.
				 * Before using the cached pointer, we check
				 * if the namespace's epoch was incremented;
				 * if so, this cached pointer is invalid. */
    int cmdEpoch;		/* Value of the command's cmdEpoch when this
				 * pointer was cached. Before using the
				 * cached pointer, we check if the cmd's
				 * epoch was incremented; if so, the cmd was
				 * renamed, deleted, hidden, or exposed, and
				 * so the pointer is invalid. */
    int refCount;		/* Reference count: 1 for each cmdName
				 * object that has a pointer to this
				 * ResolvedCmdName structure as its internal
				 * rep. This structure can be freed when
				 * refCount becomes zero. */
} ResolvedCmdName;

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl bytecode compilation and execution
 * modules but not used outside:
 *----------------------------------------------------------------
 */

EXTERN void		TclCleanupByteCode _ANSI_ARGS_((ByteCode *codePtr));
EXTERN int		TclCompileCmdWord _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr));
EXTERN int		TclCompileExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    char *script, int numBytes,
			    CompileEnv *envPtr));
EXTERN int		TclCompileExprWords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int numWords,
			    CompileEnv *envPtr));
EXTERN int		TclCompileScript _ANSI_ARGS_((Tcl_Interp *interp,
			    char *script, int numBytes, int nested,
			    CompileEnv *envPtr));
EXTERN int		TclCompileTokens _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr));
EXTERN int		TclCreateAuxData _ANSI_ARGS_((ClientData clientData,
			    AuxDataType *typePtr, CompileEnv *envPtr));
EXTERN int		TclCreateExceptRange _ANSI_ARGS_((
			    ExceptionRangeType type, CompileEnv *envPtr));
EXTERN ExecEnv *	TclCreateExecEnv _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		TclDeleteExecEnv _ANSI_ARGS_((ExecEnv *eePtr));
EXTERN void		TclDeleteLiteralTable _ANSI_ARGS_((
			    Tcl_Interp *interp, LiteralTable *tablePtr));
EXTERN void		TclEmitForwardJump _ANSI_ARGS_((CompileEnv *envPtr,
			    TclJumpType jumpType, JumpFixup *jumpFixupPtr));
EXTERN ExceptionRange *	TclGetExceptionRangeForPc _ANSI_ARGS_((
			    unsigned char *pc, int catchOnly,
			    ByteCode* codePtr));
EXTERN InstructionDesc * TclGetInstructionTable _ANSI_ARGS_(());
EXTERN int		TclExecuteByteCode _ANSI_ARGS_((Tcl_Interp *interp,
			    ByteCode *codePtr));
EXTERN void		TclExpandCodeArray _ANSI_ARGS_((
			    CompileEnv *envPtr));
EXTERN void		TclExpandJumpFixupArray _ANSI_ARGS_((
                            JumpFixupArray *fixupArrayPtr));
EXTERN void		TclFinalizeAuxDataTypeTable _ANSI_ARGS_((void));
EXTERN int		TclFindCompiledLocal _ANSI_ARGS_((char *name, 
        		    int nameChars, int create, int flags,
			    Proc *procPtr));
EXTERN LiteralEntry *	TclLookupLiteralEntry _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
EXTERN int		TclFixupForwardJump _ANSI_ARGS_((
			    CompileEnv *envPtr, JumpFixup *jumpFixupPtr,
			    int jumpDist, int distThreshold));
EXTERN void		TclFreeCompileEnv _ANSI_ARGS_((CompileEnv *envPtr));
EXTERN void		TclFreeJumpFixupArray _ANSI_ARGS_((
  			    JumpFixupArray *fixupArrayPtr));
EXTERN void		TclInitAuxDataTypeTable _ANSI_ARGS_((void));
EXTERN void		TclInitByteCodeObj _ANSI_ARGS_((Tcl_Obj *objPtr,
			    CompileEnv *envPtr));
EXTERN void		TclInitCompilation _ANSI_ARGS_((void));
EXTERN void		TclInitCompileEnv _ANSI_ARGS_((Tcl_Interp *interp,
			    CompileEnv *envPtr, char *string,
			    int numBytes));
EXTERN void		TclInitJumpFixupArray _ANSI_ARGS_((
			    JumpFixupArray *fixupArrayPtr));
EXTERN void		TclInitLiteralTable _ANSI_ARGS_((
			    LiteralTable *tablePtr));
#ifdef TCL_COMPILE_STATS
EXTERN char *		TclLiteralStats _ANSI_ARGS_((
			    LiteralTable *tablePtr));
EXTERN int		TclLog2 _ANSI_ARGS_((int value));
#endif
#ifdef TCL_COMPILE_DEBUG
EXTERN void		TclPrintByteCodeObj _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Obj *objPtr));
#endif
EXTERN int		TclPrintInstruction _ANSI_ARGS_((ByteCode* codePtr,
			    unsigned char *pc));
EXTERN void		TclPrintObject _ANSI_ARGS_((FILE *outFile,
			    Tcl_Obj *objPtr, int maxChars));
EXTERN void		TclPrintSource _ANSI_ARGS_((FILE *outFile,
			    char *string, int maxChars));
EXTERN void		TclRegisterAuxDataType _ANSI_ARGS_((AuxDataType *typePtr));
EXTERN int		TclRegisterLiteral _ANSI_ARGS_((CompileEnv *envPtr,
			    char *bytes, int length, int onHeap));
EXTERN void		TclReleaseLiteral _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
EXTERN void		TclSetCmdNameObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Command *cmdPtr));
#ifdef TCL_COMPILE_DEBUG
EXTERN void		TclVerifyGlobalLiteralTable _ANSI_ARGS_((
			    Interp *iPtr));
EXTERN void		TclVerifyLocalLiteralTable _ANSI_ARGS_((
			    CompileEnv *envPtr));
#endif

/*
 *----------------------------------------------------------------
 * Macros used by Tcl bytecode compilation and execution modules
 * inside the Tcl core but not used outside.
 *----------------------------------------------------------------
 */

/*
 * Macro to emit an opcode byte into a CompileEnv's code array.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN void	TclEmitOpcode _ANSI_ARGS_((unsigned char op,
 *		    CompileEnv *envPtr));
 */

#define TclEmitOpcode(op, envPtr) \
    if ((envPtr)->codeNext == (envPtr)->codeEnd) \
        TclExpandCodeArray(envPtr); \
    *(envPtr)->codeNext++ = (unsigned char) (op)

/*
 * Macro to emit an integer operand.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN void	TclEmitInt1 _ANSI_ARGS_((int i, CompileEnv *envPtr));
 */

#define TclEmitInt1(i, envPtr) \
    if ((envPtr)->codeNext == (envPtr)->codeEnd) \
        TclExpandCodeArray(envPtr); \
    *(envPtr)->codeNext++ = (unsigned char) ((unsigned int) (i))

/*
 * Macros to emit an instruction with signed or unsigned integer operands.
 * Four byte integers are stored in "big-endian" order with the high order
 * byte stored at the lowest address.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN void	TclEmitInstInt1 _ANSI_ARGS_((unsigned char op, int i, 
 *		    CompileEnv *envPtr));
 * EXTERN void	TclEmitInstInt4 _ANSI_ARGS_((unsigned char op, int i, 
 *		    CompileEnv *envPtr));
 */

#define TclEmitInstInt1(op, i, envPtr) \
    if (((envPtr)->codeNext + 2) > (envPtr)->codeEnd) { \
        TclExpandCodeArray(envPtr); \
    } \
    *(envPtr)->codeNext++ = (unsigned char) (op); \
    *(envPtr)->codeNext++ = (unsigned char) ((unsigned int) (i))

#define TclEmitInstInt4(op, i, envPtr) \
    if (((envPtr)->codeNext + 5) > (envPtr)->codeEnd) { \
        TclExpandCodeArray(envPtr); \
    } \
    *(envPtr)->codeNext++ = (unsigned char) (op); \
    *(envPtr)->codeNext++ = \
        (unsigned char) ((unsigned int) (i) >> 24); \
    *(envPtr)->codeNext++ = \
        (unsigned char) ((unsigned int) (i) >> 16); \
    *(envPtr)->codeNext++ = \
        (unsigned char) ((unsigned int) (i) >>  8); \
    *(envPtr)->codeNext++ = \
        (unsigned char) ((unsigned int) (i)      )
    
/*
 * Macro to push a Tcl object onto the Tcl evaluation stack. It emits the
 * object's one or four byte array index into the CompileEnv's code
 * array. These support, respectively, a maximum of 256 (2**8) and 2**32
 * objects in a CompileEnv. The ANSI C "prototype" for this macro is:
 *
 * EXTERN void	TclEmitPush _ANSI_ARGS_((int objIndex, CompileEnv *envPtr));
 */

#define TclEmitPush(objIndex, envPtr) \
    if ((objIndex) <= 255) { \
	TclEmitInstInt1(INST_PUSH1, (objIndex), (envPtr)); \
    } else { \
	TclEmitInstInt4(INST_PUSH4, (objIndex), (envPtr)); \
    }

/*
 * Macros to update a (signed or unsigned) integer starting at a pointer.
 * The two variants depend on the number of bytes. The ANSI C "prototypes"
 * for these macros are:
 *
 * EXTERN void	TclStoreInt1AtPtr _ANSI_ARGS_((int i, unsigned char *p));
 * EXTERN void	TclStoreInt4AtPtr _ANSI_ARGS_((int i, unsigned char *p));
 */
    
#define TclStoreInt1AtPtr(i, p) \
    *(p)   = (unsigned char) ((unsigned int) (i))
    
#define TclStoreInt4AtPtr(i, p) \
    *(p)   = (unsigned char) ((unsigned int) (i) >> 24); \
    *(p+1) = (unsigned char) ((unsigned int) (i) >> 16); \
    *(p+2) = (unsigned char) ((unsigned int) (i) >>  8); \
    *(p+3) = (unsigned char) ((unsigned int) (i)      )

/*
 * Macros to update instructions at a particular pc with a new op code
 * and a (signed or unsigned) int operand. The ANSI C "prototypes" for
 * these macros are:
 *
 * EXTERN void	TclUpdateInstInt1AtPc _ANSI_ARGS_((unsigned char op, int i,
 *		    unsigned char *pc));
 * EXTERN void	TclUpdateInstInt4AtPc _ANSI_ARGS_((unsigned char op, int i,
 *		    unsigned char *pc));
 */

#define TclUpdateInstInt1AtPc(op, i, pc) \
    *(pc) = (unsigned char) (op); \
    TclStoreInt1AtPtr((i), ((pc)+1))

#define TclUpdateInstInt4AtPc(op, i, pc) \
    *(pc) = (unsigned char) (op); \
    TclStoreInt4AtPtr((i), ((pc)+1))
    
/*
 * Macros to get a signed integer (GET_INT{1,2}) or an unsigned int
 * (GET_UINT{1,2}) from a pointer. There are two variants for each
 * return type that depend on the number of bytes fetched.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN int	        TclGetInt1AtPtr  _ANSI_ARGS_((unsigned char *p));
 * EXTERN int	        TclGetInt4AtPtr  _ANSI_ARGS_((unsigned char *p));
 * EXTERN unsigned int	TclGetUInt1AtPtr _ANSI_ARGS_((unsigned char *p));
 * EXTERN unsigned int	TclGetUInt4AtPtr _ANSI_ARGS_((unsigned char *p));
 */

/*
 * The TclGetInt1AtPtr macro is tricky because we want to do sign
 * extension on the 1-byte value. Unfortunately the "char" type isn't
 * signed on all platforms so sign-extension doesn't always happen
 * automatically. Sometimes we can explicitly declare the pointer to be
 * signed, but other times we have to explicitly sign-extend the value
 * in software.
 */

#ifndef __CHAR_UNSIGNED__
#   define TclGetInt1AtPtr(p) ((int) *((char *) p))
#else
#   ifdef HAVE_SIGNED_CHAR
#	define TclGetInt1AtPtr(p) ((int) *((signed char *) p))
#    else
#	define TclGetInt1AtPtr(p) (((int) *((char *) p)) \
		| ((*(p) & 0200) ? (-256) : 0))
#    endif
#endif

#define TclGetInt4AtPtr(p) (((int) TclGetInt1AtPtr(p) << 24) | \
		                  	    (*((p)+1) << 16) | \
				  	    (*((p)+2) <<  8) | \
				  	    (*((p)+3)))

#define TclGetUInt1AtPtr(p) ((unsigned int) *(p))
#define TclGetUInt4AtPtr(p) ((unsigned int) (*(p)     << 24) | \
		                            (*((p)+1) << 16) | \
				            (*((p)+2) <<  8) | \
				            (*((p)+3)))

/*
 * Macros used to compute the minimum and maximum of two integers.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN int  TclMin _ANSI_ARGS_((int i, int j));
 * EXTERN int  TclMax _ANSI_ARGS_((int i, int j));
 */

#define TclMin(i, j)   ((((int) i) < ((int) j))? (i) : (j))
#define TclMax(i, j)   ((((int) i) > ((int) j))? (i) : (j))

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLCOMPILATION */

