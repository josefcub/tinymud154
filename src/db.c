#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "db.h"
#include "externs.h"
#include "config.h"

struct object *db = 0;
dbref db_top = 0;

#ifdef TEST_MALLOC
int malloc_count = 0;
#endif

#ifndef DB_INITIAL_SIZE
#define DB_INITIAL_SIZE 10000
#endif

#ifdef DB_DOUBLING

dbref db_size = DB_INITIAL_SIZE;
#endif 

const char *alloc_string(const char *string)
{
    char *s;

    /* NULL, "" -> NULL */
    if(string == 0 || *string == '\0') return 0;

    if((s = (char *) malloc(strlen(string)+1)) == 0) {
	abort();
    }
    strcpy(s, string);
    return s;
}

#ifdef DB_DOUBLING

static void db_grow(dbref newtop)
{
    struct object *newdb;

    if(newtop > db_top) {
	db_top = newtop;
	if(!db) {
	    /* make the initial one */
	    db_size = DB_INITIAL_SIZE;
	    if((db = (struct object *)
		malloc(db_size * sizeof(struct object))) == 0) {
		abort();
	    }
	}
	
	/* maybe grow it */
	if(db_top > db_size) {
	    /* make sure it's big enough */
	    while(db_top > db_size) db_size *= 2;
	    if((newdb = (struct object *)
		realloc((void *) db,
			db_size * sizeof(struct object))) == 0) {
		abort();
	    } 
	    db = newdb;
	}
    }
}

#else /* DB_DOUBLING */

static void db_grow(dbref newtop)
{
    struct object *newdb;

    if(newtop > db_top) {
	db_top = newtop;
	if(db) {
	    if((newdb = (struct object *)
		realloc((void *) db,
			db_top * sizeof(struct object))) == 0) {
		abort();
	    } 
	    db = newdb;
	} else {
	    /* make the initial one */
	    if((db = (struct object *)
		malloc(DB_INITIAL_SIZE * sizeof(struct object))) == 0) {
		abort();
	    }
	}
    }
}
#endif 

dbref new_object(void)
{
    dbref newobj;
    struct object *o;

    newobj = db_top;
    db_grow(db_top + 1);

    /* clear it out */
    o = db+newobj;
    o->name = 0;
    o->description = 0;
    o->location = NOTHING;
    o->contents = NOTHING;
    o->exits = NOTHING;
    o->next = NOTHING;
    o->key = TRUE_BOOLEXP;
    o->fail_message = 0;
    o->succ_message = 0;
    o->ofail = 0;
    o->osuccess = 0;
    o->owner = NOTHING;
    o->pennies = 0;
    /* flags you must initialize yourself */
    o->password = 0;
#ifdef TIMESTAMPS
    /* Timestamp entries - Sep 1, 1990 by Fuzzy */
    o->created = time (NULL);
    o->lastused = 0;
    o->usecnt = 0;
#endif 

    return newobj;
}
	
#define DB_MSGLEN 512

void putref(FILE *f, dbref ref)
{
    fprintf(f, "%d\n", ref);
}

static void putstring(FILE *f, const char *s)
{
    if(s) {
	fputs(s, f);
    } 
    putc('\n', f);
}
	
static void putbool_subexp(FILE *f, struct boolexp *b)
{
    switch(b->type) {
      case BOOLEXP_AND:
	putc('(', f);
	putbool_subexp(f, b->sub1);
	putc(AND_TOKEN, f);
	putbool_subexp(f, b->sub2);
	putc(')', f);
	break;
      case BOOLEXP_OR:
	putc('(', f);
	putbool_subexp(f, b->sub1);
	putc(OR_TOKEN, f);
	putbool_subexp(f, b->sub2);
	putc(')', f);
	break;
      case BOOLEXP_NOT:
	putc('(', f);
	putc(NOT_TOKEN, f);
	putbool_subexp(f, b->sub1);
	putc(')', f);
	break;
      case BOOLEXP_CONST:
	fprintf(f, "%d", b->thing);
	break;
      default:
	break;
    }
}

void putboolexp(FILE *f, struct boolexp *b)
{
    if(b != TRUE_BOOLEXP) {
	putbool_subexp(f, b);
    }
    putc('\n', f);
}
	
