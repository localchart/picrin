/**
 * See Copyright Notice in picrin.h
 */

#include "picrin.h"

void
pic_add_feature(pic_state *pic, const char *feature)
{
  pic_push(pic, pic_obj_value(pic_intern_cstr(pic, feature)), pic->features);
}

void pic_init_undef(pic_state *);
void pic_init_bool(pic_state *);
void pic_init_pair(pic_state *);
void pic_init_port(pic_state *);
void pic_init_number(pic_state *);
void pic_init_proc(pic_state *);
void pic_init_symbol(pic_state *);
void pic_init_vector(pic_state *);
void pic_init_blob(pic_state *);
void pic_init_cont(pic_state *);
void pic_init_char(pic_state *);
void pic_init_error(pic_state *);
void pic_init_str(pic_state *);
void pic_init_macro(pic_state *);
void pic_init_var(pic_state *);
void pic_init_write(pic_state *);
void pic_init_read(pic_state *);
void pic_init_dict(pic_state *);
void pic_init_record(pic_state *);
void pic_init_eval(pic_state *);
void pic_init_lib(pic_state *);
void pic_init_attr(pic_state *);
void pic_init_reg(pic_state *);

extern const char pic_boot[][80];

static void
pic_init_features(pic_state *pic)
{
  pic_add_feature(pic, "picrin");
  pic_add_feature(pic, "ieee-float");

#if _POSIX_SOURCE
  pic_add_feature(pic, "posix");
#endif

#if _WIN32
  pic_add_feature(pic, "windows");
#endif

#if __unix__
  pic_add_feature(pic, "unix");
#endif
#if __gnu_linux__
  pic_add_feature(pic, "gnu-linux");
#endif
#if __FreeBSD__
  pic_add_feature(pic, "freebsd");
#endif

#if __i386__
  pic_add_feature(pic, "i386");
#elif __x86_64__
  pic_add_feature(pic, "x86-64");
#elif __ppc__
  pic_add_feature(pic, "ppc");
#elif __sparc__
  pic_add_feature(pic, "sparc");
#endif

#if __ILP32__
  pic_add_feature(pic, "ilp32");
#elif __LP64__
  pic_add_feature(pic, "lp64");
#endif

#if defined(__BYTE_ORDER__)
# if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  pic_add_feature(pic, "little-endian");
# elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  pic_add_feature(pic, "big-endian");
# endif
#else
# if __LITTLE_ENDIAN__
  pic_add_feature(pic, "little-endian");
# elif __BIG_ENDIAN__
  pic_add_feature(pic, "big-endian");
# endif
#endif
}

#define DONE pic_gc_arena_restore(pic, ai);

static void
pic_init_core(pic_state *pic)
{
  pic_init_features(pic);

  pic_deflibrary (pic, "(picrin base)") {
    size_t ai = pic_gc_arena_preserve(pic);

    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sDEFINE, pic->rDEFINE);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sSETBANG, pic->rSETBANG);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sQUOTE, pic->rQUOTE);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sLAMBDA, pic->rLAMBDA);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sIF, pic->rIF);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sBEGIN, pic->rBEGIN);
    pic_define_syntactic_keyword(pic, pic->lib->env, pic->sDEFINE_SYNTAX, pic->rDEFINE_SYNTAX);

    pic_init_undef(pic); DONE;
    pic_init_bool(pic); DONE;
    pic_init_pair(pic); DONE;
    pic_init_port(pic); DONE;
    pic_init_number(pic); DONE;
    pic_init_proc(pic); DONE;
    pic_init_symbol(pic); DONE;
    pic_init_vector(pic); DONE;
    pic_init_blob(pic); DONE;
    pic_init_cont(pic); DONE;
    pic_init_char(pic); DONE;
    pic_init_error(pic); DONE;
    pic_init_str(pic); DONE;
    pic_init_macro(pic); DONE;
    pic_init_var(pic); DONE;
    pic_init_write(pic); DONE;
    pic_init_read(pic); DONE;
    pic_init_dict(pic); DONE;
    pic_init_record(pic); DONE;
    pic_init_eval(pic); DONE;
    pic_init_lib(pic); DONE;
    pic_init_attr(pic); DONE;
    pic_init_reg(pic); DONE;

    pic_load_cstr(pic, &pic_boot[0][0]);
  }

  pic_import_library(pic, pic->PICRIN_BASE);
}

