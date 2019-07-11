#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>

#include "db.h"

long atosec (const char *str);

static int include_all = 0;	/* include everything unless specified */
static int keep_players = 0;	/* keep all players */
static int safe_below = 1;	/* Keep everything <= safe_below */
static int safe_above = 2e9;	/* Keep everything >= safe_above */
static int reachable = 0;	/* Only keep rooms reachable from #0 */
static int norecycle = 0;	/* Exclude things in recycling center */
static int inbuild = 0;		/* True when in main build_trans loop */
static int recycler = 0;	/* Number of player "Recycler" */
static int usecutoff = 0;	/* Max age for player/room */
static long now = 0;		/* Time of the extraction */

# define REACH_FLAG 0x40000000
# define REACHABLE(X) (db[X].flags & REACH_FLAG)
# define SET_REACHABLE(X) (db[X].flags |= REACH_FLAG)

static dbref included[NCARGS+1];
static dbref excluded[NCARGS+1];

static dbref *trans;		/* translation vector */

#define DEFAULT_LOCATION (0)
#define DEFAULT_OWNER (1)

static int isok(dbref);

/* returns 1 if object is specifically excluded */
static int is_excluded(dbref x)
{
    int i;

    if(x == NOTHING) return 0; /* Don't exclude nothing */

    /* check that it isn't excluded */
    for(i = 0; excluded[i] != NOTHING; i++) {
	if(excluded[i] == x) return 1; /* always exclude specifics */
        if(excluded[i] == db[x].owner) return 1;
    }

    return (0);
}

/* returns 1 if it is not excluded */
static int not_excluded(dbref x)
{
    int i;

    if(x == NOTHING) return 1; /* Don't exclude nothing */

    /* check that it isn't excluded */
    for(i = 0; excluded[i] != NOTHING; i++) {
	if(excluded[i] == x) return 0; /* always exclude specifics */
        if(excluded[i] == db[x].owner) return 0;
    }

    /* if it is an exit, check that its destination is ok */
    if(Typeof(x) == TYPE_EXIT && db[x].location >= 0) {
	return isok(db[x].location);
    } else {
	return 1;
    }
}

/* returns 1 if it should be included in translation vector */
static int isok(dbref x)
{
    int i;

    if (x == DEFAULT_OWNER || x == DEFAULT_LOCATION) return 1;
    if (x == NOTHING) return 1;

    if (x <= safe_below || x >= safe_above)
	return not_excluded(x);

    if (keep_players && Typeof(x) == TYPE_PLAYER)
	return not_excluded(x);

    if (norecycle && x != recycler && (db[x].owner == recycler))
	return 0;

    if (reachable && Typeof(x) == TYPE_ROOM && !REACHABLE(x)) { 
# ifdef DEBUG
	if (inbuild)
	    fprintf (stderr, "Excluding %s(%dR), not reachable\n", 
		     db[x].name, x);
# endif
	return 0;
    }
	

    /* Check inclusion list for object and its owner */
    for(i = 0; included[i] != NOTHING; i++) {
	if(included[i] == x) return 1; /* always get specific ones */

	if(included[i] == db[x].owner)
	    return not_excluded(x);
    }
    
# ifdef TIMESTAMPS
    /* Check for usage within usecutoff time */
    if (usecutoff > 0)
    { switch (Typeof(x))
      { case TYPE_ROOM:
	  /* Old rooms disappear */
	  if (db[x].lastused < usecutoff)
	  { return (0); }
	  break;

        case TYPE_EXIT:
	  /* Exits disappear if their rooms disappear.
	   * Old exits disappear if they go to the same room
	   */
	  if (db[x].lastused < usecutoff && db[x].location == x)
	  { return (0); }
	  break;

	case TYPE_THING:
	  /* Old things disappear from old rooms or dark rooms */
	  if (db[x].lastused < usecutoff &&
	      (db[db[x].owner].lastused < usecutoff ||
	       db[getloc(x)].lastused < usecutoff ||
	       Dark(getloc(x))))
	  { return (0); }
	  break;

	case TYPE_PLAYER:
	  /* At some future point, recycle old players too */
	  /* Note: when we do that, their stuff has to go, too */
	  break;
      }
    }
# endif
	
    /* not in the list, can only get it if include_all is on */
    /* or its owned by DEFAULT_OWNER */
    return (include_all && not_excluded(x));
}

