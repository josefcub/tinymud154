#include "copyright.h"

/* commands which look at things */

#include <sys/time.h>
#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"

static void look_contents(dbref player, dbref loc, const char *contents_name)
{
    dbref thing;
    dbref can_see_loc;

    /* check to see if he can see the location */
    can_see_loc = (!Dark(loc) || controls(player, loc));

    /* check to see if there is anything there */
    DOLIST(thing, db[loc].contents) {
	if(can_see(player, thing, can_see_loc)) {
	    /* something exists!  show him everything */
	    notify(player, contents_name);
	    DOLIST(thing, db[loc].contents) {
		if(can_see(player, thing, can_see_loc)) {
		    notify(player, unparse_object(player, thing));
		}
	    }
	    break;		/* we're done */
	}
    }
}

static void look_simple(dbref player, dbref thing)
{
    if(db[thing].description) {
	notify(player, db[thing].description);
    } else {
	notify(player, "You see nothing special.");
    }
}

void look_room(dbref player, dbref loc)
{

    /* tell him the name, and the number if he can link to it */
    notify(player, unparse_object(player, loc));

    /* tell him the description */
    if(db[loc].description) notify(player, db[loc].description);

    /* tell him the appropriate messages if he has the key */
    can_doit(player, loc, 0);

    /* tell him the contents */
    look_contents(player, loc, "Contents:");

}

void do_look_around(dbref player)
{
    dbref loc;

    if((loc = getloc(player)) == NOTHING) return;
    look_room(player, loc);
}

void do_look_at(dbref player, const char *name)
{
    dbref thing;

    if(*name == '\0') {
	if((thing = getloc(player)) != NOTHING) {
	    look_room(player, thing);
	}
    } else {
	/* look at a thing here */
	init_match(player, name, NOTYPE);
	match_exit();
	match_neighbor();
	match_possession();
	if(Wizard(player)) {
	    match_absolute();
	    match_player();
	}
	match_here();
	match_me();

	if((thing = noisy_match_result()) != NOTHING) {
	    switch(Typeof(thing)) {
	      case TYPE_ROOM:
		look_room(player, thing);
		break;
	      case TYPE_PLAYER:
		look_simple(player, thing);
		look_contents(player, thing, "Carrying:");
		break;
#ifdef TIMESTAMPS
	      case TYPE_THING:
	      case TYPE_EXIT:
		db[thing].usecnt++;
		db[thing].lastused = time (0);
		/* Note: fall through into default case */
#endif
	      default:
		look_simple(player, thing);
		break;
	    }
	}
    }
}

#if 0
static const char *flag_description(dbref thing)
{
    static char buf[BUFFER_LEN];

    strcpy(buf, "Type: ");
    switch(Typeof(thing)) {
      case TYPE_ROOM:
	strcat(buf, "Room");
	break;
      case TYPE_EXIT:
	strcat(buf, "Exit");
	break;
      case TYPE_THING:
	strcat(buf, "Thing");
	break;
      case TYPE_PLAYER:
	strcat(buf, "Player");
	break;
      default:
	strcat(buf, "***UNKNOWN TYPE***");
	break;
    }

    if(db[thing].flags & ~TYPE_MASK) {
	/* print flags */
	strcat(buf, " Flags:");
	if(db[thing].flags & WIZARD) strcat(buf, " WIZARD");
	if(db[thing].flags & STICKY) strcat(buf, " STICKY");
	if(db[thing].flags & DARK) strcat(buf, " DARK");
	if(db[thing].flags & LINK_OK) strcat(buf, " LINK_OK");
	if(db[thing].flags & TEMPLE) strcat(buf, " TEMPLE");
#ifdef RESTRICTED_BUILDING
	if(db[thing].flags & BUILDER) strcat(buf, " BUILDER");
#endif 
    }

    return buf;
}
#endif

