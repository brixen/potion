//
// pn-ast.h
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_AST_H
#define POTION_AST_H

typedef unsigned char PNByte;

enum PN_AST {
  AST_CODE,
  AST_VALUE,
  AST_ASSIGN,
  AST_MESSAGE,
  AST_PATH,
  AST_QUERY,
  AST_PATHQ,
  AST_EXPR,
  AST_TABLE,
  AST_BLOCK,
  AST_DATA
};

struct PNSource {
  PN_OBJECT_HEADER
  PNByte part;
  PN a;
};

#define PN_AST(T, A)  potion_source(P, AST_##T, A)
#define PN_PUSH(T, X) potion_tuple_push(P, T, X)
#define PN_TUP(X)     potion_tuple_new(P, X)
#define PN_GET(T, X)  potion_tuple_find(P, T, X)
#define PN_PUT(T, X)  potion_tuple_put(P, &(T), X)

PN potion_source(Potion *, PNByte, PN);

#endif
