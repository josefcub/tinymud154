This is TinyMUD version 1.5.4.1, a player-extensible multiuser adventure
game system.

See the file src/copyright.h for information on the copyright under which
TinyMUD 1.5.4.1 is distributed.  See the file CHANGES for a list of
changes made in each release.

See also the original README file, appended at the bottom of this one, for
information pertaining to both this, and the original release.

Compiling TinyMUD:

The code is written in C.  However, the source code originates from
1990.  In April 2004 several ugly hacks were
made to the original source code of 1.5.4f in order to enable it to compile
under Linux kernel 2.2.19 with glibc 2.1.3.  This was further hacked in July
2019 to compile under Ubuntu 18.04 with GCC 7.4

To compile the netmud server, and its companion utilitiies simply change to
the src/ directory and type:

make

The compilation process will take time, and you will see several warnings
listed as the compile progresses.  If all went well, you should possess
eight executables, listed below:

	concentrate	network concentrator for netmud
	decompress	decompresses a database file
	dump		dumps a database in human-readable format to stdout
	extract		extracts objects from a database
	netmud		TinyMUD server
	netmud.conc	TinyMUD server, with network concentrator
	sanity-check	Checks a database file from stdin for basic sanity

Running TinyMUD:

NOTE: See the original README file below for information on the parameters needed
by each executable, if you know what you're doing, and want to do things the
hard way. ;-)

To ensure that your system works right off the bat, check (and edit,
if necessary) src/config.h, game/restart, and game/do_gripes BEFORE you
compile, to ensure that the directory structure listed in those files
matches the one you are currently using.  You _shouldn't_ have to edit these
files at all, but a ittle precaution never hurts, especially if you want to
run it on a different port.

Finally, to activate the server, cd to $BASE/game and type './restart'.  The
restart script should indicate status, and tell you that netmud either
started successfully, or that an error was encountered.  If the startup was
successful, you can now 'telnet localhost 4201' and gain access to the
system.

The do_gripes script as provided is designed to be called from a cron job,
and is a handy way to extrapolate server messages and player gripes.  It
e-mails the account owner at the end of its run, also making administration
easier.

See the original README file below, and the CHANGES file for information on
garbage collection, which is NOT automatic in TinyMUD 1.5.4.

Databases:

The original tarball for Islandia 1.5.4f came with 'small.db' and
'minimal.db'.  These are preserved in 'game/data/databases.tgz'.  The
database game/data/tinymud.db is identical to the minimal.db file. 

To log in for the first time, using the supplied tinymud.db, #1's name is
'One', and his password is 'potrzebie'.  Aside from One, the only other
thing the database contains is Room Zero.

A script, 'clean_db' is provided in order to automate garbage collection
during a scheduled MUD downtime.  Edit the variables at the beginning to
match your database, and then simply run the script.  It rotates the
databases automatically too, in case of a severe error.

Additional Notes:

The source code to 1.5.4f has been preserved in src/original.src.tgz, so
that the new directory structure can be used on one of the older systems, if
desired.

Hopefully, someone gets some enjoyment from this effort of mine.  Coming
from a TinyMUCK/FuzzBall background, the simplicity of use combined with the
features that allowed things like TinyMUCK/FuzzBall to exist gives me a kind
of joy, and a lack of pressure as compared to having worked on MUCKs like
FuzzyLogic.

Take Care!
AJC
July 2019

------------------------------------------------------------------------------
Original README:
------------------------------------------------------------------------------

This is TinyMUD version 1.5.4f, a user-extendible
multi-user adventure game.

See the file copyright.h for information on the copyright under which
TinyMUD 1.5.4f is distributed.  See the file CHANGES for a list of
changes made in each release.

Compiling TinyMUD:

The code is written in ANSI C, and is known to compile and run under
Mach (a variant of 4.3BSD Unix) on a Sun-3 and uVax-2 using GCC, on on
an IBM RT using hc, and on a DECstation 3100 using the MipsCo C
compiler.  It has also been compiled on a Vax running a fairly generic
4.2BSD system, and on a NeXT machine.

I do not expect that it will work well on non-BSD systems, although
most of the BSD-isms are concentrated in interface.c and in a few
calls here and there to bzero() and such.  Please let me know if you
manage to port TinyMUD to another type of system.


Programs contained in the distribution:

netmud

 This is the actual server program; it is invoked as 

    netmud source-file dump-file [port [log-file]]

 If port is not specified it defaults to 4201.  The initial database
