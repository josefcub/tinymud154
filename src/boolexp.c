#include "copyright.h"

#include <ctype.h>
#include <stdlib.h>

#include "db.h"
#include "match.h"
#include "externs.h"
#include "config.h"
#include "interface.h"

int eval_boolexp(dbref player, struct boolexp *b)
{
    if(b == TRUE_BOOLEXP) {
	return 1;
    } else {
	switch(b->type) {
	  case BOOLEXP_AND:
	    return (eval_boolexp(player, b->sub1)
		    && eval_boolexp(player, b->sub2));
	  case BOOLEXP_OR:
	    return (eval_boolexp(player, b->sub1)
		    || eval_boolexp(player, b->sub2));
	  case BOOLEXP_NOT:
	    return !eval_boolexp(player, b->sub1);
	  case BOOLEXP_CONST:
	    return (b->thing == player
		    || member(b->thing, db[player].contents));
	  default:
	    abort();		/* bad type */
	    return 0;
	}
    }
}

/* If the parser returns TRUE_BOOLEXP, you lose */
/* TRUE_BOOLEXP cannot be typed in by the user; use @unlock instead */
static const char *parsebuf;
static dbref parse_player;

static void skip_whitespace(void)
{
    while(*parsebuf && isspace(*parsebuf)) parsebuf++;
}

static struct boolexp *parse_boolexp_E(void); /* defined below */

/* F -> (E); F -> !F; F -> object identifier */
static struct boolexp *parse_boolexp_F(void)
{
    struct boolexp *b;
    char *p;
    char buf[BUFFER_LEN];
    char msg[BUFFER_LEN];

    skip_whitespace();
    switch(*parsebuf) {
      case '(':
	parsebuf++;
	b = parse_boolexp_E();
	skip_whitespace();
	if(b == TRUE_BOOLEXP || *parsebuf++ != ')') {
	    free_boolexp(b);
	    return TRUE_BOOLEXP;
	} else {
	    return b;
	}
	/* break; */
      case NOT_TOKEN:
	parsebuf++;
	b = (struct boolexp *) malloc(sizeof(struct boolexp));
	b->type = BOOLEXP_NOT;
	b->sub1 = parse_boolexp_F();
	if(b->sub1 == TRUE_BOOLEXP) {
	    free((void *) b);
	    return TRUE_BOOLEXP;
	} else {
	    return b;
	}
	/* break */
      default:
	/* must have hit an object ref */
	/* load the name into our buffer */
	p = buf;
	while(*parsebuf
	      && *parsebuf != AND_TOKEN
	      && *parsebuf != OR_TOKEN
	      && *parsebuf != ')') {
	    *p++ = *parsebuf++;
	}
	/* strip trailing whitespace */
	*p-- = '\0';
	while(isspace(*p)) *p-- = '\0';

	b = (struct boolexp *) malloc(sizeof(struct boolexp));
	b->type = BOOLEXP_CONST;

	/* do the match */
	init_match(parse_player, buf, TYPE_THING);
	match_neighbor();
	match_possession();
	match_me();
	match_absolute();
	match_player();
	b->thing = match_result();

	if(b->thing == NOTHING) {
	    sprintf(msg, "I don't see %s here.", buf);
	    notify(parse_player, msg);
	    free((void *) b);
	    return TRUE_BOOLEXP;
	} else if(b->thing == AMBIGUOUS) {
	    sprintf(msg, "I don't know which %s you mean!", buf);
	    notify(parse_player, msg);
	    free((void *) b);
	    return TRUE_BOOLEXP;
	} else {
	    return b;
	}
	/* break */
    }
}

/* T -> F; T -> F & T */
static struct boolexp *parse_boolexp_T(void)
{
    struct boolexp *b;
    struct boolexp *b2;

    if((b = parse_boolexp_F()) == TRUE_BOOLEXP) {
	return b;
    } else {
	skip_whitespace();
	if(*parsebuf == AND_TOKEN) {
	    parsebuf++;

	    b2 = (struct boolexp *) malloc(sizeof(struct boolexp));
	    b2->type = BOOLEXP_AND;
	    b2->sub1 = b;
	    if((b2->sub2 = parse_boolexp_T()) == TRUE_BOOLEXP) {
		free_boolexp(b2);
		return TRUE_BOOLEXP;
	    } else {
		return b2;
	    }
	} else {
	    return b;
	}
    }
}

/* E -> T; E -> T | E */
static struct boolexp *parse_boolexp_E(void)
{
    struct boolexp *b;
    struct boolexp *b2;

    if((b = parse_boolexp_T()) == TRUE_BOOLEXP) {
	return b;
    } else {
	skip_whitespace();
	if(*parsebuf == OR_TOKEN) {
	    parsebuf++;

	    b2 = (struct boolexp *) malloc(sizeof(struct boolexp));
	    b2->type = BOOLEXP_OR;
	    b2->sub1 = b;
	    if((b2->sub2 = parse_boolexp_E()) == TRUE_BOOLEXP) {
		free_boolexp(b2);
		return TRUE_BOOLEXP;
	    } else {
		return b2;
	    }
	} else {
	    return b;
	}
    }
}

struct boolexp *parse_boolexp(dbref player, const char *buf)
{
    parsebuf = buf;
    parse_player = player;
    return parse_boolexp_E();
}