static void build_trans(void)
{
    dbref i;
    dbref val;

    if((trans = (dbref *) malloc(sizeof(dbref) * db_top)) == 0) {
	abort();
    }
    
    inbuild++;

    val = 0;
    for(i = 0; i < db_top; i++) {
	if(isok(i)) {
	    trans[i] = val++;
	} else {
	    trans[i] = NOTHING;
	}
    }
    
    inbuild--;
}

static dbref translate(dbref x)
{
    if(x == NOTHING || x == HOME) {
	return(x);
    } else {
	return(trans[x]);
    }
}

/* TRUE_BOOLEXP means throw this argument out */
/* even on OR; it's effectively a null boolexp */
/* NOTE: this doesn't free anything, it just munges it up */
static struct boolexp *translate_boolexp(struct boolexp *exp)
{
    struct boolexp *s1;
    struct boolexp *s2;

    if(exp == TRUE_BOOLEXP) {
	return TRUE_BOOLEXP;
    } else {
	switch(exp->type) {
	  case BOOLEXP_NOT:
	    s1 = translate_boolexp(exp->sub1);
	    if(s1 == TRUE_BOOLEXP) {
		return TRUE_BOOLEXP;
	    } else {
		exp->sub1 = s1;
		return exp;
	    }
	    /* break; */
	  case BOOLEXP_AND:
	  case BOOLEXP_OR:
	    s1 = translate_boolexp(exp->sub1);
	    s2 = translate_boolexp(exp->sub2);
	    if(s1 == TRUE_BOOLEXP && s2 == TRUE_BOOLEXP) {
		/* nothing left */
		return TRUE_BOOLEXP;
	    } else if(s1 == TRUE_BOOLEXP && s2 != TRUE_BOOLEXP) {
		/* s2 is all that is left */
		return s2;
	    } else if(s1 != TRUE_BOOLEXP && s2 == TRUE_BOOLEXP) {
		/* s1 is all that is left */
		return s1;
	    } else {
		exp->sub1 = s1;
		exp->sub2 = s2;
		return exp;
	    }
	    /* break; */
	  case BOOLEXP_CONST:
	    exp->thing = translate(exp->thing);
	    if(exp->thing == NOTHING) {
		return TRUE_BOOLEXP;
	    } else {
		return exp;
	    }
	    /* break; */
	  default:
	    abort();		/* bad boolexp type, we lose */
	    return TRUE_BOOLEXP;
	}
    }
}

static int ok(dbref x)
{
    if(x == NOTHING || x == HOME) {
	return 1;
    } else {
	return trans[x] != NOTHING;
    }
}

static void check_bad_exits(dbref x)
{
    dbref e;

    if(Typeof(x) == TYPE_ROOM && !isok(x)) {
	/* mark all exits as excluded */
	DOLIST(e, db[x].exits) {
	    trans[e] = NOTHING;
	}
    }
}

static void check_owner(dbref x)
{
    if(ok(x) && !ok(db[x].owner)) {
	db[x].owner = DEFAULT_OWNER;	/* Hmm...not what we want. Fuzzy */
    }
}

static void check_location(dbref x)
{
    dbref loc;
    dbref newloc;

    if(ok(x) && (Typeof(x) == TYPE_THING || Typeof(x) == TYPE_PLAYER)
       && !ok(loc = db[x].location)) {
	/* move it to home or DEFAULT_LOCATION */
	if(ok(db[x].exits)) {
	    newloc = db[x].exits; /* home */
	} else {
	    newloc = DEFAULT_LOCATION;
	}
	db[loc].contents = remove_first(db[loc].contents, x);
	PUSH(x, db[newloc].contents);
	db[x].location = newloc;
    }
}