will be read in from source-file, which must contain at least the two
objects in minimal.db to work well.  The file small.db, which contains
the core of the original TinyMUD universe, may be a better place to
start a new universe from.  The netmud process will write a checkpoint
out to dump-file every 3600 seconds; the interval can be changed by
setting DUMP_INTERVAL in config.h.


netmud.conc & concentrate

 This is the same as netmud, but allows up to 3600 users at one time.
The concentrate program listens on the external port and mulitplexes
up to 60 connections into the server program.  There can be up to 60
concentrators, although in practice you'll seldom need more than a
half-dozen.  It is invoked as follows (ext-port is the port number
users telnet to, int-port is the port number the server uses to
communicate with the concetrators):

    netmud.conc  source-file dump-file [ext-port int-port [log-file]]

sanity-check

 Usage: sanity-check < source-file

 Performs several simple consistency checks on the specified database
 file.  Also useful for detecting surplus WIZARDs and people with
 excess pocket change.


extract

 This is the program that was used to create the file small.db
distributed with TinyMUD.  It is used to extract a part of a database
as a connected whole.

Examples:

 extract < source-file > output-file

 Extracts location #0, player #1, and all objects owned by player #1;
 these objects are renumbered into a contiguous block starting at 0 and
 are written out as a new database file that preserves all of their
 inter-relationships.  Some objects may become "orphaned" as a result
 of having their home or location disappear; these objects are moved to
 location #0.  With some combinations of command line arguments it is
 possible to include an object without including its owner; in this
 case player #1 "inherits" the object.  Exits are only extracted if
 both their source and destination rooms are extracted.

 extract 2 < source-file > output-file

 Extracts location #0, object #1 and anything it owns, and object #2
 and anything it owns.

 extract number2 < source-file > output-file

 Same as above, but (assuming player #2 is named number2) extracts
 players by name.

 extract 2 -5 < source-file > output-file.

 As above, but does not extract object #5 even if it is owned by #1 or
 #2.

 extract 2 5991 195 -198 3391 -12 12915 < source-file > object-file

 Extracts #0, #1, #2, #195, #3391, #5991, #12915 and anything they
 own, except #12 and #198.

The keyword "all" can be specified as an argument to include
everything by default, e.g.:

 extract all -198

 Extracts everything except object #198 and anything it owns.

 extract all -198 5

 Extracts everything except object #198 and anything it owns, but
 includes object #5 even if #198 owns it.
 
 Other keywords: players reachable norecycle a<num> b<num>.
 
 Players: specifies that all players are kept (but not necessarily
 what they own).
 
 Reachable: specifies that rooms not reachable by some combination of
 exits from room #0 are not extracted unless specifically included.
 
 Norecycle: specifies that players, objects, and things in the 
 recycling center (defined as the home of player 'Recycler') are not
 kept unless specifically included.  'Norecycle' overrides 'players',
 so

    extract players norecycle 

 would extract only those players not in the recycling center.

 a<num> (above) includes all objects with id numbers greater than
 or equal to <num>
 
 b<num> (below) includes all objects with id numbers less than
 or equal to <num>
 
 Putting it all together, here is how the Daytime Islandia DB is
 built from the previous night's database:
 
    extract norecycle reachable players b1370 \
	-274 -560 -809 1384 1403 1404 1406 1505 \
	Tinker Lynx Tygling Three Janitor Mav \
	$WINNER \
	< islandia.db.new > day.db
	
 Whichs keeps: all players not in the recycling center, all objects
 below 1370 except for 274, 560, and 809, and also includes 1403, 1404,
 1406, 1505, everything owned by Tinker, Lynx, Tygling, Three, Janitor,
 Mav, and everything owned by the lottery winner (name specified by
 Shell variable WINNER). 

dump

 Usage: dump [owner] < db-file

 With no arguments, prints out every object in db-file.  With one
 argument (a number) prints out every object with that owner.


decompress

 Usage: decompress < compressed-db-file > uncompressed-db-file

 Removes compression from a database file that has been generated by
 netmud using the -DCOMPRESS compile-time option.  


restart-cmu, restart-day, restart-night

 The shell scripts used at CMU to restart the TinyMUD
 server.  Useful for making sure that back versions of the database are
 always available.


I hope that you enjoy using TinyMUD.  

--Jim Aspnes, March 29th, 1990.		 	1.5.3
--Scott Goehring, April 3rd, 1990.		1.5.3A
--Michael Mauldin & Russ Smith, June 25, 1990.	1.5.4f
