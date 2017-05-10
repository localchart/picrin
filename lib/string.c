/**
 * See Copyright Notice in picrin.h
 */

#include "picrin.h"
#include "value.h"
#include "object.h"

struct rope {
  int refcnt;
  int weight;
  bool isleaf;
  union {
    struct {
      struct rope *owner;
      const char *str;          /* always points to zero-term'd buf */
    } leaf;
    struct {
      struct rope *left, *right;
    } node;
  } u;
  char buf[1];
};

struct rope *
pic_rope_incref(struct rope *rope) {
  rope->refcnt++;
  return rope;
}

void
pic_rope_decref(pic_state *pic, struct rope *rope) {
  if (! --rope->refcnt) {
    if (rope->isleaf) {
      if (rope->u.leaf.owner) {
        pic_rope_decref(pic, rope->u.leaf.owner);
      }
    } else {
      pic_rope_decref(pic, rope->u.node.left);
      pic_rope_decref(pic, rope->u.node.right);
    }
    pic_free(pic, rope);
  }
}

static struct rope *
make_rope_leaf(pic_state *pic, const char *str, int len)
{
  struct rope *rope;

  rope = pic_malloc(pic, offsetof(struct rope, buf) + len + 1);
  rope->refcnt = 1;
  rope->weight = len;
  rope->isleaf = true;
  rope->u.leaf.owner = NULL;
  rope->u.leaf.str = rope->buf;
  rope->buf[len] = 0;
  if (str) {
    memcpy(rope->buf, str, len);
  }

  return rope;
}

static struct rope *
make_rope_lit(pic_state *pic, const char *str, int len)
{
  struct rope *rope;

  rope = pic_malloc(pic, offsetof(struct rope, buf));
  rope->refcnt = 1;
  rope->weight = len;
  rope->isleaf = true;
  rope->u.leaf.owner = NULL;
  rope->u.leaf.str = str;

  return rope;
}

static struct rope *
make_rope_slice(pic_state *pic, struct rope *owner, int i, int j)
{
  struct rope *rope, *real_owner;

  assert(owner->isleaf);

  real_owner = owner->u.leaf.owner == NULL ? owner : owner->u.leaf.owner;

  rope = pic_malloc(pic, offsetof(struct rope, buf));
  rope->refcnt = 1;
  rope->weight = j - i;
  rope->isleaf = true;
  rope->u.leaf.owner = real_owner;
  rope->u.leaf.str = owner->u.leaf.str + i;

  pic_rope_incref(real_owner);

  return rope;
}

static struct rope *
make_rope_node(pic_state *pic, struct rope *left, struct rope *right)
{
  struct rope *rope;

  rope = pic_malloc(pic, sizeof(struct rope));
  rope->refcnt = 1;
  rope->weight = left->weight + right->weight;
  rope->isleaf = false;
  rope->u.node.left = pic_rope_incref(left);
  rope->u.node.right = pic_rope_incref(right);

  return rope;
}

static pic_value
make_str(pic_state *pic, struct rope *rope)
{
  struct string *str;

  str = (struct string *)pic_obj_alloc(pic, PIC_TYPE_STRING);
  str->rope = rope;             /* delegate ownership */

  return obj_value(pic, str);
}

static struct rope *
merge(pic_state *pic, struct rope *left, struct rope *right)
{
  if (left == 0)
    return pic_rope_incref(right);
  if (right == 0)
    return pic_rope_incref(left);

  return make_rope_node(pic, left, right);
}

static struct rope *
slice(pic_state *pic, struct rope *rope, int i, int j)
{
  int lweight;

  if (i == 0 && rope->weight == j) {
    return pic_rope_incref(rope);
  }

  if (rope->isleaf) {
    return make_rope_slice(pic, rope, i, j);
  }

  lweight = rope->u.node.left->weight;

  if (j <= lweight) {
    return slice(pic, rope->u.node.left, i, j);
  } else if (lweight <= i) {
    return slice(pic, rope->u.node.right, i - lweight, j - lweight);
  } else {
    struct rope *r, *l;

    l = slice(pic, rope->u.node.left, i, lweight);
    r = slice(pic, rope->u.node.right, 0, j - lweight);
    rope = merge(pic, l, r);

    pic_rope_decref(pic, l);
    pic_rope_decref(pic, r);

    return rope;
  }
}

