#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

#ifndef COMPRESS
#define uncompress(x) (x)
#else
const char *uncompress(const char *s);
#endif 

const char *unparse_object(dbref player, dbref loc);

/* in a dump, you can see everything */
int can_link_to(dbref who, object_flag_type what, dbref where)
{
    return 1;
}

int controls(dbref who, dbref what)
{
    return 1;
}

int main(int argc, char **argv)
{
    struct object *o;
    dbref owner;
    dbref thing;

    if(argc < 1) {
	fprintf(stderr, "Usage: %s [owner]\n", *argv);
	exit(1);
    }
    
    if(argc >= 2) {
	owner = atol(argv[1]);
    } else {
	owner = NOTHING;
    }

    if(db_read(stdin) < 0) {
	fprintf(stderr, "%s: bad input\n", argv[0]);
	exit(5);
    }

    for(o = db; o < db+db_top; o++) {
	/* don't show exits separately */
	if((o->flags & TYPE_MASK) == TYPE_EXIT) continue;

	/* don't show it if it isn't owned by the right player */
	if(owner != NOTHING && o->owner != owner) continue;

	printf("#%d: %s [%s] at %s Pennies: %d Type: ",
	       o - db, o->name, db[o->owner].name,
	       unparse_object(owner, o->location),
	       o->pennies);
	switch(o->flags & TYPE_MASK) {
	  case TYPE_ROOM:
	    printf("Room");
	    break;
	  case TYPE_EXIT:
	    printf("Exit");
	    break;
	  case TYPE_THING:
	    printf("Thing");
	    break;
	  case TYPE_PLAYER:
	    printf("Player");
	    break;
	  default:
	    printf("***UNKNOWN TYPE***");
	    break;
	}

	/* handle flags */
	putchar(' ');
	if(o->flags & ~TYPE_MASK) {
	    printf("Flags: ");
	    if(o->flags & LINK_OK) printf("LINK_OK ");
	    if(o->flags & DARK) printf("DARK ");
	    if(o->flags & STICKY) printf("STICKY ");
	    if(o->flags & WIZARD) printf("WIZARD ");
	    if(o->flags & TEMPLE) printf("TEMPLE ");
#ifdef RESTRICTED_BUILDING
	    if(o->flags & BUILDER) printf("BUILDER ");
#endif
	}
	putchar('\n');
	       
	if(o->key != TRUE_BOOLEXP) printf("KEY: %s\n",
					  unparse_boolexp(owner, o->key));
	if(o->description) {
	    puts("Description:");
	    puts(uncompress(o->description));
	}
	if(o->succ_message) {
	    puts("Success Message:");
	    puts(uncompress(o->succ_message));
	}
	if(o->fail_message) {
	    puts("Fail Message:");
	    puts(uncompress(o->fail_message));
	}
	if(o->ofail) {
	    puts("Other Fail Message:");
	    puts(uncompress(o->ofail));
	}
	if(o->osuccess) {
	    puts("Other Success Message:");
	    puts(uncompress(o->osuccess));
	}
	if(o->contents != NOTHING) {
	    puts("Contents:");
	    DOLIST(thing, o->contents) {
		/* dump thing description */
		putchar(' ');
		puts(unparse_object(owner, thing));
	    }
	}
	if(o->exits != NOTHING) {
	    if((o->flags & TYPE_MASK) == TYPE_ROOM) {
		puts("Exits:");
		DOLIST(thing, o->exits) {
		    printf(" %s", unparse_object(owner, thing));
		    if(db[thing].key != TRUE_BOOLEXP) {
			printf(" KEY: %s",
			       unparse_boolexp(owner, db[thing].key));
		    }
		    if(db[thing].location != NOTHING) {
			printf(" => %s\n",
			       unparse_object(owner, db[thing].location));
		    } else {
			puts(" ***OPEN***");
		    }
		}
	    } else {
		printf("Home: %s\n", unparse_object(owner, o->exits));
	    }
	}
	putchar('\n');
    }

    exit(0);
    return(0);
}
