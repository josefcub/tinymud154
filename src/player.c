#include "copyright.h"

#include "db.h"
#include "config.h"
#include "interface.h"
#include "externs.h"

#ifndef PLAYER_LIST    
/* don't use this, it's expensive */
/* maybe soon we'll put in a hash table */
dbref lookup_player(const char *name)
{
    dbref i;

    for(i = 0; i < db_top; i++) {
	if(Typeof(i) == TYPE_PLAYER
	   && db[i].name && !string_compare(db[i].name, name)) return i;
    }
    return NOTHING;
}
#endif

dbref connect_player(const char *name, const char *password)
{
    dbref player;

    if((player = lookup_player(name)) == NOTHING) return NOTHING;
    if(db[player].password
       && *db[player].password
       &&strcmp(db[player].password, password)) return NOTHING;

    return player;
}

dbref create_player(const char *name, const char *password)
{
    dbref player;

    if(!ok_player_name(name) || !ok_password(password)) return NOTHING;

    /* else he doesn't already exist, create him */
    player = new_object();

    /* initialize everything */
    db[player].name = alloc_string(name);
    db[player].location = PLAYER_START;
    db[player].exits = PLAYER_START;	/* home */
    db[player].owner = player;
    db[player].flags = TYPE_PLAYER;
    db[player].password = alloc_string(password);
    
    /* link him to PLAYER_START */
    PUSH(player, db[PLAYER_START].contents);

#ifdef PLAYER_LIST
    add_player(player);
#endif

    return player;
}

void do_password(dbref player, const char *old, const char *newobj)
{
    if(!db[player].password || strcmp(old, db[player].password)) {
	notify(player, "Sorry");
    } else if(!ok_password(newobj)) {
	notify(player, "Bad new password.");
    } else {
	free((void *) db[player].password);
	db[player].password = alloc_string(newobj);
	notify(player, "Password changed.");
    }
}