static void
flatten(pic_state *pic, struct rope *rope, struct rope *owner, char *buf)
{
  if (rope->isleaf) {
    memcpy(buf, rope->u.leaf.str, rope->weight);
  } else {
    flatten(pic, rope->u.node.left, owner, buf);
    flatten(pic, rope->u.node.right, owner, buf + rope->u.node.left->weight);
  }

  /* path compression */

  if (! rope->isleaf) {
    pic_rope_incref(owner);
    pic_rope_decref(pic, rope->u.node.left);
    pic_rope_decref(pic, rope->u.node.right);
    rope->isleaf = true;
    rope->u.leaf.owner = owner;
    rope->u.leaf.str = buf;
  }
}

static void
str_update(pic_state *pic, pic_value dst, pic_value src)
{
  pic_rope_incref(str_ptr(pic, src)->rope);
  pic_rope_decref(pic, str_ptr(pic, dst)->rope);
  str_ptr(pic, dst)->rope = str_ptr(pic, src)->rope;
}

pic_value
pic_str_value(pic_state *pic, const char *str, int len)
{
  struct rope *r;

  if (len > 0) {
    r = make_rope_leaf(pic, str, len);
  } else {
    if (len == 0) {
      str = "";
    }
    r = make_rope_lit(pic, str, -len);
  }
  return make_str(pic, r);
}

pic_value
pic_cstr_value(pic_state *pic, const char *cstr)
{
  return pic_str_value(pic, cstr, strlen(cstr));
}

pic_value
pic_strf_value(pic_state *pic, const char *fmt, ...)
{
  va_list ap;
  pic_value str;

  va_start(ap, fmt);
  str = pic_vstrf_value(pic, fmt, ap);
  va_end(ap);
  return str;
}

pic_value
pic_vstrf_value(pic_state *pic, const char *fmt, va_list ap)
{
  pic_value str, str2;
  const char *p;

  for (p = fmt; *p; p++) {
    if (*p == '%')
      break;
  }
  str = pic_str_value(pic, fmt, p - fmt);
  if (*p == 0) {
    return str;
  }
  p++;                          /* skip '%' */
  switch (*p++) {
  case '\0':
    return pic_str_value(pic, fmt, p - fmt - 1);
  case 'd':
  case 'i': {
    int i = va_arg(ap, int);
    str2 = pic_funcall(pic, "number->string", 1, pic_int_value(pic, i));
    break;
  }
  case 'f': {
    double f = va_arg(ap, double);
    str2 = pic_funcall(pic, "number->string", 1, pic_float_value(pic, f));
    break;
  }
  case 'c': {
    char c = (char) va_arg(ap, int);
    str2 = pic_str_value(pic, &c, 1);
    break;
  }
  case 's': {
    char *sval = va_arg(ap, char*);
    str2 = pic_cstr_value(pic, sval);
    break;
  }
  case 'p': {
    static const char digits[] = "0123456789abcdef";
    static const size_t bufsiz = sizeof(long) * CHAR_BIT / 4;
    unsigned long vp = (unsigned long) va_arg(ap, void*);
    char buf[2 + bufsiz + 1] = "0x", *p = buf + 2;
    size_t i;
    for (i = 0; i < bufsiz; ++i) {
      p[bufsiz - i - 2] = digits[vp % 16];
      vp /= 16;
    }
    p[i] = '\0';
    str2 = pic_cstr_value(pic, buf);
    break;
  }
  case '%':
    str2 = pic_str_value(pic, &p[-1], 1);
    break;
  }
  return pic_str_cat(pic, str, pic_str_cat(pic, str2, pic_vstrf_value(pic, p, ap)));
}

int
pic_str_len(pic_state *pic, pic_value str)
{
  return str_ptr(pic, str)->rope->weight;
}

