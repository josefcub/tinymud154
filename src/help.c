#include "copyright.h"

/* commands for giving help */

#include "db.h"
#include "config.h"
#include "interface.h"
#include "externs.h"

int spit_file(dbref player, const char *filename)
{
    FILE *f;
    char buf[BUFFER_LEN];
    char *p;

    if((f = fopen(filename, "r")) == NULL) {
	return (0);
    } else {
	while(fgets(buf, sizeof buf, f)) {
	    for(p = buf; *p; p++) if(*p == '\n') {
		*p = '\0';
		break;
	    }
	    notify(player, buf);
	}
	fclose(f);
	return (1);
    }
}

void do_help(dbref player)
{
    if (!spit_file(player, HELP_FILE))
    { notify(player, "Sorry, the help file is missing right now.");
      writelog("GRIPE automatically generated for %s(%d): no help file %s\n",
	       db[player].name, player, HELP_FILE);      
    }
}

void do_news(dbref player)
{ int result = 0;

  result += spit_file(player, NEWS_FILE);
  result += spit_file(player, MOTD_FILE);
  if (Wizard(player)) result += spit_file(player, WIZARD_FILE);

  if (result == 0)
  { notify(player, "No news today."); }
}

void do_motd(dbref player)
{
    spit_file(player, MOTD_FILE);
    if (Wizard(player)) spit_file(player, WIZARD_FILE);
}
