#include "copyright.h"

/* netmud server version info */
#define VERSION "TinyMUD (1.5.4.1), based on Islandia/TinyHELL (1.5.4f)"

/* room number of player start location */
#define PLAYER_START ((dbref) 4)

/* minimum cost to create various things */
#define OBJECT_COST 10
#define EXIT_COST 1
#define LINK_COST 1
#define ROOM_COST 10

/* cost for various special commands */
#define FIND_COST 20
#define PAGE_COST 1

/* limit on player name length */
#define PLAYER_NAME_LIMIT 16

/* magic cookies */
#define NOT_TOKEN '!'
#define AND_TOKEN '&'
#define OR_TOKEN '|'
#define THING_TOKEN 'x'
#define LOOKUP_TOKEN '*'
#define NUMBER_TOKEN '#'
#define ARG_DELIMITER '='

/* magic command cookies */
#define SAY_TOKEN '"'
#define POSE_TOKEN ':'

/* amount of object endowment, based on cost */
#define MAX_OBJECT_ENDOWMENT 100
#define OBJECT_ENDOWMENT(cost) (((cost)-5)/5)

/* amount at which temple stops being so profitable */
#define MAX_PENNIES 10000

/* penny generation parameters */
#define PENNY_RATE 10		/* 1/chance of getting a penny per room */

/* costs of kill command */
#define KILL_BASE_COST 100	/* prob = expenditure/KILL_BASE_COST */
#define KILL_MIN_COST 10
#define KILL_BONUS 50		/* paid to victim */

/* delimiter for lists of exit aliases */
#define EXIT_DELIMITER ';'

/* timing stuff */
#define DUMP_INTERVAL 10800   /* 3 hours between dumps */
#define COMMAND_TIME_MSEC  1000	/* time slice length in milliseconds */
#define COMMAND_BURST_SIZE   20	/* commands allowed per user in a burst */
#define COMMANDS_PER_TIME     1	/* commands per time slice after burst */
#define COMMAND_ALLOW_EXTRA   1 /* Allow commands over quota if not busy */

/* maximum amount of queued output */
#define MAX_OUTPUT 16384

/* Essential Messages and Schtuff */
#define TINYPORT 4201
#define INTERNAL_PORT 4200
/* #define WELCOME_MESSAGE "Welcome to TinyMUD\nTo connect to your existing character, enter \"connect name password\"\nTo create a new character, enter \"create name password\"\nUse the news command to get up-to-date news on program changes.\n\nYou can disconnect using the QUIT command, which must be capitalized as shown.\n\nUse the WHO command to find out who is currently active.\n\n" */
#define WELCOME_MESSAGE "Welcome to...\n"
#define REGISTER_MESSAGE "You must send mail to the TinyMUD system administrator to get an account.\nUnfortunately, he or she forgot to set an Email address in this part of the\n code.  Good luck!\n\n"

#define LEAVE_MESSAGE "\n*** Disconnected.\n"

#define QUIT_COMMAND "QUIT"
#define WHO_COMMAND "WHO"
#define PREFIX_COMMAND "OUTPUTPREFIX"
#define SUFFIX_COMMAND "OUTPUTSUFFIX"

#define HELP_FILE	"/home/josefcub/tinymud/game/data/help.txt"
#define NEWS_FILE	"/home/josefcub/tinymud/game/data/news.txt"
#define MOTD_FILE	"/home/josefcub/tinymud/game/data/motd.txt"
#define CONNECT_FILE	"/home/josefcub/tinymud/game/data/welcome.txt"
#define WIZARD_FILE	"/home/josefcub/tinymud/game/data/wizard.txt"
#define LOG_FILE	"/home/josefcub/tinymud/game/logs/commands"

#ifdef LOCKOUT
#define LOCKOUT_FILE "/home/josefcub/tinymud/game/data/lockout.txt"
#endif 

/*
 * This section defines the flag markers used by unparse.c.
 * Of course, you can define them any way you want, but then
 * you may have to change other messages to make the letters 
 * match their flag types.
 */

# define TYPE_CODES	"R-EP" /* Room, thing, exit, player */
# define STICKY_MARK	'S'
# define DARK_MARK	'D'
# define LINK_MARK	'L'
# define ABODE_MARK	'A'
# define HAVEN_MARK	'H'
# define UNWANTED_MARK	'U'
# define MALE_MARK	'M'
# define FEMALE_MARK	'F'
# define NEUTER_MARK	'N'

# define WIZARD_MARK	'W'  /* For Tinker */
# define TEMPLE_MARK	'T'  /* For Temple */
# define ROBOT_MARK	'Z'  /* For Zombie */
# define BUILDER_MARK	'B'  /* For Builder */