pic_value
pic_str_cat(pic_state *pic, pic_value a, pic_value b)
{
  return make_str(pic, merge(pic, str_ptr(pic, a)->rope, str_ptr(pic, b)->rope));
}

pic_value
pic_str_sub(pic_state *pic, pic_value str, int s, int e)
{
  return make_str(pic, slice(pic, str_ptr(pic, str)->rope, s, e));
}

int
pic_str_hash(pic_state *pic, pic_value str)
{
  int len, h = 0;
  const char *s;

  s = pic_str(pic, str, &len);
  while (len-- > 0) {
    h = (h << 5) - h + *s++;
  }
  return h;
}

int
pic_str_cmp(pic_state *pic, pic_value str1, pic_value str2)
{
  int len1, len2, r;
  const char *buf1, *buf2;

  buf1 = pic_str(pic, str1, &len1);
  buf2 = pic_str(pic, str2, &len2);

  if (len1 == len2) {
    return memcmp(buf1, buf2, len1);
  }
  r = memcmp(buf1, buf2, (len1 < len2 ? len1 : len2));
  if (r != 0) {
    return r;
  }
  return len1 - len2;
}

const char *
pic_str(pic_state *pic, pic_value str, int *len)
{
  struct rope *rope = str_ptr(pic, str)->rope, *r;

  if (len) {
    *len = rope->weight;
  }

  if (rope->isleaf && rope->u.leaf.str[rope->weight] == '\0') {
    return rope->u.leaf.str;
  }

  r = make_rope_leaf(pic, 0, rope->weight);

  flatten(pic, rope, r, r->buf);

  return r->u.leaf.str;
}

const char *
pic_cstr(pic_state *pic, pic_value str, int *len)
{
  const char *buf;
  int l;

  buf = pic_str(pic, str, &l);
  if (strchr(buf, '\0') != buf + l) {
    pic_error(pic, "casting scheme string containing null character to c string", 1, str);
  }
  if (len) {
    *len = l;
  }
  return buf;
}

static pic_value
pic_str_string_p(pic_state *pic)
{
  pic_value v;

  pic_get_args(pic, "o", &v);

  return pic_bool_value(pic, pic_str_p(pic, v));
}

static pic_value
pic_str_string(pic_state *pic)
{
  int argc, i;
  pic_value *argv;
  char *buf;

  pic_get_args(pic, "*", &argc, &argv);

  buf = pic_alloca(pic, argc);

  for (i = 0; i < argc; ++i) {
    TYPE_CHECK(pic, argv[i], char);
    buf[i] = pic_char(pic, argv[i]);
  }

  return pic_str_value(pic, buf, argc);
}

static pic_value
pic_str_make_string(pic_state *pic)
{
  int len;
  char c = ' ';
  char *buf;

  pic_get_args(pic, "i|c", &len, &c);

  if (len < 0) {
    pic_error(pic, "make-string: negative length given", 1, pic_int_value(pic, len));
  }

  buf = pic_alloca(pic, len);

  memset(buf, c, len);

  return pic_str_value(pic, buf, len);
}

static pic_value
pic_str_string_length(pic_state *pic)
{
  pic_value str;

  pic_get_args(pic, "s", &str);

  return pic_int_value(pic, pic_str_len(pic, str));
}

static pic_value
pic_str_string_ref(pic_state *pic)
{
  pic_value str;
  int k;

  pic_get_args(pic, "si", &str, &k);

  VALID_INDEX(pic, pic_str_len(pic, str), k);

  return pic_char_value(pic, pic_str(pic, str, 0)[k]);
}

static pic_value
pic_str_string_set(pic_state *pic)
{
  pic_value str, x, y, z;
  char c;
  int k, len;

  pic_get_args(pic, "sic", &str, &k, &c);

  len = pic_str_len(pic, str);

  VALID_INDEX(pic, len, k);

  x = pic_str_sub(pic, str, 0, k);
  y = pic_str_value(pic, &c, 1);
  z = pic_str_sub(pic, str, k + 1, len);

  str_update(pic, str, pic_str_cat(pic, x, pic_str_cat(pic, y, z)));

  return pic_undef_value(pic);
}

