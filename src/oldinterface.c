#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "db.h"
#include "interface.h"
#include "config.h"
#include "externs.h"

extern int	errno;
int	shutdown_flag = 0;

static const char *connect_fail = "Either that player does not exist, or has a different password.\n";
#ifndef REGISTRATION
static const char *create_fail = "Either there is already a player with that name, or that name is illegal.\n";
#endif 
static const char *flushed_message = "<Output Flushed>\n";
static const char *shutdown_message = "Going down - Bye!\n";

struct text_block {
	int			nchars;
	struct text_block	*nxt;
	char			*start;
	char			*buf;
};

struct text_queue {
    struct text_block *head;
    struct text_block **tail;
};

struct descriptor_data {
        int descriptor;
	int connected;
	dbref player;
	char *output_prefix;
	char *output_suffix;
	int output_size;
	struct text_queue output;
	struct text_queue input;
	char *raw_input;
	char *raw_input_at;
	long last_time;
	long connected_at;
	int quota;
	struct sockaddr_in address;
	const char *hostname;		/* 5/18/90 - Fuzzy */
	struct descriptor_data *next;
	struct descriptor_data *prev;
} *descriptor_list = 0;

static int sock;
static int ndescriptors = 0;

void process_commands(void);
void shovechars(int port);
void shutdownsock(struct descriptor_data *d);
struct descriptor_data *initializesock(int s, struct sockaddr_in *a,
				       const char *hostname);
void make_nonblocking(int s);
void freeqs(struct descriptor_data *d);
void welcome_user(struct descriptor_data *d);
void do_motd(dbref);
void check_connect(struct descriptor_data *d, const char *msg);
void close_sockets();
const char *addrout (int);
void dump_users(struct descriptor_data *d, char *user);
void set_signals(void);
struct descriptor_data *new_connection(int sock);
void parse_connect (const char *msg, char *command, char *user, char *pass);
void set_userstring (char **userstring, const char *command);
int do_command (struct descriptor_data *d, char *command);
char *strsave (const char *s);
int make_socket(int);
int queue_string(struct descriptor_data *, const char *);
int queue_write(struct descriptor_data *, const char *, int);
int process_output(struct descriptor_data *d);
int process_input(struct descriptor_data *d);
#ifdef CONNECT_MESSAGES
void announce_connect(dbref);
void announce_disconnect(dbref);
#endif
char *time_format_1(long);
char *time_format_2(long);

/* Signal handlers */
int bailout (int, int, struct sigcontext *);
int sigshutdown (int, int, struct sigcontext *);
#ifdef DETACH
int logsynch (int, int, struct sigcontext *);
#endif

char *logfile = LOG_FILE;

#define MALLOC(result, type, number) do {			\
	if (!((result) = (type *) malloc ((number) * sizeof (type))))	\
		panic("Out of memory");				\
	} while (0)

#define FREE(x) (free((void *) x))

#ifndef BOOLEXP_DEBUGGING
int main(int argc, char **argv)
{
    if (argc < 3) {
	fprintf(stderr, "Usage: %s infile dumpfile [port [logfile]]\n", *argv);
	exit (1);
    }

    if (argc > 4) logfile = argv[4];

    set_signals ();
    if (init_game (argv[1], argv[2]) < 0) {
	writelog("INIT: Couldn't load %s!\n", argv[1]);
	exit (2);
    }

    /* go do it */
    shovechars (argc >= 4 ? atoi (argv[3]) : TINYPORT);
    close_sockets ();
    dump_database ();
    exit (0);
    return(0);
}
#endif

