#include "db.h"

#include "externs.h"
#include "config.h"
#include "interface.h"

#include <stdlib.h>
#include <string.h>

static const char *unparse_flags(dbref thing, dbref player)
{
    static char buf[BUFFER_LEN];
    char *p;
    const char *type_codes = TYPE_CODES;

    p = buf;
    if(Typeof(thing) != TYPE_THING) *p++ = type_codes[Typeof(thing)];
    if(db[thing].flags & ~TYPE_MASK) {
	/* print flags */
	if(db[thing].flags & WIZARD) *p++ = WIZARD_MARK;
#ifdef ROBOT_MODE
	if(db[thing].flags & ROBOT) *p++ = ROBOT_MARK;
#endif
	if(db[thing].flags & STICKY) *p++ = STICKY_MARK;
	if(db[thing].flags & DARK) *p++ = DARK_MARK;
	if(db[thing].flags & LINK_OK) *p++ = LINK_MARK;
	if(db[thing].flags & ABODE) *p++ = ABODE_MARK;
	if(db[thing].flags & TEMPLE) *p++ = TEMPLE_MARK;
#ifdef RESTRICTED_BUILDING
	if(db[thing].flags & BUILDER) *p++ = BUILDER_MARK;
#endif
	if(db[thing].flags & HAVEN) *p++ = HAVEN_MARK;
	if(db[thing].flags & UNWANTED) *p++ = UNWANTED_MARK;
#ifdef GENDER
	if(Genderof(thing) == GENDER_MALE) *p++ = MALE_MARK;
	if(Genderof(thing) == GENDER_FEMALE) *p++ = FEMALE_MARK;
	if(Genderof(thing) == GENDER_NEUTER) *p++ = NEUTER_MARK;
#endif 
    }
    *p = '\0';
    return buf;
}

const char *unparse_object(dbref player, dbref loc)
{
    static char buf[BUFFER_LEN];

    switch(loc) {
      case NOTHING:
	return "*NOTHING*";
      case HOME:
	return "*HOME*";
      default:
	if(controls(player, loc) || can_link_to(player, NOTYPE, loc)
	   || (db[loc].flags & UNWANTED)) {
	    /* show everything */
	    sprintf(buf, "%s(#%d%s)", db[loc].name, loc,
		    unparse_flags(loc, player));
	    return buf;
	} else {
	    /* show only the name */
	    return db[loc].name;
	}
    }
}

static char boolexp_buf[BUFFER_LEN];
static char *buftop;

static void unparse_boolexp1(dbref player,
			     struct boolexp *b, boolexp_type outer_type)
{
    if(b == TRUE_BOOLEXP) {
	strcpy(buftop, "*UNLOCKED*");
	buftop += strlen(buftop);
    } else {
	switch(b->type) {
	  case BOOLEXP_AND:
	    if(outer_type == BOOLEXP_NOT) {
		*buftop++ = '(';
	    }
	    unparse_boolexp1(player, b->sub1, b->type);
	    *buftop++ = AND_TOKEN;
	    unparse_boolexp1(player, b->sub2, b->type);
	    if(outer_type == BOOLEXP_NOT) {
		*buftop++ = ')';
	    }
	    break;
	  case BOOLEXP_OR:
	    if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND) {
		*buftop++ = '(';
	    }
	    unparse_boolexp1(player, b->sub1, b->type);
	    *buftop++ = OR_TOKEN;
	    unparse_boolexp1(player, b->sub2, b->type);
	    if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND) {
		*buftop++ = ')';
	    }
	    break;
	  case BOOLEXP_NOT:
	    *buftop++ = '!';
	    unparse_boolexp1(player, b->sub1, b->type);
	    break;
	  case BOOLEXP_CONST:
	    strcpy(buftop, unparse_object(player, b->thing));
	    buftop += strlen(buftop);
	    break;
	  default:
	    abort();		/* bad type */
	    break;
	}
    }
}
	    
const char *unparse_boolexp(dbref player, struct boolexp *b)
{
    buftop = boolexp_buf;
    unparse_boolexp1(player, b, BOOLEXP_CONST);	/* no outer type */
    *buftop++ = '\0';

    return boolexp_buf;
}
    
