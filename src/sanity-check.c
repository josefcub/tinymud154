#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "config.h"
#include "externs.h"

# define HAS_ENTRANCES 0x40000000

void check_exits(dbref i)
{
    dbref exit;
    int count;

    count = 10000;
    for(exit = db[i].exits;
	exit != NOTHING;
	exit = db[exit].next) {
	if(exit < 0 || exit >= db_top || Typeof(exit) != TYPE_EXIT) {
	    printf("%d has bad exit %d\n", i, exit);
	    break;
	}

	/* set type of exit to be really strange */
	db[exit].flags = 4;	/* nonexistent type */

	if(count-- < 0) {
	    printf("%d has looping exits\n", i);
	    break;
	}
    }
}

void check_contents(dbref i)
{
    dbref thing;
    dbref loc;
    int count;

    count = 10000;
    for(thing = db[i].contents;
	thing != NOTHING;
	thing = db[thing].next) {
	if(thing < 0 || thing >= db_top || Typeof(thing) == TYPE_ROOM) {
	    printf("%d contains bad object %d\n", i, thing);
	    break;
	} else if(Typeof(thing) == TYPE_EXIT) {
	    db[thing].flags = 4; /* nonexistent type */
	} else if((loc = db[thing].location) != i) {
	    printf("%d in %d but location is %d\n", thing, i, loc);
	}
	if(count-- < 0) {
	    printf("%d has looping contents\n", i);
	    break;
	}
    }
}

void check_location(dbref i)
{
    dbref loc;

    loc = db[i].location;
    if(loc < 0 || loc >= db_top) {
	printf("%d has bad loc %d\n", i, loc);
    } else if(!member(i, db[loc].contents)) {
	printf("%d not in loc %d\n", i, loc);
    }
}

void check_owner(dbref i)
{
    dbref owner;

    owner = db[i].owner;
    if(owner < 0 || owner >= db_top || Typeof(owner) != TYPE_PLAYER) {
	printf("Object %s(%d) has bad owner %d\n",
	       db[i].name, i, owner);
    }
}

void check_pennies(dbref i)
{
    dbref pennies;

    pennies = db[i].pennies;

    switch(Typeof(i)) {
      case TYPE_ROOM:
      case TYPE_EXIT:
	break;
      case TYPE_PLAYER:
	if(pennies < 0 || pennies > MAX_PENNIES+100) {
	    printf("Player %s(%d) has %d pennies\n", db[i].name, i, pennies);
	}
	break;
      case TYPE_THING:
	if(pennies < 0 || pennies > MAX_OBJECT_ENDOWMENT) {
	    printf("Object %s(%d) endowed with %d pennies\n",
		   db[i].name, i, pennies);
	}
	break;
    }
}

int main(int argc, char **argv)
{
    dbref i;

    if(db_read(stdin) < 0) {
	puts("Database load failed!");
	exit(1);
    } 

    puts("Done loading database");

    for(i = 0; i < db_top; i++) {
	check_pennies(i);
	check_owner(i);
	switch(Typeof(i)) {
	  case TYPE_PLAYER:
	    check_contents(i);
	    check_location(i);
	    if(Wizard(i)) printf("Wizard: %s(%d)\n", db[i].name, i);
	    if(Dark(i)) printf("Dark: %s(%d)\n", db[i].name, i);
#ifdef ROBOT_MODE
	    if(Robot(i)) printf("Robot: %s(%d)\n", db[i].name, i);
#endif
	    if(db[i].password == NULL)
	    { printf ("Null password: %s(%d)\n", db[i].name, i); }
	    break;
	  case TYPE_THING:
	    check_location(i);
	    break;
	  case TYPE_ROOM:
#ifdef ROBOT_MODE
	    if(Robot(i)) printf("UnRobot: %s(%d)\n", db[i].name, i);
#endif
	    check_contents(i);
	    check_exits(i);
	    break;
	}
    }

    /* scan for unattached exits */
    for(i = 0; i < db_top; i++) {
	if(Typeof(i) == TYPE_EXIT) {
	    printf("Unattached exit %d\n", i);
	}
    }
	    
    /* scan for unattached exits */
    for(i = 0; i < db_top; i++) {
	if(Typeof(i) == TYPE_EXIT) {
	    printf("Unattached exit %d\n", i);
	}
    }
	    
    /*---- scan for unattached rooms ----*/
    
    /* Clear entry flags */
    for(i = 0; i < db_top; i++) db[i].flags &= ~HAS_ENTRANCES;

    /* For each exit, set its destination entry bit */    
    for(i = 0; i < db_top; i++) {
	if(Typeof(i) == TYPE_EXIT || Typeof(i) == 4) {
	    if(db[i].location >= 0) db[db[i].location].flags |= HAS_ENTRANCES;
	}
    }

    /* Now print every room with no entrances */    
    for(i = 0; i < db_top; i++) {
	if(Typeof(i) == TYPE_ROOM && 
	   (db[i].flags & HAS_ENTRANCES) == 0) {
	    if (db[i].exits == NOTHING && db[i].contents == NOTHING) {
		printf ("Room %d has no entrances, exits, or contents\n", i);
	    } else {
		printf ("Room %d has no entrances\n", i);
	    }
	}
    }
	    
    exit(0);
    return(0);
}