#define DEFINE_STRING_CMP(name, op)                             \
  static pic_value                                              \
  pic_str_string_##name(pic_state *pic)                         \
  {                                                             \
    int argc, i;                                                \
    pic_value *argv;                                            \
                                                                \
    pic_get_args(pic, "*", &argc, &argv);                       \
                                                                \
    if (argc < 1 || ! pic_str_p(pic, argv[0])) {                \
      return pic_false_value(pic);                              \
    }                                                           \
                                                                \
    for (i = 1; i < argc; ++i) {                                \
      if (! pic_str_p(pic, argv[i])) {                          \
        return pic_false_value(pic);                            \
      }                                                         \
      if (! (pic_str_cmp(pic, argv[i-1], argv[i]) op 0)) {      \
        return pic_false_value(pic);                            \
      }                                                         \
    }                                                           \
    return pic_true_value(pic);                                 \
  }

DEFINE_STRING_CMP(eq, ==)
DEFINE_STRING_CMP(lt, <)
DEFINE_STRING_CMP(gt, >)
DEFINE_STRING_CMP(le, <=)
DEFINE_STRING_CMP(ge, >=)

static pic_value
pic_str_string_copy(pic_state *pic)
{
  pic_value str;
  int n, start, end, len;

  n = pic_get_args(pic, "s|ii", &str, &start, &end);

  len = pic_str_len(pic, str);

  switch (n) {
  case 1:
    start = 0;
  case 2:
    end = len;
  }

  VALID_RANGE(pic, len, start, end);

  return pic_str_sub(pic, str, start, end);
}

static pic_value
pic_str_string_copy_ip(pic_state *pic)
{
  pic_value to, from, x, y, z;
  int n, at, start, end, tolen, fromlen;

  n = pic_get_args(pic, "sis|ii", &to, &at, &from, &start, &end);

  tolen = pic_str_len(pic, to);
  fromlen = pic_str_len(pic, from);

  switch (n) {
  case 3:
    start = 0;
  case 4:
    end = fromlen;
  }

  VALID_ATRANGE(pic, tolen, at, fromlen, start, end);

  x = pic_str_sub(pic, to, 0, at);
  y = pic_str_sub(pic, from, start, end);
  z = pic_str_sub(pic, to, at + end - start, tolen);

  str_update(pic, to, pic_str_cat(pic, x, pic_str_cat(pic, y, z)));

  return pic_undef_value(pic);
}

static pic_value
pic_str_string_fill_ip(pic_state *pic)
{
  pic_value str, x, y, z;
  char c, *buf;
  int n, start, end, len;

  n = pic_get_args(pic, "sc|ii", &str, &c, &start, &end);

  len = pic_str_len(pic, str);

  switch (n) {
  case 2:
    start = 0;
  case 3:
    end = len;
  }

  VALID_RANGE(pic, len, start, end);

  buf = pic_alloca(pic, end - start);
  memset(buf, c, end - start);

  x = pic_str_sub(pic, str, 0, start);
  y = pic_str_value(pic, buf, end - start);
  z = pic_str_sub(pic, str, end, len);

  str_update(pic, str, pic_str_cat(pic, x, pic_str_cat(pic, y, z)));

  return pic_undef_value(pic);
}

static pic_value
pic_str_string_append(pic_state *pic)
{
  int argc, i;
  pic_value *argv;
  pic_value str = pic_lit_value(pic, "");

  pic_get_args(pic, "*", &argc, &argv);

  for (i = 0; i < argc; ++i) {
    TYPE_CHECK(pic, argv[i], str);
    str = pic_str_cat(pic, str, argv[i]);
  }
  return str;
}