static void check_next(dbref x)
{
    dbref next;

    if(ok(x)) {
	while(!ok(next = db[x].next)) db[x].next = db[next].next;
    }
}

static void check_contents(dbref x)
{
    dbref c;

    if(ok(x)) {
	while(!ok(c = db[x].contents)) db[x].contents = db[c].next;
    }
}

/* also updates home */
/* MUST BE CALLED AFTER check_owner! */
static void check_exits(dbref x)
{
    dbref e;

    if(ok(x) && !ok(e = db[x].exits)) {
	switch(Typeof(x)) {
	  case TYPE_ROOM:
	    while(!ok(e = db[x].exits)) db[x].exits = db[e].next;
	    break;
	  case TYPE_PLAYER:
	  case TYPE_THING:
	    if(ok(db[db[x].owner].exits)) {
		/* set it to owner's home */
		db[x].exits = db[db[x].owner].exits; /* home */
	    } else {
		/* set it to DEFAULT_LOCATION */
		db[x].exits = DEFAULT_LOCATION; /* home */
	    }
	    break;
	}
    }
}

static void do_write(void)
{
    dbref i;
    dbref kludge;

    /* this is braindamaged */
    /* we have to rebuild the translation map */
    /* because part of it may have gotten nuked in check_bad_exits */
    for(i = 0, kludge = 0; i < db_top; i++) {
	if(trans[i] != NOTHING) trans[i] = kludge++;
    }

    for(i = 0; i < db_top; i++) {
	if(ok(i)) {
	    /* translate all object pointers */
	    db[i].location = translate(db[i].location);
	    db[i].contents = translate(db[i].contents);
	    db[i].exits = translate(db[i].exits);
	    db[i].next = translate(db[i].next);
	    db[i].key = translate_boolexp(db[i].key);
	    db[i].owner = translate(db[i].owner);

	    /* write it out */
	    printf("#%d\n", translate(i));
	    db_write_object(stdout, i);
	}
    }
    puts("***END OF DUMP***");
    
    fprintf (stderr, "Wrote %d objects.\n", kludge);
}

int reach_lvl = 0;

make_reachable (dbref x)
{   dbref e, r;
    int i;
    
    if (Typeof(x) != TYPE_ROOM || is_excluded(x)) return;

    reach_lvl++;

    SET_REACHABLE(x);

#ifdef DEBUG
    for (i=0; i<reach_lvl; i++ ) fputc (' ', stderr);
    fprintf (stderr, "Set %s(%dR) reachable.\n", db[x].name, x);
#endif

    DOLIST(e, db[x].exits) {
	r = db[e].location;

	if (r < 0) continue;
	if (is_excluded(r)) continue;
        if (is_excluded(e)) continue;
	if (!REACHABLE(r)) make_reachable(r);
    }

    reach_lvl--;
}

