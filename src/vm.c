#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"


void reset_stack(vm* cvm) {
  cvm->stack_top = cvm->stack;
}

vm* init_vm() {
  vm* res = malloc(sizeof(vm));
  reset_stack(res);
  return res;
}

void free_vm(vm* cvm) {
  free(cvm);
}

static void runtime_error(vm* cvm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = cvm->ip - cvm->c->code;
  fprintf(stderr, "[line %d] in script\n",
          cvm->c->lines[instruction]);

  reset_stack(cvm);
}

void push(vm* cvm, value v) {
  *cvm->stack_top = v;
  cvm->stack_top++;
}

value pop(vm* cvm) {
  return *(--cvm->stack_top);
}

static value peek(vm* cvm, int distance) {
  return cvm->stack_top[-1-distance];
}

static bool is_falsy(value v) {
  return IS_NIL(v) || (IS_BOOL(v) && !AS_BOOL(v));
}

static interpret_result run(vm* cvm) {
#define binary_op(value_type, op) { \
      if (!IS_NUMBER(peek(cvm, 0)) || !IS_NUMBER(peek(cvm, 1))) { \
        runtime_error(cvm, "Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop(cvm)); \
      double a = AS_NUMBER(pop(cvm)); \
      push(cvm, value_type(a op b)); \
    }
#define bitwise_op(value_type, op) { \
      long b = (long)AS_NUMBER(pop(cvm)); \
      long a = (long)AS_NUMBER(pop(cvm)); \
      push(cvm, value_type(a op b)); \
    }
#define read_byte() (*cvm->ip++)
#define read_constant() (cvm->c->constants.values[read_byte()])
#define read_long_constant() (cvm->c->constants.values[read_byte() + (read_byte()<<8) + (read_byte()<<16)])

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    print_trace(cvm);
#endif

    uint8_t instruction;
    switch (instruction = read_byte()) {
      case OP_CONSTANT: {
        value constant = read_constant();
        push(cvm, constant);
        break;
      }
      case OP_CONSTANT_LONG: {
        value constant = read_long_constant();
        push(cvm, constant);
        break;
      }
      case OP_RETURN: {
        print_value(pop(cvm));
        puts("");
        return INTERPRET_OK;
      }
      case OP_EQUAL: {
        value b = pop(cvm);
        value a = pop(cvm);
        push(cvm, BOOL_VAL(values_equal(a, b)));
        break;
      }
      case OP_GREATER:    binary_op(BOOL_VAL, >); break;
      case OP_LESS:       binary_op(BOOL_VAL, <); break;
      case OP_NIL:        push(cvm, NIL_VAL); break;
      case OP_TRUE:       push(cvm, BOOL_VAL(true)); break;
      case OP_FALSE:      push(cvm, BOOL_VAL(false)); break;
      case OP_ADD:        binary_op(NUMBER_VAL, +); break;
      case OP_SUBTRACT:   binary_op(NUMBER_VAL, -); break;
      case OP_MULTIPLY:   binary_op(NUMBER_VAL, *); break;
      case OP_DIVIDE:     binary_op(NUMBER_VAL, /); break;
      case OP_MODULO:     bitwise_op(NUMBER_VAL, %); break;
      case OP_SHIFTLEFT:  bitwise_op(NUMBER_VAL, <<); break;
      case OP_SHIFTRIGHT: bitwise_op(NUMBER_VAL, >>); break;
      case OP_BITOR:      bitwise_op(NUMBER_VAL, |); break;
      case OP_BITXOR:     bitwise_op(NUMBER_VAL, ^); break;
      case OP_BITAND:     bitwise_op(NUMBER_VAL, &); break;
      case OP_NOT:        push(cvm, BOOL_VAL(is_falsy(pop(cvm)))); break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(cvm, 0))) {
          runtime_error(cvm, "Operand to '-' must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }

        push(cvm, NUMBER_VAL(-AS_NUMBER(pop(cvm))));
        break;
      case OP_BITNOT:
        if (!IS_NUMBER(peek(cvm, 0))) {
          runtime_error(cvm, "Operand to '-' must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }

        push(cvm, NUMBER_VAL((double)(~((long)AS_NUMBER(pop(cvm))))));
        break;
    }
  }

#undef binary_op
#undef read_byte
#undef read_constant
#undef read_long_constant
}

interpret_result interpret(vm* cvm, const char* source) {
  chunk c;
  init_chunk(&c);
  if (!compile(source, &c)) {
    free_chunk(&c);
    return INTERPRET_COMPILE_ERROR;
  }

  cvm->c = &c;
  cvm->ip = cvm->c->code;

  interpret_result res = run(cvm);

  free_chunk(&c);

  return res;
}
