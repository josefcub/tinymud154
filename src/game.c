#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
    
#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"

/* declarations */
static const char *dumpfile = 0;
static int epoch = 0;
static int alarm_triggered = 0;
static int alarm_block = 0;

static void fork_and_dump(void);
void dump_database(void);

void do_tune(dbref player, const char *varname, const char *valstr);

void do_dump(dbref player)
{
    if(Wizard(player)) {
	alarm_triggered = 1;
	notify(player, "Dumping...");
    } else {
	notify(player, "Sorry, you are in a no dumping zone.");
    }
}

void do_shutdown(dbref player)
{
    if(Wizard(player)) {
	writelog("SHUTDOWN: by %s(%d)\n", db[player].name, player);
	fflush(stderr);
	shutdown_flag = 1;
    } else {
	notify(player, "Your delusions of grandeur have been duly noted.");
    }
}
	
/* should be void, but it's defined as int */
static int alarm_handler(void)
{
    alarm_triggered = 1;
    if(!alarm_block) {
        fork_and_dump();
    }
    return 0;
}

static void dump_database_internal(void)
{
    char tmpfile[2048];
    FILE *f;

    sprintf(tmpfile, "%s.#%d#", dumpfile, epoch - 1);
    unlink(tmpfile);		/* nuke our predecessor */

    sprintf(tmpfile, "%s.#%d#", dumpfile, epoch);

    if((f = fopen(tmpfile, "w")) != NULL) {
	db_write(f);
	fclose(f);
	if(rename(tmpfile, dumpfile) < 0) perror(tmpfile);
    } else {
	perror(tmpfile);
    }
}

void panic(const char *message)
{
    char panicfile[2048];
    FILE *f;
    int i;

    writelog("PANIC: %s\n", message);

    /* turn off signals */
    for(i = 0; i < NSIG; i++) {
	signal(i, SIG_IGN);
    }

    /* shut down interface */
    emergency_shutdown();

    /* dump panic file */
    sprintf(panicfile, "%s.PANIC", dumpfile);
    if((f = fopen(panicfile, "w")) == NULL) {
	perror("CANNOT OPEN PANIC FILE, YOU LOSE:");
#ifndef NODUMPCORE
	signal(SIGILL, SIG_DFL);
	abort();
#endif 
	exit(135);
    } else {
	writelog("DUMPING: %s\n", panicfile);
	db_write(f);
	fclose(f);
	writelog("DUMPING: %s (done)\n", panicfile);
#ifndef NODUMPCORE
	signal(SIGILL, SIG_DFL);
	abort();
#endif 
	exit(136);
    }
}

void writelog(const char *fmt, ...)
{
    va_list list;
    struct tm *tm;
    long t;
    char buffer[2048];

    va_start(list, fmt);
    vsprintf(buffer, fmt, list);
    t = time(NULL);
    tm = localtime(&t);
    fprintf(stderr, "%d/%02d %02d:%02d:%02d %s",
	    tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
	    buffer);
    fflush(stderr);
    va_end(list);
}

void dump_database(void)
{
    epoch++;

    writelog("DUMPING: %s.#%d#\n", dumpfile, epoch);
    dump_database_internal();
    writelog("DUMPING: %s.#%d# (done)\n", dumpfile, epoch);
}

static void fork_and_dump(void)
{
    int child;

    epoch++;

    writelog("CHECKPOINTING: %s.#%d#\n", dumpfile, epoch);
#ifdef USE_VFORK
    child = vfork();
#else /* USE_VFORK */
    child = fork();
#endif 
    if(child == 0) {
	/* in the child */
	close(0);		/* get that file descriptor back */
	dump_database_internal();
	exit(0);		/* !!! */
    } else if(child < 0) {
	perror("fork_and_dump: fork()");
    }
	
    /* in the parent */
    /* reset alarm */
    alarm_triggered = 0;
    alarm(DUMP_INTERVAL);
}

static int reaper(void)
{
	  int stat;
    /* union wait stat; */

    while(wait3(&stat, WNOHANG, 0) > 0);
    return 0;
}