void set_signals(void)
{
#ifdef DETACH
    int i;

    if (fork() != 0) exit(0);

    for (i=getdtablesize(); i >= 0; i--)
	(void) close(i);

    i = open("/dev/tty", O_RDWR, 0);
    if (i != -1) {
	ioctl(i, TIOCNOTTY, 0);
	close(i);
    }

    freopen(logfile, "a", stderr);
    setbuf(stderr, NULL);
#endif    
    
    /* we don't care about SIGPIPE, we notice it in select() and write() */
    signal (SIGPIPE, SIG_IGN);

    /* standard termination signals */
    signal (SIGINT,  (void (*)) sigshutdown);
    signal (SIGTERM, (void (*)) sigshutdown);

#ifdef DETACH
    /* SIGUSR2 synchronizes the log file */
    signal (SIGUSR2, (void (*)) logsynch);
#else 
    signal (SIGUSR2, (void (*)) bailout);
#endif

    /* catch these because we might as well */
    signal (SIGQUIT, (void (*)) bailout);
    signal (SIGILL, (void (*)) bailout);
    signal (SIGTRAP, (void (*)) bailout);
    signal (SIGIOT, (void (*)) bailout);
/*    signal (SIGEMT, (void (*)) bailout); */
    signal (SIGFPE, (void (*)) bailout);
    signal (SIGBUS, (void (*)) bailout);
    signal (SIGSEGV, (void (*)) bailout);
    signal (SIGSYS, (void (*)) bailout);
    signal (SIGXCPU, (void (*)) bailout);
    signal (SIGXFSZ, (void (*)) bailout);
    signal (SIGVTALRM, (void (*)) bailout);
    signal (SIGUSR1, (void (*)) bailout);
}

int notify(dbref player, const char *msg)
{
    struct descriptor_data *d;
    int retval = 0;
#ifdef COMPRESS
    extern const char *uncompress(const char *);

    msg = uncompress(msg);
#endif

    for(d = descriptor_list; d; d = d->next) {
	if (d->connected && d->player == player) {
	    queue_string(d, msg);
	    queue_write(d, "\n", 1);	/* Fuzzy: why make two packets? */
	    retval = 1;
	}
    }
    return(retval);
}

struct timeval timeval_sub(struct timeval now, struct timeval then)
{
    now.tv_sec -= then.tv_sec;
    now.tv_usec -= then.tv_usec;
    if (now.tv_usec < 0) {
	now.tv_usec += 1000000;
	now.tv_sec--;
    }
    return now;
}

int msec_diff(struct timeval now, struct timeval then)
{
    return ((now.tv_sec - then.tv_sec) * 1000
	    + (now.tv_usec - then.tv_usec) / 1000);
}

struct timeval msec_add(struct timeval t, int x)
{
    t.tv_sec += x / 1000;
    t.tv_usec += (x % 1000) * 1000;
    if (t.tv_usec >= 1000000) {
	t.tv_sec += t.tv_usec / 1000000;
	t.tv_usec = t.tv_usec % 1000000;
    }
    return t;
}

struct timeval update_quotas(struct timeval last, struct timeval current)
{
    int nslices;
    struct descriptor_data *d;

    nslices = msec_diff (current, last) / COMMAND_TIME_MSEC;

    if (nslices > 0) {
	for (d = descriptor_list; d; d = d -> next) {
	    d -> quota += COMMANDS_PER_TIME * nslices;
	    if (d -> quota > COMMAND_BURST_SIZE)
		d -> quota = COMMAND_BURST_SIZE;
	}
    }
    return msec_add (last, nslices * COMMAND_TIME_MSEC);
}

void shovechars(int port)
{
    fd_set input_set, output_set;
    long now;
    struct timeval last_slice, current_time;
    struct timeval next_slice;
    struct timeval timeout, slice_timeout;
    int maxd;
    struct descriptor_data *d, *dnext;
    struct descriptor_data *newd;
    int avail_descriptors;

    sock = make_socket (port);
    maxd = sock+1;
    gettimeofday(&last_slice, (struct timezone *) 0);

    avail_descriptors = getdtablesize() - 4;
    
    while (shutdown_flag == 0) {
	gettimeofday(&current_time, (struct timezone *) 0);
	last_slice = update_quotas (last_slice, current_time);

	process_commands();

	if (shutdown_flag)
	    break;
	timeout.tv_sec = 1000;
	timeout.tv_usec = 0;
	next_slice = msec_add (last_slice, COMMAND_TIME_MSEC);
	slice_timeout = timeval_sub (next_slice, current_time);
	
	FD_ZERO (&input_set);
	FD_ZERO (&output_set);
	if (ndescriptors < avail_descriptors)
	    FD_SET (sock, &input_set);
	for (d = descriptor_list; d; d=d->next) {
	    if (d->input.head)
		timeout = slice_timeout;
	    else
		FD_SET (d->descriptor, &input_set);
	    if (d->output.head)
		FD_SET (d->descriptor, &output_set);
	}

	if (select (maxd, &input_set, &output_set,
		    (fd_set *) 0, &timeout) < 0) {
	    if (errno != EINTR) {
		perror ("select");
		return;
	    }
	} else {
	    (void) time (&now);
	    if (FD_ISSET (sock, &input_set)) {
		if (!(newd = new_connection (sock))) {
		    if (errno
			&& errno != EINTR
			&& errno != EMFILE
			&& errno != ENFILE) {
			perror ("new_connection");
			return;
		    }
		} else {
		if (newd->descriptor >= maxd)
		    maxd = newd->descriptor + 1;
		}
	    }
	    for (d = descriptor_list; d; d = dnext) {
		dnext = d->next;
		if (FD_ISSET (d->descriptor, &input_set)) {
			d->last_time = now;
			if (!process_input (d)) {
			    shutdownsock (d);
			    continue;
			}
		}
		if (FD_ISSET (d->descriptor, &output_set)) {
		    if (!process_output (d)) {
			shutdownsock (d);
		    }
		}
	    }
	}
    }
}

