#include "copyright.h"

/* Predicates for testing various conditions */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include "db.h"
#include "interface.h"
#include "config.h"
#include "externs.h"

void pronoun_substitute(char *result, dbref player, const char *str);

int can_link_to(dbref who, object_flag_type what, dbref where)
{
    return(where >= 0 &&
	   where < db_top &&
	   Typeof(where) == TYPE_ROOM &&
	   (controls(who, where) ||
	    (what == NOTYPE &&
	     (Flag(where,LINK_OK|ABODE))) ||
	    (what == TYPE_ROOM &&
	     (Flag(where,ABODE))) ||
	    (what == TYPE_EXIT &&
	     (Flag(where,LINK_OK))) ||
	    ((what == TYPE_PLAYER || what == TYPE_THING) &&
#ifdef ROBOT_MODE
	     (what != TYPE_PLAYER || !Robot(who) || !Robot(where)) &&
#endif
	     Flag(where,ABODE))));
    }

/*
 * Check whether a player can perform an action...robotic players are
 * now implicitly barred from performing actions on things with the
 * robot flag set.	5/18/90 - Fuzzy
 */

int could_doit(dbref player, dbref thing)
{
    if(Typeof(thing) != TYPE_ROOM && db[thing].location == NOTHING) return 0;

#ifdef ROBOT_MODE
    if(db[thing].owner != player &&
       Typeof(thing) != TYPE_PLAYER &&
       Robot(player) && Robot(thing)) return 0;

    if(Typeof(thing) == TYPE_EXIT && Robot(player) &&
       db[thing].location >= 0 &&
       Robot(db[thing].location) &&
       db[db[thing].location].owner != player) return 0;
#endif

    return(eval_boolexp (player, db[thing].key));
}

int can_doit(dbref player, dbref thing, const char *default_fail_msg)
{
    dbref loc;
    char buf[BUFFER_LEN];

    if((loc = getloc(player)) == NOTHING) return 0;

    if(!could_doit(player, thing)) {
	/* can't do it */
	if(db[thing].fail_message) {
	    notify(player, db[thing].fail_message);
	} else if(default_fail_msg) {
	    notify(player, default_fail_msg);
	}
	
	if(db[thing].ofail && !Dark(player)) {
#ifdef GENDER
	    pronoun_substitute(buf, player, db[thing].ofail);
#else	    
	    sprintf(buf, "%s %s", db[player].name, db[thing].ofail);
#endif
	    notify_except(db[loc].contents, player, buf);
	}

	return 0;
    } else {
	/* can do it */
	if(db[thing].succ_message) {
	    notify(player, db[thing].succ_message);
	}

	if(db[thing].osuccess && !Dark(player)) {
#ifdef GENDER
	    pronoun_substitute(buf, player, db[thing].osuccess);
#else
	    sprintf(buf, "%s %s", db[player].name, db[thing].osuccess);
#endif 
	    notify_except(db[loc].contents, player, buf);
	}

	return 1;
    }
}

int can_see(dbref player, dbref thing, int can_see_loc)
{
    if(player == thing || Typeof(thing) == TYPE_EXIT) { 
	return 0;
    } else if(can_see_loc) {
	return(!Dark(thing) || controls(player, thing));
    } else {
	/* can't see loc */
	return(controls(player, thing));
    }
}

int controls(dbref who, dbref what)
{
    /* Wizard controls everything */
    /* owners control their stuff */
    return(what >= 0
	   && what < db_top
	   && (Wizard(who)
	       || who == db[what].owner));
}

int can_link(dbref who, dbref what)
{
    return((Typeof(what) == TYPE_EXIT && db[what].location == NOTHING)
	   || controls(who, what));
}

int payfor(dbref who, int cost)
{
    if(Wizard(who)) {
	return 1;
    } else if(db[who].pennies >= cost) {
	db[who].pennies -= cost;
	return 1;
    } else {
	return 0;
    }
}

