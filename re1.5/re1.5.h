// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Copyright 2014 Paul Sokolovsky.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef _RE1_5_REGEXP__H
#define _RE1_5_REGEXP__H

#include <esp8266.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define nil ((void*)0)
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

typedef struct Regexp Regexp;
typedef struct Prog Prog;
typedef struct ByteProg ByteProg;
typedef struct Inst Inst;
typedef struct Subject Subject;

struct Regexp
{
	int type;
	int n;
	int ch;
	Regexp *left;
	Regexp *right;
};

enum	/* Regexp.type */
{
	Alt = 1,
	Cat,
	Lit,
	Dot,
	Paren,
	Quest,
	Star,
	Plus,
};

Regexp ICACHE_FLASH_ATTR *parse(char*);
Regexp ICACHE_FLASH_ATTR *reg(int type, Regexp *left, Regexp *right);
void ICACHE_FLASH_ATTR printre(Regexp*);
#ifndef re1_5_fatal
#define re1_5_fatal(x) (void)0
#endif
#ifndef re1_5_stack_chk
#define re1_5_stack_chk()
#endif
void ICACHE_FLASH_ATTR *mal(int);

struct Prog
{
	Inst *start;
	int len;
};

struct ByteProg
{
	int bytelen;
	int len;
	int sub;
	char insts[0];
};

struct Inst
{
	int opcode;
	int c;
	int n;
	Inst *x;
	Inst *y;
	int gen;	// global state, oooh!
};

enum	/* Inst.opcode */
{
	// Instructions which consume input bytes (and thus fail if none left)
	CONSUMERS = 1,
	Char = CONSUMERS,
	Any,
	Class,
	ClassNot,
	NamedClass,

	ASSERTS = 0x50,
	Bol = ASSERTS,
	Eol,

	// Instructions which take relative offset as arg
	JUMPS = 0x60,
	Jmp = JUMPS,
	Split,
	RSplit,

	// Other (special) instructions
	Save = 0x7e,
	Match = 0x7f,
};

#define inst_is_consumer(inst) ((inst) < ASSERTS)
#define inst_is_jump(inst) ((inst) & 0x70 == JUMPS)

Prog ICACHE_FLASH_ATTR *compile(Regexp*);
void ICACHE_FLASH_ATTR printprog(Prog*);

extern int gen;

enum {
	MAXSUB = 20
};

typedef struct Sub Sub;

struct Sub
{
	int ref;
	int nsub;
	const char *sub[MAXSUB];
};

Sub ICACHE_FLASH_ATTR *newsub(int n);
Sub ICACHE_FLASH_ATTR *incref(Sub*);
Sub ICACHE_FLASH_ATTR *copy(Sub*);
Sub ICACHE_FLASH_ATTR *update(Sub*, int, const char*);
void ICACHE_FLASH_ATTR decref(Sub*);

struct Subject {
	const char *begin;
	const char *end;
};


#define NON_ANCHORED_PREFIX 5
#define HANDLE_ANCHORED(bytecode, is_anchored) ((is_anchored) ? (bytecode) + NON_ANCHORED_PREFIX : (bytecode))

int ICACHE_FLASH_ATTR re1_5_backtrack(ByteProg*, Subject*, const char**, int, int);
int ICACHE_FLASH_ATTR re1_5_pikevm(ByteProg*, Subject*, const char**, int, int);
int ICACHE_FLASH_ATTR re1_5_recursiveloopprog(ByteProg*, Subject*, const char**, int, int);
int ICACHE_FLASH_ATTR re1_5_recursiveprog(ByteProg*, Subject*, const char**, int, int);
int ICACHE_FLASH_ATTR re1_5_thompsonvm(ByteProg*, Subject*, const char**, int, int);

int ICACHE_FLASH_ATTR re1_5_sizecode(const char *re);
int ICACHE_FLASH_ATTR re1_5_compilecode(ByteProg *prog, const char *re);
void ICACHE_FLASH_ATTR cleanmarks(ByteProg *prog);
int ICACHE_FLASH_ATTR _re1_5_classmatch(const char *pc, const char *sp);
int ICACHE_FLASH_ATTR _re1_5_namedclassmatch(const char *pc, const char *sp);

#endif /*_RE1_5_REGEXP__H*/
