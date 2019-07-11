#include "copyright.h"

/* commands which set parameters */
#include <stdio.h>
#include <ctype.h>

#include "db.h"
#include "config.h"
#include "match.h"
#include "interface.h"
#include "externs.h"

#ifdef COMPRESS
#define alloc_compressed(x) alloc_string(compress(x))
#else /* COMPRESS */
#define alloc_compressed(x) alloc_string(x)
#endif 

#define FREE_STRING(X) X = ((X) ? (free ((void *) (X)), NULL) : NULL)

static dbref match_controlled(dbref player, const char *name)
{
    dbref match;

    init_match(player, name, NOTYPE);
    match_everything();

    match = noisy_match_result();
    if(match != NOTHING && !controls(player, match)) {
	notify(player, "Permission denied.");
	return NOTHING;
    } else {
	return match;
    }
}

void do_name(dbref player, const char *name, char *newname)
{
    dbref thing;
    char *password;

    if((thing = match_controlled(player, name)) != NOTHING) {
	/* check for bad name */
	if(*newname == '\0') {
	    notify(player, "Give it what new name?");
	    return;
	}

	/* check for renaming a player */
	if(Typeof(thing) == TYPE_PLAYER) {
	    /* split off password */
	    for(password = newname;
		*password && !isspace(*password);
		password++);
	    /* eat whitespace */
	    if(*password) {
		*password++ = '\0'; /* terminate name */
		while(*password && isspace(*password)) password++;
	    }

	    /* check for null password */
	    if(!*password) {
		notify(player,
		       "You must specify a password to change a player name.");
		notify(player, "E.g.: name player = newname password");
		return;
	    } else if(strcmp (db[thing].name, "guest") == 0) {
		notify(player,
		       "You are only a guest here...no name changing.");
		return;
	    } else if(!db[thing].password) {
		/* If original has no password, set one */
		db[thing].password = alloc_string(password);
	    } else if(strcmp(password, db[thing].password)) {
		notify(player, "Incorrect password.");
		return;
	    } else if(string_compare(newname, db[thing].name) &&
		      !ok_player_name(newname)) {
		notify(player, "You can't give a player that name.");
		return;
	    }
	    /* everything ok, notify */
	    writelog("NAME CHANGE: %s(#%d) to %s\n",
		    db[thing].name, thing, newname);
#ifdef PLAYER_LIST
	    delete_player(thing);
	    free((void *) db[thing].name);
	    db[thing].name = alloc_string(newname);
	    add_player(thing);
	    notify(player, "Name set.");
	    return;
#endif
	} else {
	    if(!ok_name(newname)) {
		notify(player, "That is not a reasonable name.");
		return;
	    }
	}

	/* everything ok, change the name */
	if(db[thing].name) {
	    free((void *) db[thing].name);
	}
	db[thing].name = alloc_string(newname);
	notify(player, "Name set.");
    }
}

void do_describe(dbref player, const char *name, const char *description)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	if(db[thing].description) {
	    free((void *) db[thing].description);
	}
	db[thing].description = alloc_compressed(description);
	notify(player, "Description set.");
    }
}

void do_fail(dbref player, const char *name, const char *message)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	if(db[thing].fail_message) {
	    free((void *) db[thing].fail_message);
	}
	db[thing].fail_message = alloc_compressed(message);
	notify(player, "Message set.");
    }
}

void do_success(dbref player, const char *name, const char *message)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	if(db[thing].succ_message) {
	    free((void *) db[thing].succ_message);
	}
	db[thing].succ_message = alloc_compressed(message);
	notify(player, "Message set.");
    }
}

void do_osuccess(dbref player, const char *name, const char *message)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	if(db[thing].osuccess) {
	    free((void *) db[thing].osuccess);
	}
	db[thing].osuccess = alloc_compressed(message);
	notify(player, "Message set.");
    }
}

void do_ofail(dbref player, const char *name, const char *message)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	if(db[thing].ofail) {
	    free((void *) db[thing].ofail);
	}
	db[thing].ofail = alloc_compressed(message);
	notify(player, "Message set.");
    }
}

void do_lock(dbref player, const char *name, const char *keyname)
{
    dbref thing;
    struct boolexp *key;

    init_match(player, name, NOTYPE);
    match_everything();

    switch(thing = match_result()) {
      case NOTHING:
	notify(player, "I don't see what you want to lock!");
	return;
      case AMBIGUOUS:
	notify(player, "I don't know which one you want to lock!");
	return;
      default:
	if(!controls(player, thing)) {
	    notify(player, "You can't lock that!");
	    return;
	}
	break;
    }

    key = parse_boolexp(player, keyname);
    if(key == TRUE_BOOLEXP) {
	notify(player, "I don't understand that key.");
    } else {
	/* everything ok, do it */
	free_boolexp(db[thing].key);
	db[thing].key = key;
	notify(player, "Locked.");
    }
}