pic_state *
pic_open(int argc, char *argv[], char **envp, pic_allocf allocf)
{
  struct pic_port *pic_make_standard_port(pic_state *, xFILE *, short);
  char t;

  pic_state *pic;
  size_t ai;

  pic = allocf(NULL, sizeof(pic_state));

  if (! pic) {
    goto EXIT_PIC;
  }

  /* allocator */
  pic->allocf = allocf;

  /* turn off GC */
  pic->gc_enable = false;

  /* jmp */
  pic->jmp = NULL;

  /* root block */
  pic->cp = NULL;

  /* command line */
  pic->argc = argc;
  pic->argv = argv;
  pic->envp = envp;

  /* prepare VM stack */
  pic->stbase = pic->sp = allocf(NULL, PIC_STACK_SIZE * sizeof(pic_value));
  pic->stend = pic->stbase + PIC_STACK_SIZE;

  if (! pic->sp) {
    goto EXIT_SP;
  }

  /* callinfo */
  pic->cibase = pic->ci = allocf(NULL, PIC_STACK_SIZE * sizeof(pic_callinfo));
  pic->ciend = pic->cibase + PIC_STACK_SIZE;

  if (! pic->ci) {
    goto EXIT_CI;
  }

  /* exception handler */
  pic->xpbase = pic->xp = allocf(NULL, PIC_RESCUE_SIZE * sizeof(struct pic_proc *));
  pic->xpend = pic->xpbase + PIC_RESCUE_SIZE;

  if (! pic->xp) {
    goto EXIT_XP;
  }

  /* GC arena */
  pic->arena = allocf(NULL, PIC_ARENA_SIZE * sizeof(struct pic_object *));
  pic->arena_size = PIC_ARENA_SIZE;
  pic->arena_idx = 0;

  if (! pic->arena) {
    goto EXIT_ARENA;
  }

  /* memory heap */
  pic->heap = pic_heap_open(pic);

  /* registries */
  pic->regs = NULL;

  /* symbol table */
  xh_init_str(&pic->syms, sizeof(pic_sym *));

  /* global variables */
  pic->globals = NULL;

  /* macros */
  pic->macros = NULL;

  /* attributes */
  pic->attrs = NULL;

  /* features */
  pic->features = pic_nil_value();

  /* libraries */
  pic->libs = pic_nil_value();
  pic->lib = NULL;

  /* raised error object */
  pic->err = pic_invalid_value();

  /* standard ports */
  pic->xSTDIN = NULL;
  pic->xSTDOUT = NULL;
  pic->xSTDERR = NULL;

  /* parameter table */
  pic->ptable = pic_nil_value();

  /* native stack marker */
  pic->native_stack_start = &t;

  ai = pic_gc_arena_preserve(pic);

#define S(slot,name) pic->slot = pic_intern_cstr(pic, name);

  S(sDEFINE, "define");
  S(sLAMBDA, "lambda");
  S(sIF, "if");
  S(sBEGIN, "begin");
  S(sSETBANG, "set!");
  S(sQUOTE, "quote");
  S(sQUASIQUOTE, "quasiquote");
  S(sUNQUOTE, "unquote");
  S(sUNQUOTE_SPLICING, "unquote-splicing");
  S(sDEFINE_SYNTAX, "define-syntax");
  S(sIMPORT, "import");
  S(sEXPORT, "export");
  S(sDEFINE_LIBRARY, "define-library");
  S(sIN_LIBRARY, "in-library");
  S(sCOND_EXPAND, "cond-expand");
  S(sAND, "and");
  S(sOR, "or");
  S(sELSE, "else");
  S(sLIBRARY, "library");
  S(sONLY, "only");
  S(sRENAME, "rename");
  S(sPREFIX, "prefix");
  S(sEXCEPT, "except");
  S(sCONS, "cons");
  S(sCAR, "car");
  S(sCDR, "cdr");
  S(sNILP, "null?");
  S(sSYMBOLP, "symbol?");
  S(sPAIRP, "pair?");
  S(sADD, "+");
  S(sSUB, "-");
  S(sMUL, "*");
  S(sDIV, "/");
  S(sMINUS, "minus");
  S(sEQ, "=");
  S(sLT, "<");
  S(sLE, "<=");
  S(sGT, ">");
  S(sGE, ">=");
  S(sNOT, "not");
  S(sREAD, "read");
  S(sFILE, "file");
  S(sCALL, "call");
  S(sTAILCALL, "tail-call");
  S(sGREF, "gref");
  S(sLREF, "lref");
  S(sCREF, "cref");
  S(sRETURN, "return");
  S(sCALL_WITH_VALUES, "call-with-values");
  S(sTAILCALL_WITH_VALUES, "tailcall-with-values");

  pic_gc_arena_restore(pic, ai);

#define R(slot,name) pic->slot = pic_gensym(pic, pic_intern_cstr(pic, name));

  R(rDEFINE, "define");
  R(rLAMBDA, "lambda");
  R(rIF, "if");
  R(rBEGIN, "begin");
  R(rSETBANG, "set!");
  R(rQUOTE, "quote");
  R(rDEFINE_SYNTAX, "define-syntax");
  R(rIMPORT, "import");
  R(rEXPORT, "export");
  R(rDEFINE_LIBRARY, "define-library");
  R(rIN_LIBRARY, "in-library");
  R(rCOND_EXPAND, "cond-expand");
  R(rCONS, "cons");
  R(rCAR, "car");
  R(rCDR, "cdr");
  R(rNILP, "null?");
  R(rSYMBOLP, "symbol?");
  R(rPAIRP, "pair?");
  R(rADD, "+");
  R(rSUB, "-");
  R(rMUL, "*");
  R(rDIV, "/");
  R(rEQ, "=");
  R(rLT, "<");
  R(rLE, "<=");
  R(rGT, ">");
  R(rGE, ">=");
  R(rNOT, "not");
  R(rVALUES, "values");
  R(rCALL_WITH_VALUES, "call-with-values");
  pic_gc_arena_restore(pic, ai);

  /* root tables */
  pic->globals = pic_make_dict(pic);
  pic->macros = pic_make_dict(pic);
  pic->attrs = pic_make_reg(pic);

  /* root block */
  pic->cp = pic_malloc(pic, sizeof(pic_checkpoint));
  pic->cp->prev = NULL;
  pic->cp->depth = 0;
  pic->cp->in = pic->cp->out = NULL;

  /* reader */
  pic->reader = pic_reader_open(pic);

  /* standard I/O */
  pic->xSTDIN = pic_make_standard_port(pic, xstdin, PIC_PORT_IN);
  pic->xSTDOUT = pic_make_standard_port(pic, xstdout, PIC_PORT_OUT);
  pic->xSTDERR = pic_make_standard_port(pic, xstderr, PIC_PORT_OUT);

  /* parameter table */
  pic->ptable = pic_cons(pic, pic_obj_value(pic_make_dict(pic)), pic->ptable);

  /* standard libraries */
  pic->PICRIN_BASE = pic_make_library(pic, pic_read_cstr(pic, "(picrin base)"));
  pic->PICRIN_USER = pic_make_library(pic, pic_read_cstr(pic, "(picrin user)"));
  pic->lib = pic->PICRIN_USER;
  pic->prev_lib = NULL;

  pic_gc_arena_restore(pic, ai);

  /* turn on GC */
  pic->gc_enable = true;

  pic_init_core(pic);

  pic_gc_arena_restore(pic, ai);

  return pic;

 EXIT_ARENA:
  allocf(pic->xp, 0);
 EXIT_XP:
  allocf(pic->ci, 0);
 EXIT_CI:
  allocf(pic->sp, 0);
 EXIT_SP:
  allocf(pic, 0);
 EXIT_PIC:
  return NULL;
}

