#ifdef PLAYER_LIST

#include "copyright.h"

#include "db.h"
#include "config.h"
#include "interface.h"
#include "externs.h"

#include <stdlib.h>
#include <ctype.h>

#define PLAYER_LIST_SIZE (1 << 12) /* must be a power of 2 */

static dbref hash_function_table[256];
static int hft_initialized = 0;

#define DOWNCASE(x) (isupper(x) ? tolower(x) : (x))

static void init_hft(void)
{
    int i;

    for(i = 0; i < 256; i++) {
	hash_function_table[i] = random() & (PLAYER_LIST_SIZE - 1);
    }
    hft_initialized = 1;
}

static dbref hash_function(const char *string)
{
    dbref hash;

    if(!hft_initialized) init_hft();
    hash = 0;
    for(; *string; string++) {
	hash ^= ((hash >> 1) ^ hash_function_table[DOWNCASE(*string)]);
    }
    return(hash);
}

struct pl_elt {
    dbref player;		/* pointer to player */
				/* key is db[player].name */
    struct pl_elt *next;
};

static struct pl_elt *player_list[PLAYER_LIST_SIZE];
static int pl_used = 0;

void clear_players(void)
{
    int i;
    struct pl_elt *e;
    struct pl_elt *next;

    for(i = 0; i < PLAYER_LIST_SIZE; i++) {
	if(pl_used) {
	    for(e = player_list[i]; e; e = next) {
		next = e->next;
		free((void *) e);
	    }
	}
	player_list[i] = 0;
    }
    pl_used = 1;
}

void add_player(dbref player)
{
    dbref hash;
    struct pl_elt *e;

    hash = hash_function(db[player].name);

    e = (struct pl_elt *) malloc(sizeof(struct pl_elt));
    e->player = player;
    e->next = player_list[hash];
    player_list[hash] = e;
}

dbref lookup_player(const char *name)
{
    struct pl_elt *e;

    for(e = player_list[hash_function(name)]; e; e = e->next) {
	if(!string_compare(db[e->player].name, name)) return e->player;
    }
    return NOTHING;
}

void delete_player(dbref player)
{
    dbref hash;
    struct pl_elt *prev;
    struct pl_elt *e;

    hash = hash_function(db[player].name);
    if((e = player_list[hash]) == 0) {
	return;
    } else if(e->player == player) {
	/* it's the first one */
	player_list[hash] = e->next;
	free((void *) e);
    } else {
	for(prev = e, e = e->next; e; prev = e, e = e->next) {
	    if(e->player == player) {
		/* got it */
		prev->next = e->next;
		free((void *) e);
		break;
	    }
	}
    }
}

#endif /* PLAYER_LIST */