static char hostname[128];

struct descriptor_data *new_connection(int sock)
{
    int newsock;
    struct sockaddr_in addr;
    unsigned int addr_len;

    addr_len = sizeof (addr);
    newsock = accept (sock, (struct sockaddr *) & addr, &addr_len);
    if (newsock < 0) {
	return 0;
#ifdef LOCKOUT
    } else if(forbidden_site(ntohl(addr.sin_addr.s_addr))) {
	writelog("REFUSED CONNECTION from %s(%d) on descriptor %d\n",
		addrout(addr.sin_addr.s_addr),
		ntohs(addr.sin_port), newsock);
	shutdown(newsock, 2);
	close(newsock);
	errno = 0;
	return 0;
#endif
    } else {
	strcpy (hostname, addrout (addr.sin_addr.s_addr));
#ifdef NOISY_LOG
	writelog("ACCEPT from %s(%d) on descriptor %d\n",
		 hostname,
		 ntohs (addr.sin_port), newsock);
#endif
	return initializesock (newsock, &addr, hostname);
    }
}

const char *addrout(int a)
{
    /* New version: returns host names, not octets.  Uses gethostbyaddr. */
    extern char *inet_ntoa(long);
    
#ifdef HOST_NAME
    struct hostent *he = gethostbyaddr((char *)&a,sizeof(a),AF_INET);
    if (he) return he->h_name;
    else return inet_ntoa(a);
#else
    return inet_ntoa(a);
#endif
}


void clearstrings(struct descriptor_data *d)
{
    if (d->output_prefix) {
	FREE(d->output_prefix);
	d->output_prefix = 0;
    }
    if (d->output_suffix) {
	FREE(d->output_suffix);
	d->output_suffix = 0;
    }
}

void shutdownsock(struct descriptor_data *d)
{
    if (d->connected) {
	writelog("DISCONNECT player %s(%d) %d %s\n",
		db[d->player].name, d->player, d->descriptor, d->hostname);
#ifdef CONNECT_MESSAGES
	announce_disconnect(d->player);
#endif
    } else {
	writelog("DISCONNECT descriptor %d never connected\n",
		d->descriptor);
    }
    clearstrings (d);
    shutdown (d->descriptor, 2);
    close (d->descriptor);
    freeqs (d);
    if (d->prev) d->prev->next = d->next; else descriptor_list = d->next;
    if (d->next) d->next->prev = d->prev;
    FREE (d);
    ndescriptors--;
}

struct descriptor_data *initializesock(int s, struct sockaddr_in *a,
				       const char *hostname)
{
    struct descriptor_data *d;

    ndescriptors++;
    MALLOC(d, struct descriptor_data, 1);
    d->descriptor = s;
    d->connected = 0;
    make_nonblocking (s);
    d->output_prefix = 0;
    d->output_suffix = 0;
    d->output_size = 0;
    d->output.head = 0;
    d->output.tail = &d->output.head;
    d->input.head = 0;
    d->input.tail = &d->input.head;
    d->raw_input = 0;
    d->raw_input_at = 0;
    d->quota = COMMAND_BURST_SIZE;
    d->last_time = 0;
    d->address = *a;			/* added 5/3/90 SCG */
    d->hostname = alloc_string(hostname);
    if (descriptor_list)
        descriptor_list->prev = d;
    d->next = descriptor_list;
    d->prev = NULL;
    descriptor_list = d;
    
    welcome_user (d);
    return d;
}