int init_game(const char *infile, const char *outfile)
{
   FILE *f;

   if((f = fopen(infile, "r")) == NULL) return -1;
   
   /* ok, read it in */
   writelog("LOADING: %s\n", infile);
   if(db_read(f) < 0) return -1;
   writelog("LOADING: %s (done)\n", infile);

   /* everything ok */
   fclose(f);

   /* initialize random number generator */
   srandom(getpid());

   /* set up dumper */
   if(dumpfile) free((void *) dumpfile);
   dumpfile = alloc_string(outfile);
   signal(SIGALRM, (void (*)) alarm_handler);
   signal(SIGHUP, (void (*)) alarm_handler);
   signal(SIGCHLD, (void (*)) reaper);
   alarm_triggered = 0;
   alarm(DUMP_INTERVAL);
   
   return 0;
}

/* use this only in process_command */
#define Matched(string) { if(!string_prefix((string), command)) goto bad; }

void process_command(dbref player, char *command)
{
    char *arg1;
    char *arg2;
    char *q;			/* utility */
    char *p;			/* utility */

/*    char *index(char *, char); */

    if(command == 0) abort();

    /* robustify player */
    if(player < 0 || player >= db_top || Typeof(player) != TYPE_PLAYER) {
	writelog("process_command: bad player %d\n", player);
	return;
    }

#ifdef LOG_COMMANDS
    writelog("COMMAND from %s(%d) in %s(%d): %s\n",
	    db[player].name, player,
	    db[db[player].location].name, db[player].location,
	    command);
#endif

#ifdef TIMESTAMPS
    db[player].usecnt++;
    db[player].lastused = time (0);
#endif 

    /* eat leading whitespace */
    while(*command && isspace(*command)) command++;

    /* eat extra white space */
    q = p = command;
    while(*p) {
	/* scan over word */
	while(*p && !isspace(*p)) *q++ = *p++;
	/* smash spaces */
	while(*p && isspace(*++p));
	if(*p) *q++ = ' '; /* add a space to separate next word */
    }
    /* terminate */
    *q = '\0';

    /* block dump to prevent db inconsistencies from showing up */
    alarm_block = 1;

    /* check for single-character commands */
    if(*command == SAY_TOKEN) {
	do_say(player, command+1, NULL);
    } else if(*command == POSE_TOKEN) {
	do_pose(player, command+1, NULL);
    } else if(can_move(player, command)) {
	/* command is an exact match for an exit */
	do_move(player, command);
    } else {
	/* parse arguments */

	/* find arg1 */
	/* move over command word */
	for(arg1 = command; *arg1 && !isspace(*arg1); arg1++);
	/* truncate command */
	if(*arg1) *arg1++ = '\0';

	/* move over spaces */
	while(*arg1 && isspace(*arg1)) arg1++;

	/* find end of arg1, start of arg2 */
	for(arg2 = arg1; *arg2 && *arg2 != ARG_DELIMITER; arg2++);

	/* truncate arg1 */
	for(p = arg2 - 1; p >= arg1 && isspace(*p); p--) *p = '\0';

	/* go past delimiter if present */
	if(*arg2) *arg2++ = '\0';
	while(*arg2 && isspace(*arg2)) arg2++;

	switch(command[0]) {
	  case '@':
	    switch(command[1]) {
	      case 'b':
	      case 'B':
	      	switch (command[2]) {
		  case 'o':
		  case 'O':
		    switch (command[3]) {
		      case 'o':
		      case 'O':	Matched("@boot");
				do_boot(player, arg1);
		      		break;
		     default:	goto bad;
		    }
		    break;
		  default: goto bad;
		}
		break;
	      case 'c':
	      case 'C':
		/* chown, create */
		switch(command[2]) {
		  case 'h':
		  case 'H':
		    Matched("@chown");
		    do_chown(player, arg1, arg2);
		    break;
#ifdef RECYCLE
		  case 'o':
		  case 'O':
		    Matched("@count");
		    do_count(player, arg1);
		    break;
#endif 
		  case 'r':
		  case 'R':
		    Matched("@create");
		    do_create(player, arg1, atol(arg2));
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'd':
	      case 'D':
		/* describe, dig, or dump */
		switch(command[2]) {
		  case 'a':
		  case 'A':
		    Matched("@date");
		    do_date(player, arg1, arg2);
		    break;
		  case 'e':
		  case 'E':
		    Matched("@describe");
		    do_describe(player, arg1, arg2);
		    break;
		  case 'i':
		  case 'I':
		    Matched("@dig");
		    do_dig(player, arg1);
		    break;
		  case 'u':
		  case 'U':
		    Matched("@dump");
		    do_dump(player);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'f':
		/* fail, find, or force */
		switch(command[2]) {
		  case 'a':
		  case 'A':
		    Matched("@fail");
		    do_fail(player, arg1, arg2);
		    break;
		  case 'i':
		  case 'I':
		    Matched("@find");
		    do_find(player, arg1);
		    break;
		  case 'o':
		  case 'O':
		    Matched("@force");
		    do_force(player, arg1, arg2);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'l':
	      case 'L':
		/* lock or link */
		switch(command[2]) {
		  case 'i':
		  case 'I':
		    Matched("@link");
		    do_link(player, arg1, arg2);
		    break;
		  case 'o':
		  case 'O':
		    Matched("@lock");
		    do_lock(player, arg1, arg2);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'm':
	      case 'M':
	      	Matched("@mass_teleport");
		do_mass_teleport(player, arg1);
	      	break;
	      case 'n':
	      case 'N':
		/* @name or @newpassword */
		switch(command[2]) {
		  case 'a':
		  case 'A':
		    Matched("@name");
		    do_name(player, arg1, arg2);
		    break;
		  case 'e':
		    if(strcmp(command, "@newpassword")) goto bad;
		    do_newpassword(player, arg1, arg2);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'o':
	      case 'O':
		switch(command[2]) {
		  case 'f':
		  case 'F':
		    Matched("@ofail");
		    do_ofail(player, arg1, arg2);
		    break;
		  case 'p':
		  case 'P':
		    Matched("@open");
		    do_open(player, arg1, arg2);
		    break;
		  case 's':
		  case 'S':
		    Matched("@osuccess");
		    do_osuccess(player, arg1, arg2);
		    break;
		  case 'w':
		  case 'W':
		    Matched("@owned");
		    do_owned(player, arg1);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'p':
	      case 'P':
 		switch(command[2]) {
 		  case 'a':
 		  case 'A':
 		    Matched("@password");
 		    do_password(player, arg1, arg2);
 		    break;
#ifdef REGISTRATION		    
 		  case 'c':
 		  case 'C':
 		    Matched("@pcreate");
 		    do_pcreate(player, arg1, arg2);
 		    break;
#endif 
	          default: goto bad;
 		}
		break;
#ifdef RECYCLE
	      case 'r':
	      case 'R':
	      	/* Recycle */
		Matched("@recycle");
		do_recycle(player, arg1);
		break;
#endif
	      case 's':
	      case 'S':
		/* set, shutdown, success */
		switch(command[2]) {
		  case 'e':
		  case 'E':
		    Matched("@set");
		    do_set(player, arg1, arg2);
		    break;
		  case 'h':
		    if(strcmp(command, "@shutdown")) goto bad;
		    do_shutdown(player);
		    break;
		  case 't':
		  case 'T':
		    Matched("@stats");
		    do_stats(player, arg1);
		    break;
		  case 'u':
		  case 'U':
		    Matched("@success");
		    do_success(player, arg1, arg2);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 't':
	      case 'T':
		switch(command[2]) {
		  case 'e':
		  case 'E':
		    Matched("@teleport");
		    do_teleport(player, arg1, arg2);
		    break;
		  case 'o':
		  case 'O':
		    switch(command[3]) {
		      case 'a':
		      case 'A':
		        Matched("@toad");
			do_bobble(player, arg1, arg2);
		        break;
		      case 'p':
		      case 'P':
		        Matched("@top");
			do_top(player, arg1, arg2);
		        break;
		      default:
		        goto bad;
		    }
		    break;
		  case 'u':
		  case 'U':
		    Matched("@tune");
		    do_tune(player, arg1, arg2);
		    break;
		  default:
		    goto bad;
		}
		break;
	      case 'u':
	      case 'U':
		if(string_prefix(command, "@unli")) {
		    Matched("@unlink");
		    do_unlink(player, arg1);
		} else if(string_prefix(command, "@unlo")) {
		    Matched("@unlock");
		    do_unlock(player, arg1);
		} else if(string_prefix(command, "@unt")) {
		    Matched("@untoad");
		    do_unbobble(player, arg1, arg2);
		} else {
		    goto bad;
		}
		break;
	      case 'V':
	      case 'v':
		if(string_prefix(command, "@ver")) {
		  Matched("@version");
		  notify(player, VERSION);
		} else {
		    goto bad;
		}
		break;
	      case 'w':
		if(strcmp(command, "@wall")) goto bad;
		do_wall(player, arg1, arg2);
		break;
	      default:
		goto bad;
	    }
	    break;
	  case 'd':
	  case 'D':
	    Matched("drop");
	    do_drop(player, arg1);
	    break;
	  case 'e':
	  case 'E':
	    if (command[1] == 'x' || command[1] == 'X') {
		Matched("examine");
		do_examine(player, arg1);
		break;
	    } else {
		goto bad;
	    }
	  case 'g':
	  case 'G':
	    /* get, give, go, or gripe */
	    switch(command[1]) {
	      case 'e':
	      case 'E':
		Matched("get");
		do_get(player, arg1);
		break;
	      case 'i':
	      case 'I':
		Matched("give");
		do_give(player, arg1, atol(arg2));
		break;
	      case 'o':
	      case 'O':
		Matched("goto");
		do_move(player, arg1);
		break;
	      case 'r':
	      case 'R':
		Matched("gripe");
		do_gripe(player, arg1, arg2);
		break;
	      default:
		goto bad;
	    }
	    break;
	  case 'h':
	  case 'H':
	    Matched("help");
	    do_help(player);
	    break;
	  case 'i':
	  case 'I':
	    Matched("inventory");
	    do_inventory(player);
	    break;
	  case 'k':
	  case 'K':
	    Matched("kill");
	    do_kill(player, arg1, atol(arg2));
	    break;
	  case 'l':
	  case 'L':
	    Matched("look");
	    do_look_at(player, arg1);
	    break;
	  case 'm':
	  case 'M':
	    Matched("move");
	    do_move(player, arg1);
	    break;
	  case 'n':
	  case 'N':
	    /* news */
	    if(string_compare(command, "news")) goto bad;
	    do_news(player);
	    break;
	  case 'p':
	  case 'P':
	    Matched("page");
	    do_page(player, arg1, arg2);
	    break;
	  case 'r':
	  case 'R':
	    switch(command[1]) {
	      case 'e':
	      case 'E':
		Matched("read"); /* undocumented alias for look at */
		do_look_at(player, arg1);
		break;
	      case 'o':
	      case 'O':
		Matched("rob");
		do_rob(player, arg1);
		break;
	      default:
		goto bad;
	    }
	    break;
	  case 's':
	  case 'S':
	    /* say, "score" */
	    switch(command[1]) {
	      case 'a':
	      case 'A':
		Matched("say");
		do_say(player, arg1, arg2);
		break;
	      case 'c':
	      case 'C':
		Matched("score");
		do_score(player);
		break;
	      default:
		goto bad;
	    }
	    break;
	  case 't':
	  case 'T':
	    switch(command[1]) {
	      case 'a':
	      case 'A':
		Matched("take");
		do_get(player, arg1);
		break;
	      case 'h':
	      case 'H':
		Matched("throw");
		do_drop(player, arg1);
		break;
	      default:
		goto bad;
	    }
	    break;
	  case 'w':
	  case 'W':
	    Matched("whisper");
	    do_whisper(player, arg1, arg2);
	    break;
	  default:
	  bad:
	    notify(player, "Huh?  (Type \"help\" for help.)");
#ifdef LOG_FAILED_COMMANDS
	    if(!controls(player, db[player].location)) {
		writelog("HUH from %s(%d) in %s(%d)[%s]: %s %s\n",
			db[player].name, player,
			db[db[player].location].name,
			db[player].location,
			db[db[db[player].location].owner].name,
			command,
			reconstruct_message(arg1, arg2));
	    }
#endif 
	    break;
	}
    }

    /* unblock alarms */
    alarm_block = 0;
    if(alarm_triggered) {
	fork_and_dump();
    }
}

#undef Matched