int main(int argc, char **argv)
{
    dbref i;
    int top_in;
    int top_ex;
    char *arg0;
    
    now = time (0);

    top_in = 0;
    top_ex = 0;

    /* Load database */
    if(db_read(stdin) < 0) {
	fputs("Database load failed!\n", stderr);
	exit(1);
    } 

    fputs("Done loading database...\n", stderr);


    /* now parse args */
    arg0 = *argv;
    for (argv++, argc--; argc > 0; argv++, argc--) {
	if (isdigit (**argv) || **argv == '-' && isdigit ((*argv)[1])) {
	    i = atol(*argv);
	} else if (**argv == '+' && isdigit ((*argv)[1])) {
	    i = atol(*argv+1);
	} else if (**argv == 'b' && isdigit ((*argv)[1])) {
	    safe_below = atol(*argv+1);
	    fprintf (stderr, "Including all objects %d and below\n",
	    	     safe_below);
	} else if (**argv == 'a' && isdigit ((*argv)[1])) {
	    safe_above = atol(*argv+1);
	    fprintf (stderr, "Including all objects %d and above\n",
	    	     safe_above);
#ifdef TIMESTAMPS
	} else if (**argv == 'u' && isdigit ((*argv)[1])) {
	    usecutoff = atosec(*argv+1);
	    fprintf (stderr,
		     "Excluding rooms not used within the last %d seconds\n",
	    	     usecutoff);
	    usecutoff = now - usecutoff;
#endif
	} else if (!strcmp(*argv, "all")) {
	    include_all = 1;
	} else if (!strcmp(*argv, "reachable")) {
	    reachable = 1;
	} else if (!strcmp(*argv, "players")) {
	    keep_players = 1;
	} else if (!strcmp(*argv, "norecycle")) {
	    norecycle = 1;
	} else if (**argv == '-' &&
	           (i = lookup_player (*argv+1)) != 0) {
	    fprintf (stderr, "Excluding player %s(%d)\n",
		     db[i].name, i);
	    i = -i;
	} else if (**argv != '-' &&
	    	       (i = lookup_player (*argv)) != NOTHING) {
	    fprintf (stderr, "Including player %s(%d)\n",
		     db[i].name, i);
	} else {
		fprintf(stderr, "%s: bogus argument %s\n", arg0, *argv);
		continue;
	}
	
	if(i < 0) {
	    excluded[top_ex++] = -i;
	} else {
	    included[top_in++] = i;
	}
    }

    /* Terminate */
    included[top_in++] = NOTHING;
    excluded[top_ex++] = NOTHING;

    /* Check for reachability from DEFAULT_LOCATION */
    if (reachable)
    { make_reachable (DEFAULT_LOCATION); 
      fputs ("Done marking reachability...\n", stderr); 
    }
    
    /* Find recycler */
    if (norecycle && ((i = lookup_player (RECYCLER)) != NOTHING))
    { recycler = i;
      fprintf (stderr, "Excluding all objects owned by %s\n",
		db[recycler].name);
    }
    else
    { norecycle = 0; }

    /* Build translation table */
    build_trans();
    fputs("Done building translation table...\n", stderr);

    /* Scan everything */
    for(i = 0; i < db_top; i++) check_bad_exits(i);
    fputs("Done checking bad exits...\n", stderr);

    for(i = 0; i < db_top; i++) check_owner(i);
    fputs("Done checking owners...\n", stderr);

    for(i = 0; i < db_top; i++) check_location(i);
    fputs("Done checking locations...\n", stderr);

    for(i = 0; i < db_top; i++) check_next(i);
    fputs("Done checking next pointers...\n", stderr);

    for(i = 0; i < db_top; i++) check_contents(i);
    fputs("Done checking contents...\n", stderr);

    for(i = 0; i < db_top; i++) check_exits(i);
    fputs("Done checking homes and exits...\n", stderr);

    do_write();

    exit(0);
    return(0);
}

# define MINUTES (60)
# define HOURS   (3600)
# define DAYS    (24 * HOURS)
# define WEEKS   (7 * DAYS)
# define MONTHS  (30 * DAYS)
# define YEARS   (365 * DAYS)

long atosec (const char *str)
{
    long num;
    const char *s;
    
    if (!str || !*str) return (0L);
    
    num = atol (str);
    
    for (s=str; *s && isspace (*s) || isdigit (*s); s++) ;
    
    switch (*s) {
      case '\0':
      case 's':
      case 'S':		return (num); break;

      case 'm':
      case 'M':		if (s[1] == 'i' || s[1] == 'I') {
			    return (num * MINUTES); break;
      			} else {
			    return (num * MONTHS); break;
			}

      case 'h':
      case 'H':		return (num * HOURS); break;

      case 'd':
      case 'D':		return (num * DAYS); break;

      case 'w':
      case 'W':		return (num * WEEKS); break;

      case 'y':
      case 'Y':		return (num * YEARS); break;

      default:		return (num);
    }
}