int make_socket(int port)
{
    int s;
    struct sockaddr_in server;
    int opt;

    s = socket (AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	perror ("creating stream socket");
	exit (3);
    }
    opt = 1;
    if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &opt, sizeof (opt)) < 0) {
	perror ("setsockopt");
	exit (1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons (port);
    if (bind (s, (struct sockaddr *) & server, sizeof (server))) {
	perror ("binding stream socket");
	close (s);
	exit (4);
    }
    listen (s, 5);
    return s;
}

struct text_block *make_text_block(const char *s, int n)
{
	struct text_block *p;

	MALLOC(p, struct text_block, 1);
	MALLOC(p->buf, char, n);
	bcopy (s, p->buf, n);
	p->nchars = n;
	p->start = p->buf;
	p->nxt = 0;
	return p;
}

void free_text_block (struct text_block *t)
{
	FREE (t->buf);
	FREE ((char *) t);
}

void add_to_queue(struct text_queue *q, const char *b, int n)
{
    struct text_block *p;

    if (n == 0) return;

    p = make_text_block (b, n);
    p->nxt = 0;
    *q->tail = p;
    q->tail = &p->nxt;
}

int flush_queue(struct text_queue *q, int n)
{
        struct text_block *p;
	int really_flushed = 0;
	
	n += strlen(flushed_message);

	while (n > 0 && (p = q->head)) {
	    n -= p->nchars;
	    really_flushed += p->nchars;
	    q->head = p->nxt;
	    free_text_block (p);
	}
	p = make_text_block(flushed_message, strlen(flushed_message));
	p->nxt = q->head;
	q->head = p;
	if (!p->nxt)
	    q->tail = &p->nxt;
	really_flushed -= p->nchars;
	return really_flushed;
}

int queue_write(struct descriptor_data *d, const char *b, int n)
{
    int space;

    space = MAX_OUTPUT - d->output_size - n;
    if (space < 0)
        d->output_size -= flush_queue(&d->output, -space);
    add_to_queue (&d->output, b, n);
    d->output_size += n;
    return n;
}

int queue_string(struct descriptor_data *d, const char *s)
{
    return queue_write (d, s, strlen (s));
}

int process_output(struct descriptor_data *d)
{
    struct text_block **qp, *cur;
    int cnt;

    for (qp = &d->output.head; cur = *qp;) {
	cnt = write (d->descriptor, cur -> start, cur -> nchars);
	if (cnt < 0) {
	    if (errno == EWOULDBLOCK)
		return 1;
	    return 0;
	}
	d->output_size -= cnt;
	if (cnt == cur -> nchars) {
	    if (!cur -> nxt)
		d->output.tail = qp;
	    *qp = cur -> nxt;
	    free_text_block (cur);
	    continue;		/* do not adv ptr */
	}
	cur -> nchars -= cnt;
	cur -> start += cnt;
	break;
    }
    return 1;
}

void make_nonblocking(int s)
{
    if (fcntl (s, F_SETFL, FNDELAY) == -1) {
	perror ("make_nonblocking: fcntl");
	panic ("FNDELAY fcntl failed");
    }
}

void freeqs(struct descriptor_data *d)
{
    struct text_block *cur, *next;

    cur = d->output.head;
    while (cur) {
	next = cur -> nxt;
	free_text_block (cur);
	cur = next;
    }
    d->output.head = 0;
    d->output.tail = &d->output.head;

    cur = d->input.head;
    while (cur) {
	next = cur -> nxt;
	free_text_block (cur);
	cur = next;
    }
    d->input.head = 0;
    d->input.tail = &d->input.head;

    if (d->raw_input)
        FREE (d->raw_input);
    d->raw_input = 0;
    d->raw_input_at = 0;
}

void welcome_user(struct descriptor_data *d)
{ 
    queue_string (d, WELCOME_MESSAGE);
# ifdef CONNECT_FILE
    do_connect_msg(d, CONNECT_FILE);
# endif
}

void goodbye_user(struct descriptor_data *d)
{
    write (d->descriptor, LEAVE_MESSAGE, strlen (LEAVE_MESSAGE));
}

char *strsave (const char *s)
{
    char *p;

    MALLOC (p, char, strlen(s) + 1);

    if (p)
	strcpy (p, s);
    return p;
}

void save_command (struct descriptor_data *d, const char *command)
{
    add_to_queue (&d->input, command, strlen(command)+1);
}

int process_input (struct descriptor_data *d)
{
    char buf[1024];
    int got;
    char *p, *pend, *q, *qend;

    got = read (d->descriptor, buf, sizeof buf);
    if (got <= 0)
	return 0;
    if (!d->raw_input) {
	MALLOC(d->raw_input,char,MAX_COMMAND_LEN);
	d->raw_input_at = d->raw_input;
    }
    p = d->raw_input_at;
    pend = d->raw_input + MAX_COMMAND_LEN - 1;
    for (q=buf, qend = buf + got; q < qend; q++) {
	if (*q == '\n') {
	    *p = '\0';
	    if (p > d->raw_input)
		save_command (d, d->raw_input);
	    p = d->raw_input;
	} else if (p < pend && isascii (*q) && isprint (*q)) {
	    *p++ = *q;
	}
    }
    if(p > d->raw_input) {
	d->raw_input_at = p;
    } else {
	FREE(d->raw_input);
	d->raw_input = 0;
	d->raw_input_at = 0;
    }
    return 1;
}

void set_userstring (char **userstring, const char *command)
{
    if (*userstring) {
	FREE(*userstring);
	*userstring = 0;
    }
    while (*command && isascii (*command) && isspace (*command))
	command++;
    if (*command)
	*userstring = strsave (command);
}

void process_commands(void)
{
    int nprocessed;
    struct descriptor_data *d, *dnext;
    struct text_block *t;

    do {
	nprocessed = 0;
	for (d = descriptor_list; d; d = dnext) {
	    dnext = d->next;
	    if (d -> quota > 0 && (t = d -> input.head)) {
		d -> quota--;
		nprocessed++;
		if (!do_command (d, t -> start)) {
		    shutdownsock (d);
		} else {
		    d -> input.head = t -> nxt;
		    if (!d -> input.head)
			d -> input.tail = &d -> input.head;
		    free_text_block (t);
		}
	    }
	}
    } while (nprocessed > 0);
}

int do_command (struct descriptor_data *d, char *command)
{
    if (!strcmp (command, QUIT_COMMAND)) {
	goodbye_user (d);
	return 0;
    } else if (!strncmp (command, WHO_COMMAND, strlen(WHO_COMMAND))) {
	if (d->output_prefix) {
	    queue_string (d, d->output_prefix);
	    queue_write (d, "\n", 1);
	}
	dump_users (d, command + strlen(WHO_COMMAND));
	if (d->output_suffix) {
	    queue_string (d, d->output_suffix);
	    queue_write (d, "\n", 1);
	}
    } else if (d->connected &&
	       !strncmp (command, PREFIX_COMMAND, strlen (PREFIX_COMMAND))) {
#ifdef ROBOT_MODE
	if (!Robot(d->player)) { 
	    notify(d->player,
		   "Only zombies can use OUTPUTPREFIX; contact a Wizard.");
	    return 1;
	}
	if (!d->connected) return 1;
#endif
	set_userstring (&d->output_prefix, command+strlen(PREFIX_COMMAND));
    } else if (d->connected &&
	       !strncmp (command, SUFFIX_COMMAND, strlen (SUFFIX_COMMAND))) {
#ifdef ROBOT_MODE
	if (!Robot(d->player)) { 
	    notify(d->player,
		   "Only zombies can use OUTPUTSUFFIX; contact a Wizard.");
	    return 1;
	}
#endif
	set_userstring (&d->output_suffix, command+strlen(SUFFIX_COMMAND));
    } else {
	if (d->connected) {
	    if (d->output_prefix) {
		queue_string (d, d->output_prefix);
		queue_write (d, "\n", 1);
	    }
	    process_command (d->player, command);
	    if (d->output_suffix) {
		queue_string (d, d->output_suffix);
		queue_write (d, "\n", 1);
	    }
	} else {
	    check_connect (d, command);
	}
    }
    return 1;
}

void check_connect (struct descriptor_data *d, const char *msg)
{
    char command[MAX_COMMAND_LEN];
    char user[MAX_COMMAND_LEN];
    char password[MAX_COMMAND_LEN];
    dbref player;

    parse_connect (msg, command, user, password);

    if (!strncmp (command, "co", 2)) {
	player = connect_player (user, password);
	if (player == NOTHING) {
	    queue_string (d, connect_fail);
	    writelog("FAILED CONNECT %s on %d %s\n",
		     user, d->descriptor, d->hostname);
	} else {
	    writelog("CONNECTED %s(%d) on %d %s\n",
		     db[player].name, player, d->descriptor, d->hostname);
	    d->connected = 1;
	    d->connected_at = time(NULL);
	    d->player = player;

	    do_motd (player);
	    do_look_around (player);
#ifdef CONNECT_MESSAGES
	    announce_connect(player);
#endif
	}
    } else if (!strncmp (command, "cr", 2)) {
#ifndef REGISTRATION	
	player = create_player (user, password);
	if (player == NOTHING) {
	    queue_string (d, create_fail);
	    writelog("FAILED CREATE %s on %d %s\n",
		     user, d->descriptor, d->hostname);
	} else {
	    writelog("CREATED %s(%d) on descriptor %d %s\n",
		     db[player].name, player, d->descriptor, d->hostname);
	    d->connected = 1;
	    d->connected_at = time(NULL);
	    d->player = player;

	    do_motd (player);
	    do_look_around (player);
#ifdef CONNECT_MESSAGES
	    announce_connect(player);
#endif
	}
#else
	queue_string (d, REGISTER_MESSAGE);
#endif
    } else {
	welcome_user (d);
    }
}

void parse_connect (const char *msg, char *command, char *user, char *pass)
{
    char *p;

    while (*msg && isascii(*msg) && isspace (*msg))
	msg++;
    p = command;
    while (*msg && isascii(*msg) && !isspace (*msg))
	*p++ = *msg++;
    *p = '\0';
    while (*msg && isascii(*msg) && isspace (*msg))
	msg++;
    p = user;
    while (*msg && isascii(*msg) && !isspace (*msg))
	*p++ = *msg++;
    *p = '\0';
    while (*msg && isascii(*msg) && isspace (*msg))
	msg++;
    p = pass;
    while (*msg && isascii(*msg) && !isspace (*msg))
	*p++ = *msg++;
    *p = '\0';
}

void close_sockets(void)
{
    struct descriptor_data *d, *dnext;

    for (d = descriptor_list; d; d = dnext) {
	dnext = d->next;
	write (d->descriptor, shutdown_message, strlen (shutdown_message));
	if (shutdown (d->descriptor, 2) < 0)
	    perror ("shutdown");
	close (d->descriptor);
    }
    close (sock);
}

void emergency_shutdown(void)
{
	close_sockets();
}

void boot_off(dbref player)
{
    struct descriptor_data *d, *dnext;
    for (d = descriptor_list; d; d = dnext) {
      dnext = d->next;
      if (d->connected && d->player == player) {
          process_output(d);
	  shutdownsock(d);
      }
    }
}

int bailout (int sig, int code, struct sigcontext *scp)
{
    long *ptr;
    int i;
    
    writelog("BAILOUT: caught signal %d code %d\n", sig, code);
    ptr = (long *) scp;
    for (i=0; i<sizeof(struct sigcontext); i++)
	writelog("  %08lx\n", *ptr);
    panic("PANIC on spurious signal");
    exit(7);
    return 0;
}

int sigshutdown (int sig, int code, struct sigcontext *scp)
{
    writelog("SHUTDOWN: on signal %d code %d\n", sig, code);
    shutdown_flag = 1;
    return 0;
}

#ifdef DETACH
int logsynch (int sig, int code, struct sigcontext *scp)
{
    freopen(logfile, "a", stderr);
    setbuf(stderr, NULL);
    writelog("log file reopened\n");
    return 0;
}
#endif

void dump_users(struct descriptor_data *e, char *user)
{
    struct descriptor_data *d;
    long now;
    char buf[1024], flagbuf[16];
    int wizard, god;
    int counter=0;
    int reversed, tabular;

    while (*user && isspace(*user)) user++;
    if (!*user) user = NULL;

    reversed = e->connected && Flag(e->player,REVERSED_WHO);
    tabular = e->connected && Flag(e->player,TABULAR_WHO);

    (void) time (&now);
    queue_string(e,
		 tabular ? "Player Name          On For Idle\n" : "Current Players:\n");

#ifdef GOD_MODE
  god = wizard = e->connected && God(e->player);
#else
  god = e->connected && God(e->player);
  wizard = e->connected && Wizard(e->player);
#endif

    d = descriptor_list;
    
    if (reversed)
	while (d && d->next) d = d->next;

    while (d) {
	if (d->connected &&
	    ++counter && /* Count everyone connected */
	    (!user || string_prefix(db[d->player].name, user))) {
	    if (god)
	    {
#ifdef ROBOT_MODE
	      sprintf (flagbuf, "%c%c%c  ",
		       Flag(d->player, WIZARD) ? WIZARD_MARK : ' ',
		       Flag(d->player, ROBOT)  ? ROBOT_MARK  : ' ',
		       Flag(d->player, DARK)   ? DARK_MARK   : ' ');
#else
	      sprintf (flagbuf, "%c%c  ",
		       Flag(d->player, WIZARD) ? WIZARD_MARK : ' ',
		       Flag(d->player, DARK)   ? DARK_MARK   : ' ');
#endif
	    }
	    else
	    { sprintf (flagbuf, ""); }

	    if (tabular) {
		sprintf(buf,"%-16s %10s %4s",
			db[d->player].name,
			time_format_1(now - d->connected_at),
			time_format_2(now - d->last_time));
		if (wizard) 
		    sprintf(buf+strlen(buf),
			    "  %s%s", flagbuf, d->hostname);
	    } else {
		sprintf(buf,
			"%s idle %d seconds",
			db[d->player].name,
			now - d->last_time);
		if (wizard) 
		    sprintf(buf+strlen(buf),
			    "  %sfrom host %s", flagbuf, d->hostname);
	    }
	    strcat(buf,"\n");
	    queue_string (e, buf);
	}
	if (reversed) d = d->prev; else d = d->next;
    }

    sprintf(buf, "%d user%s connected\n", counter,
	    counter == 1 ? " is" : "s are");
    queue_string(e, buf);
}

char *time_format_1(long dt)
{
    register struct tm *delta;
    static char buf[64];
    
    delta = gmtime(&dt);
    if (delta->tm_yday > 0)
	sprintf(buf, "%dd %02d:%02d",
		delta->tm_yday, delta->tm_hour, delta->tm_min);
    else
	sprintf(buf, "%02d:%02d",
		delta->tm_hour, delta->tm_min);
    return buf;
}

char *time_format_2(long dt)
{
    register struct tm *delta;
    static char buf[64];
    
    delta = gmtime(&dt);
    if (delta->tm_yday > 0)
	sprintf(buf, "%dd", delta->tm_yday);
    else if (delta->tm_hour > 0)
	sprintf(buf, "%dh", delta->tm_hour);
    else if (delta->tm_min > 0)
	sprintf(buf, "%dm", delta->tm_min);
    else
	sprintf(buf, "%ds", delta->tm_sec);
    return buf;
}

#ifdef CONNECT_MESSAGES
void announce_connect(dbref player)
{
    dbref loc;
    char buf[BUFFER_LEN];

    if ((loc = getloc(player)) == NOTHING) return;
    if (Dark(player) || Dark(loc)) return;

    sprintf(buf, "%s has connected.", db[player].name);

    notify_except(db[loc].contents, player, buf);
}

void announce_disconnect(dbref player)
{
    dbref loc;
    char buf[BUFFER_LEN];

    if ((loc = getloc(player)) == NOTHING) return;
    if (Dark(player) || Dark(loc)) return;

    sprintf(buf, "%s has disconnected.", db[player].name);

    notify_except(db[loc].contents, player, buf);
}
#endif
int do_connect_msg(struct descriptor_data * d, const char *filename)
{
  FILE           *f;
  char            buf[BUFFER_LEN];
  char           *p;

  if ((f = fopen(filename, "r")) == NULL)
  {
    return (0);
  } else
  {
    while (fgets(buf, sizeof buf, f))
    {
      queue_string(d, (char *)buf);

    }
    fclose(f);
    return (1);
  }
}

void do_tune (const char *varname, const char *valstr)
{
}