int word_start (const char *str, const char let)
{
    int chk;

    for (chk = 1; *str; str++) {
	if (chk && *str == let) return 1;
	chk = *str == ' ';
    }
    return 0;
}


int ok_name(const char *name)
{
    return (name
	    && *name
	    && *name != LOOKUP_TOKEN
	    && *name != NUMBER_TOKEN
	    && !index(name, ARG_DELIMITER)
	    && !index(name, AND_TOKEN)
	    && !index(name, OR_TOKEN)
	    && !word_start(name, NOT_TOKEN)
#ifdef NOFAKES
	    && string_compare(name, "A")
	    && string_compare(name, "An")
	    && string_compare(name, "The")
	    && string_compare(name, "You")
	    && string_compare(name, "Your")
	    && string_compare(name, "Going")
    	    && string_compare(name, "Huh?")
    	    && string_compare(name, "[")
#endif
	    && string_compare(name, "me")	    
	    && string_compare(name, "home")
	    && string_compare(name, "here"));
}

int ok_player_name(const char *name)
{
    const char *scan;

    if(!ok_name(name) || strlen(name) > PLAYER_NAME_LIMIT) return 0;

    for(scan = name; *scan; scan++) {
	if(!(isprint(*scan) && !isspace(*scan))) { /* was isgraph(*scan) */
	    return 0;
	}
    }

    /* lookup name to avoid conflicts */
    return (lookup_player(name) == NOTHING);
}

int ok_password(const char *password)
{
    const char *scan;

    if(*password == '\0') return 0;

    for(scan = password; *scan; scan++) {
	if(!(isprint(*scan) && !isspace(*scan))) {
	    return 0;
	}
    }

    return 1;
}

#ifdef GENDER
/*
 * pronoun_substitute()
 *
 * %-type substitutions for pronouns
 *
 * %s/%S for subjective pronouns (he/she/it, He/She/It)
 * %o/%O for objective pronouns (him/her/it, Him/Her/It)
 * %p/%P for possessive pronouns (his/her/its, His/Her/Its)
 * %n    for the player's name.
 */
void pronoun_substitute(char *result, dbref player, const char *str)
{
    char c;

    const static char *subjective[4] = { "", "it", "she", "he" };
    const static char *possessive[4] = { "", "its", "her", "his" };
    const static char *objective[4] = { "", "it", "her", "him" };

#ifdef COMPRESS
    str = uncompress(str);
#endif
    strcpy(result, db[player].name);
    result += strlen(result);
    *result++ = ' ';
    while (*str) {
	if(*str == '%') {
	    *result = '\0';
	    c = *(++str);
	    if (Genderof(player) == GENDER_UNASSIGNED) {
		switch(c) {
		  case 'n':
		  case 'N':
		  case 'o':
		  case 'O':
		  case 's':
		  case 'S':
		    strcat(result, db[player].name);
		    break;
		  case 'p':
		  case 'P':
		    strcat(result, db[player].name);
		    strcat(result, "'s");
		    break;
		  default:
		    result[0] = *str;
		    result[1] = 0;
		    break;
		}
		str++;
		result += strlen(result);
	    } else {
		switch (c) {
		  case 's':
		  case 'S':
		    strcat(result, subjective[Genderof(player)]);
		    break;
		  case 'p':
		  case 'P':
		    strcat(result, possessive[Genderof(player)]);
		    break;
		  case 'o':
		  case 'O':
		    strcat(result, objective[Genderof(player)]);
		    break;
		  case 'n':
		  case 'N':
		    strcat(result, db[player].name);
		    break;
		  default:
		    *result = *str;
		    result[1] = '\0';
		    break;
		} 
		if(isupper(c) && islower(*result)) {
		    *result = toupper(*result);
		}
		
		result += strlen(result);
		str++;
	    }
	} else {
	    *result++ = *str++;
	}
    }
    *result = '\0';
} 

#endif 