void do_examine(dbref player, const char *name)
{
    dbref thing;
    dbref content;
    dbref exit;
    char buf[BUFFER_LEN];

    if(*name == '\0') {
	if((thing = getloc(player)) == NOTHING) return;
    } else {
	/* look it up */
	init_match(player, name, NOTYPE);
	match_exit();
	match_neighbor();
	match_possession();
	match_absolute();
	/* only Wizards can examine other players */
	if(Wizard(player)) match_player();
	match_here();
	match_me();

	/* get result */
	if((thing = noisy_match_result()) == NOTHING) return;
    }

    if(!can_link(player, thing)) {
	sprintf(buf, "Owner: %s", db[db[thing].owner].name);
	notify(player, buf);
	if(db[thing].description) notify(player, db[thing].description);
	return;
    }

    notify(player, unparse_object(player, thing));
    sprintf(buf, "Owner: %s  Key: %s Pennies: %d",
	    db[db[thing].owner].name,
	    unparse_boolexp(player, db[thing].key),
	    db[thing].pennies);
    notify(player, buf);
    if(db[thing].description) notify(player, db[thing].description);
    if(db[thing].fail_message) {
	sprintf(buf, "Fail: %s", db[thing].fail_message);
	notify(player, buf);
    }
    if(db[thing].succ_message) {
	sprintf(buf, "Success: %s", db[thing].succ_message);
	notify(player, buf);
    }
    if(db[thing].ofail) {
	sprintf(buf, "Ofail: %s", db[thing].ofail);
	notify(player, buf);
    }
    if(db[thing].osuccess) {
	sprintf(buf, "Osuccess: %s", db[thing].osuccess);
	notify(player, buf);
    }

    /* show him the contents */
    if(db[thing].contents != NOTHING) {
	notify(player, "Contents:");
	DOLIST(content, db[thing].contents) {
	    notify(player, unparse_object(player, content));
	}
    }

    switch(Typeof(thing)) {
      case TYPE_ROOM:
	/* tell him about exits */
	if(db[thing].exits != NOTHING) {
	    notify(player, "Exits:");
	    DOLIST(exit, db[thing].exits) {
		notify(player, unparse_object(player, exit));
	    }
	} else {
	    notify(player, "No exits.");
	}

	/* print dropto if present */
	if(db[thing].location != NOTHING) {
	    sprintf(buf, "Dropped objects go to: %s",
		    unparse_object(player, db[thing].location));
	    notify(player, buf);
	}
	break;
      case TYPE_THING:
      case TYPE_PLAYER:
	/* print home */
	sprintf(buf, "Home: %s",
		unparse_object(player, db[thing].exits)); /* home */
	notify(player, buf);
	/* print location if player can link to it */
	if(db[thing].location != NOTHING
	   && (controls(player, db[thing].location)
	       || can_link_to(player, Typeof(thing), db[thing].location))) {
	    sprintf(buf, "Location: %s",
		    unparse_object(player, db[thing].location));
	    notify(player, buf);
	}
	break;
      case TYPE_EXIT:
	/* print destination */
	switch(db[thing].location) {
	  case NOTHING:
	    break;
	  case HOME:
	    notify(player, "Destination: *HOME*");
	    break;
	  default:
	    sprintf(buf, "%s: %s",
		    (Typeof(db[thing].location) == TYPE_ROOM
		     ? "Destination" : "Carried by"),
		    unparse_object(player, db[thing].location));
	    notify(player, buf);
	    break;
	}
	break;
      default:
	/* do nothing */
	break;
    }

#ifdef TIMESTAMPS
    if(db[thing].created > 0) {
	sprintf(buf, "Created: %24.24s", ctime (&db[thing].created));
	notify(player, buf);
    }

    if(db[thing].usecnt > 0) {
        switch (Typeof (thing)) {
	  case TYPE_PLAYER:
	    sprintf(buf, "Last command: %24.24s, %d command%s total",
	    	    ctime (&db[thing].lastused), db[thing].usecnt,
		    (db[thing].usecnt == 1) ? "" : "s");
	    break;
	  case TYPE_EXIT:
	    sprintf(buf, "Last used: %24.24s, %d use%s total",
	    	    ctime (&db[thing].lastused), db[thing].usecnt,
		    (db[thing].usecnt == 1) ? "" : "s");
	    break;
	  case TYPE_ROOM:
	    sprintf(buf, "Last entered: %24.24s, %d use%s total",
	    	    ctime (&db[thing].lastused), db[thing].usecnt,
		    (db[thing].usecnt == 1) ? "" : "s");
	    break;
	  case TYPE_THING:
	    sprintf(buf, "Last used: %24.24s, %d use%s total",
	    	    ctime (&db[thing].lastused), db[thing].usecnt,
		    (db[thing].usecnt == 1) ? "" : "s");
	    break;
	}
	notify(player, buf);
    }

#endif
}

void do_score(dbref player) 
{
    char buf[BUFFER_LEN];

    sprintf(buf, "You have %d %s.",
	    db[player].pennies,
	    db[player].pennies == 1 ? "penny" : "pennies");
    notify(player, buf);
}

void do_inventory(dbref player)
{
    dbref thing;

    if((thing = db[player].contents) == NOTHING) {
	notify(player, "You aren't carrying anything.");
    } else {
	notify(player, "You are carrying:");
	DOLIST(thing, thing) {
	    notify(player, unparse_object(player, thing));
	}
    }

    do_score(player);
}

void do_find(dbref player, const char *name)
{
    dbref i;
    long msec_diff(struct timeval now, struct timeval start);
    long cnt=0;
    struct timeval start, now;

    if(!payfor(player, FIND_COST)) {
	notify(player, "You don't have enough pennies.");
    } else {
	gettimeofday(&start, (struct timezone *) 0);

	for(i = 0; i < db_top; i++) {
	    if(Typeof(i) != TYPE_EXIT
	       && controls(player, i)
	       && (!*name || string_match(db[i].name, name))) {
		notify(player, unparse_object(player, i));
		cnt++;
	    }
	}
	notify(player, "***End of List***");

	gettimeofday(&now, (struct timezone *) 0);
	writelog ("FIND %s(%d) \"%s\" %1.3lf seconds, %ld objects\n",
		  db[player].name, player, name,
		  (double) msec_diff (now, start) / 1000.0,
		  cnt);
    }
}

void do_owned(dbref player, const char *sowner)
{
    dbref owner,i;
    long msec_diff(struct timeval now, struct timeval start);
    long cnt=0;
    struct timeval start, now;
    
    if(!Wizard(player)) {
 	notify(player,"Only a Wizard can check the ownership list. Use @find.");
    } else if ((owner = lookup_player(sowner)) == NOTHING) {
 	notify(player,"I couldn't find that player.");
    } else {
	gettimeofday(&start, (struct timezone *) 0);
	
 	for(i = 0; i < db_top; i++)
 	    if(Typeof(i) != TYPE_EXIT && db[i].owner == owner)
	    {
 		notify(player, unparse_object(player, i));
		cnt++;
	    }
 	notify(player, "***End of List***");

	gettimeofday(&now, (struct timezone *) 0);
	writelog ("OWND %s(%d) \"%s\" %1.3lf seconds, %ld objects\n",
		  db[player].name, player, sowner,
		  (double) msec_diff (now, start) / 1000.0,
		  cnt);
    }
}