int db_write_object(FILE *f, dbref i)
{
    struct object *o;

    o = db + i;
    putstring(f, o->name);
    putstring(f, o->description);
    putref(f, o->location);
    putref(f, o->contents);
    putref(f, o->exits);
    putref(f, o->next);
    putboolexp(f, o->key);
    putstring(f, o->fail_message);
    putstring(f, o->succ_message);
    putstring(f, o->ofail);
    putstring(f, o->osuccess);
    putref(f, o->owner);
    putref(f, o->pennies);
    putref(f, (o->flags));
    putstring(f, o->password);
#ifdef TIMESTAMPS
    putref(f, o->created);
    putref(f, o->lastused);
    putref(f, o->usecnt);
#endif 

    return 0;
}

dbref db_write(FILE *f)
{
    dbref i;

    for(i = 0; i < db_top; i++) {
	fprintf(f, "#%d\n", i);
	db_write_object(f, i);
    }
    fputs("***END OF DUMP***\n", f);
    fflush(f);
    return(db_top);
}

dbref parse_dbref(const char *s)
{
    const char *p;
    long x;

    x = atol(s);
    if(x > 0) {
	return x;
    } else if(x == 0) {
	/* check for 0 */
	for(p = s; *p; p++) {
	    if(*p == '0') return 0;
	    if(!isspace(*p)) break;
	}
    }

    /* else x < 0 or s != 0 */
    return NOTHING;
}
	    
static int do_peek (FILE *f)
{ int peekch;

  ungetc ((peekch = getc (f)), f);
  
  return (peekch);
}

dbref getref(FILE *f)
{
    static char buf[DB_MSGLEN];
    int peekch;
    char * fgets_result;

    /* Compiled in with or without timestamps, Sep 1, 1990 by Fuzzy */    
    if ((peekch = do_peek (f)) == '#' || peekch == '*') {
	return (0);
    }

    fgets_result = fgets(buf, sizeof(buf), f);

		return(atol(buf));
}

static const char *getstring_noalloc(FILE *f)
{
    static char buf[DB_MSGLEN];
    char *p;
    char * fgets_result;

    fgets_result = fgets(buf, sizeof(buf), f);

		for(p = buf; *p; p++) {
	if(*p == '\n') {
	    *p = '\0';
	    break;
	}
    }

    return buf;
}

#define getstring(x) alloc_string(getstring_noalloc(x))

#ifdef COMPRESS
extern const char *compress(const char *);
#define getstring_compress(x) alloc_string(compress(getstring_noalloc(x)));
#else
#define getstring_compress(x) getstring(x)
#endif 

static struct boolexp *negate_boolexp(struct boolexp *b)
{
    struct boolexp *n;

    /* Obscure fact: !NOTHING == NOTHING in old-format databases! */
    if(b == TRUE_BOOLEXP) return TRUE_BOOLEXP;

    n = (struct boolexp *) malloc(sizeof(struct boolexp));
    n->type = BOOLEXP_NOT;
    n->sub1 = b;

    return n;
}

static struct boolexp *getboolexp1(FILE *f)
{
    struct boolexp *b;
    int c;

    c = getc(f);
    switch(c) {
      case '\n':
	ungetc(c, f);
	return TRUE_BOOLEXP;
	/* break; */
      case EOF:
	abort();		/* unexpected EOF in boolexp */
	break;
      case '(':
	b = (struct boolexp *) malloc(sizeof(struct boolexp));
	if((c = getc(f)) == '!') {
	    b->type = BOOLEXP_NOT;
	    b->sub1 = getboolexp1(f);
	    if(getc(f) != ')') goto error;
	    return b;
	} else {
	    ungetc(c, f);
	    b->sub1 = getboolexp1(f);
	    switch(c = getc(f)) {
	      case AND_TOKEN:
		b->type = BOOLEXP_AND;
		break;
	      case OR_TOKEN:
		b->type = BOOLEXP_OR;
		break;
	      default:
		goto error;
		/* break */
	    }
	    b->sub2 = getboolexp1(f);
	    if(getc(f) != ')') goto error;
	    return b;
	}
	/* break; */
      case '-':
	/* obsolete NOTHING key */
	/* eat it */
	while((c = getc(f)) != '\n') if(c == EOF) abort(); /* unexp EOF */
	ungetc(c, f);
	return TRUE_BOOLEXP;
	/* break */
      default:
	/* better be a dbref */
	ungetc(c, f);
	b = (struct boolexp *) malloc(sizeof(struct boolexp));
	b->type = BOOLEXP_CONST;
	b->thing = 0;
	    
	/* NOTE possibly non-portable code */
	/* Will need to be changed if putref/getref change */
	while(isdigit(c = getc(f))) {
	    b->thing = b->thing * 10 + c - '0';
	}
	ungetc(c, f);
	return b;
    }