void do_unlock(dbref player, const char *name)
{
    dbref thing;

    if((thing = match_controlled(player, name)) != NOTHING) {
	free_boolexp(db[thing].key);
	db[thing].key = TRUE_BOOLEXP;
	notify(player, "Unlocked.");
    }
}

void do_unlink(dbref player, const char *name)
{
    dbref exit;

    init_match(player, name, TYPE_EXIT);
    match_exit();
    match_here();
    if(Wizard(player)) {
	match_absolute();
    }

    switch(exit = match_result()) {
      case NOTHING:
	notify(player, "Unlink what?");
	break;
      case AMBIGUOUS:
	notify(player, "I don't know which one you mean!");
	break;
      default:
	if(!controls(player, exit)) {
	    notify(player, "Permission denied.");
	} else {
	    switch(Typeof(exit)) {
	      case TYPE_EXIT:
		db[exit].location = NOTHING;
		notify(player, "Unlinked.");
		break;
	      case TYPE_ROOM:
		db[exit].location = NOTHING;
		notify(player, "Dropto removed.");
		break;
	      default:
		notify(player, "You can't unlink that!");
		break;
	    }
	}
    }
}

void do_chown(dbref player, const char *name, const char *newobj)
{
    dbref thing;
    dbref owner;
    
    init_match(player, name, NOTYPE);
    match_everything();

    if((thing = noisy_match_result()) == NOTHING) {
	return;
    } else if((owner = lookup_player(newobj)) == NOTHING &&
	      string_compare(newobj,"me")) {
	notify(player, "I couldn't find that player.");
    } else if(Typeof(thing) == TYPE_PLAYER) {
	notify(player, "Players always own themselves.");
    } else if (!Wizard(player) &&
	       ((string_compare(newobj,"me") &&
		 owner != player) || !(db[thing].flags & UNWANTED))) {
	notify(player, "Permission denied.");
    } else if (!Wizard(player) && !could_doit(player,thing)) {
	notify(player, "That item is locked.");
    } else {
	if (!string_compare(newobj,"me"))
	    owner = player;
	db[thing].owner = owner;
	notify(player, "Owner changed.");
    }
}

