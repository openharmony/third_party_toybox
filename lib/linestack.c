#include "toys.h"

// The design idea here is indexing a big blob of (potentially mmaped) data
// instead of copying the data into a zillion seperate malloc()s.

// A linestack is an array of struct ptr_len, with a currently used len
// and max tracking the memory allocation. This indexes existing string data,
// the lifetime of which is tracked externally.

// Insert one stack into another before position in old stack.
// (Does not copy contents of strings, just shuffles index array contents.)
void linestack_addstack(struct linestack **lls, struct linestack *throw,
  long pos)
{
  struct linestack *catch = *lls;

  if (CFG_TOYBOX_DEBUG)
    if (pos > catch->len) error_exit("linestack_addstack past end.");

  // Make a hole, allocating more space if necessary.
  if (catch->len+throw->len >= catch->max) {
    // New size rounded up to next multiple of 64, allocate and copy start.
    catch->max = ((catch->len+throw->len)|63)+1;
    *lls = xmalloc(sizeof(struct linestack)+catch->max*sizeof(struct ptr_len));
    memcpy(*lls, catch, sizeof(struct linestack)+pos*sizeof(struct ptr_len));
  }

  // Copy end (into new allocation if necessary)
  if (pos != catch->len)
    memmove((*lls)->idx+pos+throw->len, catch->idx+pos,
      (catch->len-pos)*sizeof(struct ptr_len));

  // Cleanup if we had to realloc.
  if (catch != *lls) {
    free(catch);
    catch = *lls;
  }

  // Copy new chunk we made space for
  memcpy(catch->idx+pos, throw->idx, throw->len*sizeof(struct ptr_len));
  catch->len += throw->len;
}

// Insert one line/len into a linestack at pos
void linestack_insert(struct linestack **lls, long pos, char *line, long len)
{
  // alloca() was in 32V and Turbo C for DOS, but isn't in posix or c99.
  // This allocates enough memory for the linestack to have one ptr_len.
  // (Even if a compiler adds gratuitous padidng that just makes it bigger.)
  struct {
    struct ptr_len pl;
    struct linestack ls;
  } ls;

  ls.ls.len = ls.ls.max = 1;
  ls.ls.idx[0].ptr = line;
  ls.ls.idx[0].len = len;
  linestack_addstack(lls, &ls.ls, pos);
}

void linestack_append(struct linestack **lls, char *line)
{
  linestack_insert(lls, (*lls)->len, line, strlen(line));
}

struct linestack *linestack_load(char *name)
{
  FILE *fp = fopen(name, "r");
  struct linestack *ls;

  if (!fp) return 0;

  ls = xzalloc(sizeof(struct linestack));

  for (;;) {
    char *line = 0;
    ssize_t len;

    if ((len = getline(&line, (void *)&len, fp))<1) break;
    if (line[len-1]=='\n') len--;
    linestack_insert(&ls, ls->len, line, len);
  }
  fclose(fp);

  return ls;
}