  error:
    abort();			/* bomb out */
    return TRUE_BOOLEXP;
}

struct boolexp *getboolexp(FILE *f)
{
    struct boolexp *b;

    b = getboolexp1(f);
    if(getc(f) != '\n') abort(); /* parse error, we lose */
    return b;
}

void free_boolexp(struct boolexp *b)
{
    if(b != TRUE_BOOLEXP) {
	switch(b->type) {
	  case BOOLEXP_AND:
	  case BOOLEXP_OR:
	    free_boolexp(b->sub1);
	    free_boolexp(b->sub2);
	    free((void *) b);
	    break;
	  case BOOLEXP_NOT:
	    free_boolexp(b->sub1);
	    free((void *) b);
	    break;
	  case BOOLEXP_CONST:
	    free((void *) b);
	    break;
	}
    }
}

void db_free(void)
{
    dbref i;
    struct object *o;

    if(db) {
	for(i = 0; i < db_top; i++) {
	    o = &db[i];
	    if(o->name) free((void *) o->name);
	    if(o->description) free((void *) o->description);
	    if(o->succ_message) free((void *) o->succ_message);
	    if(o->fail_message) free((void *) o->fail_message);
	    if(o->ofail) free((void *) o->ofail);
	    if(o->osuccess) free((void *) o->osuccess);
	    if(o->password) free((void *) o->password);
	    if(o->key) free_boolexp(o->key);
	}
	free((void *) db);
	db = 0;
	db_top = 0;
    }
}

dbref db_read(FILE *f)
{
    dbref i;
    struct object *o;
    const char *end;
    static char buf[DB_MSGLEN];
    int peekch;
    char * fgets_result;

#ifdef PLAYER_LIST
    clear_players();
#endif
    
    db_free();
    for(i = 0;; i++) {
	switch(getc(f)) {
	  case '#':
	    /* another entry, yawn */
	    if(i != getref(f)) {
		/* we blew it */
		return -1;
	    }
	    /* make space */
	    db_grow(i+1);
	    
	    /* read it in */
	    o = db+i;
	    o->name = getstring(f);
	    o->description = getstring_compress(f);
	    o->location = getref(f);
	    o->contents = getref(f);
	    o->exits = getref(f);
	    o->next = getref(f);
	    o->key = getboolexp(f);
	    o->fail_message = getstring_compress(f);
	    o->succ_message = getstring_compress(f);
	    o->ofail = getstring_compress(f);
	    o->osuccess = getstring_compress(f);
	    o->owner = getref(f);
	    o->pennies = getref(f);
	    o->flags = getref(f);
	    o->password = getstring(f);
#ifdef TIMESTAMPS
	    o->created = getref (f);
	    o->lastused = getref (f);
	    o->usecnt = getref (f);
#endif 

	    /* Ignore extra input to next '#' or '*' */
	    while ((peekch = do_peek (f)) != EOF &&
	    	   peekch != '#' && peekch != '*') {
           fgets_result = fgets (buf, sizeof(buf), f);
	    }

	    /* For downward compatibility with databases using the */
	    /* obsolete ANTILOCK flag. */ 
	    if(o->flags & ANTILOCK) {
	      o->key = negate_boolexp(o->key);
	      o->flags &= ~ANTILOCK;
	    }
#ifdef PLAYER_LIST	    
	    if(Typeof(i) == TYPE_PLAYER) {
		add_player(i);
	    }
#endif
	    break;
	  case '*':
	    end = getstring(f);
	    if(strcmp(end, "**END OF DUMP***")) {
		free((void *) end);
		return -1;
	    } else {
		free((void *) end);
		return db_top;
	    }
	  default:
	    return -1;
	    /* break; */
	}
    }
}
		