void do_set(dbref player, const char *name, const char *flag)
{
    dbref thing;
    const char *p;
    object_flag_type f;

    /* find thing */
    if((thing = match_controlled(player, name)) == NOTHING) return;

    /* move p past NOT_TOKEN if present */
    for(p = flag; *p && (*p == NOT_TOKEN || isspace(*p)); p++);

    /* identify flag */
    if(*p == '\0') {
	notify(player, "You must specify a flag to set.");
	return;
    } else if(string_prefix("LINK_OK", p)) {
	f = LINK_OK;
    } else if(string_prefix("DARK", p)) {
	f = DARK;
    } else if(string_prefix("STICKY", p)) {
	f = STICKY;
    } else if(string_prefix("WIZARD", p)) {
	f = WIZARD;
    } else if(string_prefix("TEMPLE", p)) {
	f = TEMPLE;
    } else if(string_prefix("HAVEN", p)) {
	f = HAVEN;
    } else if(string_prefix("ABODE", p)) {
	f = ABODE;
    } else if(string_prefix("UNWANTED", p)) {
	f = UNWANTED;
#ifdef ROBOT_MODE
    } else if(string_prefix("ZOMBIE", p) ||
	      string_prefix("ZOM", p)) {
	if (Typeof(thing) == TYPE_PLAYER) {
	    if (Wizard(player)) {
		writelog ("ZOMBIE: success %s(%d) %s %s(%d)\n",
			  db[player].name, player,
			  *flag == NOT_TOKEN ? "reset" : "set",
			  db[thing].name, thing);
	    } else {
		writelog ("ZOMBIE: failed %s(%d) %s %s(%d)\n",
			  db[player].name, player,
			  *flag == NOT_TOKEN ? "reset" : "set",
			  db[thing].name, thing);
		notify(player,
		       "Only a Wizard can make a player into a zombie.");
		return;
	    }
	}
	f = ROBOT;
#endif
    } else if(string_prefix("TABULAR_WHO", p)) {
	f = TABULAR_WHO;
    } else if(string_prefix("REVERSED_WHO", p)) {
	f = REVERSED_WHO;
#ifdef GENDER
    } else if(string_prefix("MALE", p) || string_prefix("FEMALE", p) ||
              string_prefix("NEUTER", p)   || string_prefix("UNASSIGNED", p)) {
        if (Typeof(thing) != TYPE_PLAYER) {
	    notify(player, "Sorry, only players have gender.");
	    return;
        }
        if (thing != player && !Wizard(player)) {
	    notify(player, "You can only give yourself a sex-change.");
	    return;
        }
        db[thing].flags &= ~GENDER_MASK;
	if(string_prefix("UNASSIGNED", p) || *flag == NOT_TOKEN) {
	    db[thing].flags |= (GENDER_UNASSIGNED << GENDER_SHIFT);
	    notify(player, "Gender set to unassigned.");
	} else if (string_prefix("MALE", p)) {
	    db[thing].flags |= (GENDER_MALE << GENDER_SHIFT);
	    notify(player, "Gender set to male.");
        } else if(string_prefix("FEMALE", p)) {
	    db[thing].flags |= (GENDER_FEMALE << GENDER_SHIFT);
	    notify(player, "Gender set to female.");
        } else if(string_prefix("NEUTER", p)) {
	    db[thing].flags |= (GENDER_NEUTER << GENDER_SHIFT);
	    notify(player, "Gender set to neuter.");
	}
        return;
#endif
#ifdef RESTRICTED_BUILDING
    } else if(string_prefix("BUILDER", p))
	f = BUILDER;
#endif
    else {
	notify(player, "I don't recognize that flag.");
	return;
    }

    /* check for restricted flag */
    if(!Wizard(player)
       && (f == TEMPLE
#ifdef RESTRICTED_BUILDING
	   || f == BUILDER
#endif
	   || f == DARK &&
	   (Typeof(thing) != TYPE_ROOM && Typeof(thing) != TYPE_THING))) {
	notify(player, "Permission denied.");
	return;
    }

#ifdef GOD_PRIV
    if (!God(player) && f == DARK && Typeof(thing) == TYPE_PLAYER) {
	notify(player, "Permission denied.");
	return;
    }
#endif

    if (!Wizard(player) && f == DARK && Typeof(thing) == TYPE_THING &&
    	db[thing].location != player) {
	notify(player, "You must be holding an object to set it DARK.");
	return;
    }

#ifdef GOD_PRIV
    if(!God(player) && (f == WIZARD)) {
#else
    if (!Wizard(player) && (f == WIZARD)) {
#endif
	writelog ("WIZARD: failed %s(%d) %s %s(%d)\n",
		  db[player].name, player,
		  *flag == NOT_TOKEN ? "reset" : "set",
		  db[thing].name, thing);

	notify(player, "Permission denied.");
	return;
    }

    if (f == WIZARD) {
	writelog ("WIZARD: success %s(%d) %s %s(%d)\n",
		  db[player].name, player,
		  *flag == NOT_TOKEN ? "reset" : "set",
		  db[thing].name, thing);
    }

#ifdef GOD_PRIV
    /* check for unGODding */
    if (f == WIZARD && God(thing)) {
	notify(player, "Gods can not me made mortal.");
	return;
    }
#else
    if (f == WIZARD && thing == player) {
	notify(player, "You cannot make yourself mortal!");
	return;
    }
#endif

    if ( f==HAVEN && !(Typeof(thing) == TYPE_ROOM ||
		       Typeof(thing) == TYPE_PLAYER)) {
	notify(player, "Only rooms or players can be HAVEN.");
	return;
    }

    if (f==UNWANTED && Typeof(thing)==TYPE_PLAYER) {
	notify(player, "You should always want yourself.");
	return;
    }

    if ((f==TABULAR_WHO || f==REVERSED_WHO) &&
	Typeof(thing) != TYPE_PLAYER) {
	notify(player, "Player preference flags can only be set on players.");
	return;
    }
    
    /* else everything is ok, do the set */
    if(*flag == NOT_TOKEN) {
	/* reset the flag */
	db[thing].flags &= ~f;
	notify(player, "Flag reset.");
    } else {
	/* set the flag */
	db[thing].flags |= f;
	notify(player, "Flag set.");
    }
}

#ifdef RECYCLE
/****************************************************************
 * do_recycle: Allow any player to give an item away...the item
 * is @chowned to the "Recycler" player, and if it's a thing,
 * it is @linked to the home of the "Recycler" as well.
 ****************************************************************/

void do_recycle(dbref player, const char *name)
{
    dbref thing, recip, i;
    char buf[BUFFER_LEN];

    if (!name || !*name) {
	notify(player, "You must specify an object to @recycle.");
	return;
    }

    if((thing = match_controlled(player, name)) == NOTHING) return;
    
    if(Typeof(thing) == TYPE_PLAYER) {
	notify(player, "You can't @recycle players.");
	return;
    }

    if((recip = lookup_player(RECYCLER)) == NOTHING) {
	notify(player, "There is no current Recycler.");
	return;
    }
    
    /* Okay - do it */
    db[thing].owner = recip;

    /* If its a thing, link it to the Recyclers home */
    if (Typeof(thing) == TYPE_THING) {
	db[thing].exits = db[recip].exits;
	moveto (thing, db[thing].exits);
    }

    /* Clear its strings */
    FREE_STRING (db[thing].name);
    FREE_STRING (db[thing].description);
    FREE_STRING (db[thing].fail_message);
    FREE_STRING (db[thing].ofail);
    FREE_STRING (db[thing].succ_message);
    FREE_STRING (db[thing].osuccess);
    sprintf (buf, "junk-%d", thing);
    db[thing].name = alloc_string(buf);

    /* unlock it */
    free_boolexp(db[thing].key);
    db[thing].key = TRUE_BOOLEXP;

    /* Make it worthless */
    db[thing].pennies = 1;

    /* Make it up for grabs */
    db[thing].flags |= UNWANTED;

    /* Tell player we did it */    
    notify(player, "Object recycled.");
}

/****************************************************************
 * do_count: count things...
 *	players:	Number of objects owned, carried
 *	rooms:		number of exits from & to, number contents
 ****************************************************************/

void do_count(dbref player, const char *name)
{
    dbref thing, i, exit;
    char buf[BUFFER_LEN];
    int owned=0, contents=0, from=0, to=0, objects=0, rooms=0, exits=0;
    int rowned=0, rplayers=0, robjects=0, rrooms=0, rexits=0;

    if (!name || !*name) {
	notify(player, "You must specify an object to @count.");
	return;
    }

    if((thing = match_controlled(player, name)) == NOTHING) return;

    if(Typeof(thing) != TYPE_PLAYER && Typeof(thing) != TYPE_ROOM) {
	notify(player, "You can only @count people or rooms.\n");
	return;
    }

    if(!payfor(player, FIND_COST)) {
	notify(player, "You don't have enough pennies.");
	return;
    }
    
    switch (Typeof(thing)) {
      case TYPE_PLAYER:
	for (i=0; i<db_top; i++) {
	    if (db[i].location == thing) contents++;
	    if (db[i].owner == thing) {
		switch (Typeof(i)) {
		  case TYPE_THING:	objects++; break;
		  case TYPE_EXIT:	exits++; break;
		  case TYPE_ROOM:	rooms++; break;
		}
		owned++;
	    }
	}
	sprintf(buf,
	     "%s owns %d objects (%d things, %d rooms, %d exits), %d carried.",
		unparse_object(player, thing),
	        owned, objects, rooms, exits, contents);
	notify(player, buf);
	break;

      case TYPE_ROOM:
	for (i=0; i<db_top; i++) {
	    if (db[i].location == thing) {
		if (Typeof(i) == TYPE_EXIT) {
		    to++;
		} else {
		    contents++;
		}
	    }
	}
	DOLIST(exit, db[thing].exits) {
	    from++;
	}
	sprintf(buf, "%s has %d entrances, %d exits, %d objects.",
		unparse_object(player, thing), to, from, contents);
	notify(player, buf);
	
	if (!Wizard (player)) break;

	for (i=0; i<db_top; i++) {
	    if (db[db[i].owner].location == thing) {
	    	switch (Typeof(i)) {
		  case TYPE_EXIT: rexits++; break;
  		  case TYPE_ROOM: rrooms++; break;
  		  case TYPE_THING: robjects++; break;
		  case TYPE_PLAYER: rplayers++; break;
		}
		rowned++;
	    }
	}

	if (rplayers == 0) {
	    sprintf (buf, "There are no players in %s.\n",
	    unparse_object (player, thing));
	} else {
	    sprintf(buf,
	    	"The %d player%s in %s own%s %d %s: %d %s, %d %s, %d %s\n.",
		rplayers, (rplayers == 1) ? "" : "s",
		unparse_object(player, thing), (rplayers == 1) ? "s" : "",
		rowned, "objects", robjects, "things", rrooms, "rooms",
		rexits, "exits");
	}

	notify(player, buf);

	break;
    }

}
#endif

