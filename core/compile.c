//
// compile.c
// ast to bytecode
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "pn-ast.h"
#include "opcodes.h"

typedef unsigned char u8;

#define PN_ASM2(ins, a, b) ({ \
    *((*pos)++) = (u8)ins; \
    *((*pos)++) = (u8)a; \
    *((*pos)++) = (u8)b; \
  })

const char *potion_op_names[] = {
  "move", "loadk", "loadnil", "loadbool",
  "getlocal", "setlocal", "gettable", "settable",
  "getpath", "setpath", "self", "bind",
  "test", "testset", "call", "tailcall",
  "return"
};

const enum PN_OPARGS potion_op_args[] = {
  OP_ASM2, OP_ASM2, OP_ASM1, OP_ASM1,
  OP_ASM2, OP_ASM2, OP_ASM2, OP_ASM2,
  OP_ASM2, OP_ASM2, OP_ASM1, OP_ASM2,
  OP_ASM2, OP_ASM2, OP_ASM2, OP_ASM2,
  OP_ASM2
};

PN potion_proto_inspect(Potion *P, PN cl, PN self) {
  struct PNProto *t = (struct PNProto *)self;
  unsigned int num = 1;
  u8 *pos, *end;
  printf("; function definition: %p ; %u bytes\n", t, PN_STR_LEN(t->asmb));
  PN_TUPLE_EACH(t->locals, i, v, {
    printf("; local \"");
    potion_send(v, PN_inspect);
    printf("\" ; %lu\n", i);
  });
  PN_TUPLE_EACH(t->values, i, v, {
    printf("; value \"");
    potion_send(v, PN_inspect);
    printf("\" ; %lu\n", i);
  });
  pos = (u8 *)PN_STR_PTR(t->asmb);
  end = (u8 *)PN_STR_PTR(t->asmb) + PN_STR_LEN(t->asmb);
  while (pos < end) {
    printf("[%u] %s", num, potion_op_names[pos[0]]);
    switch (potion_op_args[pos[0]]) {
      case OP_ASM1:
        printf(" %u", (unsigned)pos[1]);
        pos += 2;
      break;
      case OP_ASM2:
        printf(" %u %u", (unsigned)pos[1], (unsigned)pos[2]);
        pos += 3;
      break;
    }
    printf("\n");
    num++;
  }
  return PN_NIL;
}

void potion_source_asmb(Potion *P, struct PNProto *f, struct PNSource *t, u8 **pos) {
  switch (t->part) {
    case AST_CODE:
    case AST_EXPR:
      PN_TUPLE_EACH(t->a, i, v, {
        potion_source_asmb(P, f, (struct PNSource *)v, pos);
      });
      if (t->part == AST_CODE)
        PN_ASM2(OP_RETURN, 0, 1);
    break;

    case AST_VALUE: {
      unsigned long num = PN_PUT(f->values, t->a);
      PN_ASM2(OP_LOADK, 0, num);
    }
    break;

    case AST_ASSIGN: {
      struct PNTuple *tp = PN_GET_TUPLE(t->a);
      struct PNSource *lhs = (struct PNSource *)tp->set[0];
      unsigned long num = PN_NONE;

      if (lhs->part == AST_MESSAGE || lhs->part == AST_QUERY)
        num = PN_PUT(f->locals, lhs->a);
      else if (lhs->part == AST_PATH || lhs->part == AST_PATHQ)
        num = PN_PUT(f->values, lhs->a);

      PN_TUPLE_EACH(tp->set[1], i, v, {
        potion_source_asmb(P, f, (struct PNSource *)v, pos);
      });

      if (lhs->part == AST_MESSAGE || lhs->part == AST_QUERY) {
        if (lhs->part == AST_QUERY)
          PN_ASM2(OP_TEST, num, 1); 
        PN_ASM2(OP_SETLOCAL, 0, num);
      } else if (lhs->part == AST_PATH || lhs->part == AST_PATHQ) {
        if (lhs->part == AST_PATHQ)
          PN_ASM2(OP_TEST, num, 1);
        PN_ASM2(OP_SETPATH, 0, num);
      }
    }
    break;

    case AST_MESSAGE:
    case AST_QUERY: {
      unsigned long num = PN_GET(f->locals, t->a);
      if (num == PN_NONE) {
        num = PN_PUT(f->values, t->a);
        if (t->part == AST_MESSAGE)
          PN_ASM2(OP_CALL, 0, 0);
        else
          PN_ASM2(OP_TEST, num, 1);
      } else {
        PN_ASM2(OP_GETLOCAL, 0, num);
        if (t->part == AST_QUERY)
          PN_ASM2(OP_TEST, num, 1);
      }
    }
    break;

    case AST_PATH:
    case AST_PATHQ: {
      unsigned long num = PN_PUT(f->values, t->a);
      PN_ASM2(OP_GETPATH, 0, num);
      if (t->part == AST_PATHQ)
        PN_ASM2(OP_TEST, num, 1);
    }
    break;
  }
}

PN potion_source_compile(Potion *P, PN cl, PN self, PN source, PN sig) {
  struct PNProto *f;
  struct PNSource *t = (struct PNSource *)self;
  u8 *start, *pos;
  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK: break;
    default: return PN_NIL; // TODO: error
  }

  f = PN_OBJ_ALLOC(struct PNProto, PN_TPROTO, 0);
  f->source = source;
  f->sig = (sig == PN_NIL ? PN_EMPTY : sig);
  f->protos = f->locals = f->values = PN_EMPTY;
  f->asmb = potion_bytes(P, 8192);

  start = pos = (u8 *)PN_STR_PTR(f->asmb);
  potion_source_asmb(P, f, t, &pos);
  // TODO: byte strings should be more flexible than this
  PN_STR_LEN(f->asmb) = pos - start;
  return (PN)f;
}

void potion_compiler_init(Potion *P) {
  PN pro_vt = PN_VTABLE(PN_TPROTO);
  potion_method(pro_vt, "inspect", potion_proto_inspect, 0);
}