void
pic_close(pic_state *pic)
{
  xh_entry *it;
  pic_allocf allocf = pic->allocf;

  /* invoke exit handlers */
  while (pic->cp) {
    if (pic->cp->out) {
      pic_apply0(pic, pic->cp->out);
    }
    pic->cp = pic->cp->prev;
  }

  /* free symbol names */
  for (it = xh_begin(&pic->syms); it != NULL; it = xh_next(it)) {
    allocf(xh_key(it, char *), 0);
  }

  /* clear out root objects */
  pic->sp = pic->stbase;
  pic->ci = pic->cibase;
  pic->xp = pic->xpbase;
  pic->arena_idx = 0;
  pic->err = pic_invalid_value();
  pic->globals = NULL;
  pic->macros = NULL;
  pic->attrs = NULL;
  xh_clear(&pic->syms);
  pic->features = pic_nil_value();
  pic->libs = pic_nil_value();

  /* free all heap objects */
  pic_gc_run(pic);

  /* free heaps */
  pic_heap_close(pic, pic->heap);

  /* free reader struct */
  pic_reader_close(pic, pic->reader);

  /* free runtime context */
  allocf(pic->stbase, 0);
  allocf(pic->cibase, 0);
  allocf(pic->xpbase, 0);

  /* free global stacks */
  xh_destroy(&pic->syms);

  /* free GC arena */
  allocf(pic->arena, 0);

  allocf(pic, 0);
}
