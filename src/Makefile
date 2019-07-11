#################################################################
#
# Makefile for TinyMUD source code
#
# 04/28/04:	Cleanups, reorg.
# 06/06/90:	First known revision.
#
#################################################################

CC= cc
OPTIM= --pedantic -g -O2

#################################################################
# Here's a list of DEFINEs that can be used to modify the 
# behavior of the TinyMUD server.
#
# DEFINE		Description
#################################################################
# -DCOMPRESS		Attempts to compress string data.
# -DCONNECT_MESSAGES	Adds messages for connecting and disconn-
#			ecting.
# -DDB_DOUBLING		Use this if your realloc() does not
#			allocate in powers of 2.  (It wastes
#			space otherwise)
# -DDB_INITIAL_SIZE=xxx	db.c initially allocates enough space for
#			10,000 objects then grows the space if it
#			needs to.  Use this to change the value to
#			xxx. (minimum of 1)
# -DDETACH		Detaches the netmud server from the terminal
#			on startup.  LOG_FILE (in config.h) receives 
#                       all messages.
# -DGENDER		Includes Stephen White's gender flags and 
#			pronoun-substitution code.
# -DGOD_PRIV		Gives Player #1 the ability to (un)set
#			WIZARD flags, and enables extended WHO
#			privledges.
# -DGOD_MODE		Restricts host name resolution, and some 
#                       commands, to Player#1. Requires GOD_PRIV.
# -DGOD_ONLY_PCREATE	Restricts @pcreate to Player #1.  Requires
#			GOD_PRIV to function.
# -DHOST_NAME		Enables host name resolution for logs and
#			Wizard/God WHO listings.
# -DLOG_COMMANDS	Log all command input to the server.
# -DNODUMPCORE		Disables core dumps when crashing.
# -DNOFAKES		Enables name checking for player creation.
#			(A, An, The, You, Your, Going, Huh?)
# -DPLAYER_LIST		Use hashed player lists for player lookups.
# -DQUIET_WHISPER	Eliminates the third-party notification of
#			a whisper.
# -DRECYCLE		Adds the @count and @recycle commands.
# -DREGISTRATION	Disables welcome-screen player creation.
# -DRESTRICTED_BUILDING Restricts object-creating commands to
#			users with a BUILDER bit.
# -DROBOT_MODE		Adds the ROBOT (ZOMBIE) flag.
# -DTEST_MALLOC		Includes code for keeping track of the 
#			number of blocks allocated.
# -DTIMESTAMPS		Marks all things with a timestamp/use count.
# -DUSE_VFORK		Use vfork() instead of fork()
#################################################################

# A reasonable set of DEFINEs.
DEFS= -DGOD_PRIV -DCOMPRESS -DQUIET_WHISPER -DGENDER -DHOST_NAME \
      -DCONNECT_MESSAGES -DPLAYER_LIST -DDETACH \
      -DRECYCLE -DNOFAKES -DTIMESTAMPS -DREGISTRATION \
      -DQUIET_WHISPER -DRESTRICTED_BUILDING -DLOG_COMMANDS

CFLAGS= $(OPTIM) $(DEFS)

# Everything needed to use db.c

DBFILES= db.c compress.c player_list.c stringutil.c
DBOFILES= db.o compress.o player_list.o stringutil.o

# Everything except interface.c --- allows for multiple interfaces
CFILES= create.c game.c help.c look.c match.c move.c player.c predicates.c \
	rob.c set.c speech.c utils.c wiz.c game.c \
	boolexp.c unparse.c conc.c oldinterface.c $(DBFILES)

# .o versions of above
OFILES= create.o game.o help.o look.o match.o move.o player.o predicates.o \
	rob.o set.o speech.o utils.o wiz.o boolexp.o \
	unparse.o $(DBOFILES)

OUTFILES= netmud netmud.conc concentrate dump paths sanity-check \
	  extract decompress

all: extract sanity-check dump decompress netmud netmud.conc concentrate

netmud.conc: $P interface.o $(OFILES)
	$(CC) $(CFLAGS) -o netmud.conc interface.o $(OFILES)

netmud: $P oldinterface.o $(OFILES)
	$(CC) $(CFLAGS) -o netmud oldinterface.o $(OFILES)

concentrate: $P conc.c config.h
	$(CC) $(CFLAGS) -o concentrate conc.c

dump: $P dump.o unparse.o $(DBOFILES)
	$(CC) $(CFLAGS) -o dump dump.o unparse.o $(DBOFILES) 

sanity-check: $P sanity-check.o utils.o $(DBOFILES) 
	$(CC) $(CFLAGS) -o sanity-check sanity-check.o utils.o $(DBOFILES)

extract: $P extract.o utils.o $(DBOFILES) 
	$(CC) $(CFLAGS) -o extract extract.o utils.o $(DBOFILES)

decompress: $P decompress.o compress.o
	$(CC) $(CFLAGS) -o decompress decompress.o compress.o

clean:
	-rm -f *.o a.out core gmon.out $(OUTFILES)

#################################################################
#
# DO NOT EDIT ANYTHING BELOW HERE!
#
#################################################################
boolexp.o: boolexp.c copyright.h db.h match.h externs.h config.h interface.h
compress.o: compress.c
create.o: create.c copyright.h db.h config.h interface.h externs.h
db.o: db.c copyright.h db.h config.h
decompress.o: decompress.c
dump.o: dump.c copyright.h db.h
extract.o: extract.c copyright.h db.h
fix.o: fix.c copyright.h db.h config.h
game.o: game.c copyright.h db.h config.h interface.h match.h externs.h
help.o: help.c copyright.h db.h config.h interface.h externs.h
interface.o: interface.c copyright.h db.h interface.h config.h
look.o: look.c copyright.h db.h config.h interface.h match.h externs.h
match.o: match.c copyright.h db.h config.h match.h
move.o: move.c copyright.h db.h config.h interface.h match.h externs.h
oldinterface.o: oldinterface.c copyright.h db.h interface.h config.h
player.o: player.c copyright.h db.h config.h interface.h externs.h
player_list.o: player_list.c copyright.h db.h config.h interface.h externs.h
predicates.o: predicates.c copyright.h db.h interface.h config.h externs.h
rob.o: rob.c copyright.h db.h config.h interface.h match.h externs.h
sanity-check.o: sanity-check.c copyright.h db.h config.h
set.o: set.c copyright.h db.h config.h match.h interface.h externs.h
speech.o: speech.c copyright.h db.h interface.h match.h config.h externs.h
stringutil.o: stringutil.c copyright.h externs.h
unparse.o: unparse.c db.h externs.h config.h interface.h
utils.o: utils.c copyright.h db.h
wiz.o: wiz.c copyright.h db.h interface.h match.h externs.h
config.h: copyright.h
copyright.h:
db.h: copyright.h
externs.h: copyright.h db.h
interface.h: copyright.h db.h
match.h: copyright.h db.h