static pic_value
pic_str_string_map(pic_state *pic)
{
  pic_value proc, *argv, vals, val;
  int argc, i, len, j;
  char *buf;

  pic_get_args(pic, "l*", &proc, &argc, &argv);

  if (argc == 0) {
    pic_error(pic, "string-map: one or more strings expected, but got zero", 0);
  }

  len = INT_MAX;
  for (i = 0; i < argc; ++i) {
    int l;
    TYPE_CHECK(pic, argv[i], str);
    l = pic_str_len(pic, argv[i]);
    len = len < l ? len : l;
  }

  buf = pic_alloca(pic, len);

  for (i = 0; i < len; ++i) {
    vals = pic_nil_value(pic);
    for (j = 0; j < argc; ++j) {
      pic_push(pic, pic_char_value(pic, pic_str(pic, argv[j], 0)[i]), vals);
    }
    vals = pic_reverse(pic, vals);
    val = pic_funcall(pic, "apply", 2, proc, vals);

    TYPE_CHECK(pic, val, char);

    buf[i] = pic_char(pic, val);
  }
  return pic_str_value(pic, buf, len);
}

static pic_value
pic_str_string_for_each(pic_state *pic)
{
  pic_value proc, *argv, vals;
  int argc, i, len, j;

  pic_get_args(pic, "l*", &proc, &argc, &argv);

  if (argc == 0) {
    pic_error(pic, "string-map: one or more strings expected, but got zero", 0);
  }

  len = INT_MAX;
  for (i = 0; i < argc; ++i) {
    int l;
    TYPE_CHECK(pic, argv[i], str);
    l = pic_str_len(pic, argv[i]);
    len = len < l ? len : l;
  }

  for (i = 0; i < len; ++i) {
    vals = pic_nil_value(pic);
    for (j = 0; j < argc; ++j) {
      pic_push(pic, pic_char_value(pic, pic_str(pic, argv[j], 0)[i]), vals);
    }
    vals = pic_reverse(pic, vals);
    pic_funcall(pic, "apply", 2, proc, vals);
  }
  return pic_undef_value(pic);
}

static pic_value
pic_str_list_to_string(pic_state *pic)
{
  pic_value list, e, it;
  int i;
  char *buf;

  pic_get_args(pic, "o", &list);

  buf = pic_alloca(pic, pic_length(pic, list));

  i = 0;
  pic_for_each (e, list, it) {
    TYPE_CHECK(pic, e, char);

    buf[i++] = pic_char(pic, e);
  }

  return pic_str_value(pic, buf, i);
}

static pic_value
pic_str_string_to_list(pic_state *pic)
{
  pic_value str, list;
  int n, start, end, len, i;

  n = pic_get_args(pic, "s|ii", &str, &start, &end);

  len = pic_str_len(pic, str);

  switch (n) {
  case 1:
    start = 0;
  case 2:
    end = len;
  }

  VALID_RANGE(pic, len, start, end);

  list = pic_nil_value(pic);
  for (i = start; i < end; ++i) {
    pic_push(pic, pic_char_value(pic, pic_str(pic, str, 0)[i]), list);
  }
  return pic_reverse(pic, list);
}

void
pic_init_str(pic_state *pic)
{
  pic_defun(pic, "string?", pic_str_string_p);
  pic_defun(pic, "string", pic_str_string);
  pic_defun(pic, "make-string", pic_str_make_string);
  pic_defun(pic, "string-length", pic_str_string_length);
  pic_defun(pic, "string-ref", pic_str_string_ref);
  pic_defun(pic, "string-set!", pic_str_string_set);
  pic_defun(pic, "string-copy", pic_str_string_copy);
  pic_defun(pic, "string-copy!", pic_str_string_copy_ip);
  pic_defun(pic, "string-fill!", pic_str_string_fill_ip);
  pic_defun(pic, "string-append", pic_str_string_append);
  pic_defun(pic, "string-map", pic_str_string_map);
  pic_defun(pic, "string-for-each", pic_str_string_for_each);
  pic_defun(pic, "list->string", pic_str_list_to_string);
  pic_defun(pic, "string->list", pic_str_string_to_list);

  pic_defun(pic, "string=?", pic_str_string_eq);
  pic_defun(pic, "string<?", pic_str_string_lt);
  pic_defun(pic, "string>?", pic_str_string_gt);
  pic_defun(pic, "string<=?", pic_str_string_le);
  pic_defun(pic, "string>=?", pic_str_string_ge);
}
