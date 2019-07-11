/* Concentrator upgraded June 1990 Robert Hood */

/* modifed Concentrator to match Fuzzy & Randoms 1.5.4 changes = 6/90 Fuzzy */

/* modified interface.c to support LOTS of people, using a concentrator */
/* May 1990, Robert Hood */

/* #define CHECKC	/* consistency checking */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
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
#include <unistd.h>

#include "config.h"
#include "db.h"
#include "interface.h"

#define BUFSIZE 0xFFFF

struct text_block
{
  int             nchars;
  struct text_block *nxt;
  char           *start;
  char           *buf;
};

struct text_queue
{
  struct text_block *head;
  struct text_block **tail;
};

struct descriptor_data
{
  int             descriptor;
  int             num;
  int             connected;
  dbref           player;
  char           *output_prefix;
  char           *output_suffix;
  int             output_size;
  struct text_queue output;
  struct text_queue input;
  char           *raw_input;
  char           *raw_input_at;
  long            connected_at;
  long            last_time;
  int             quota;
  struct sockaddr_in address;	       /* added 3/6/90 SCG */
  char           *hostname;	       /* 5/18/90 - Fuzzy */
  struct descriptor_data *next;
};

#define MALLOC(result, type, number) do {			\
	if (!((result) = (type *) malloc ((number) * sizeof (type))))	\
		panic("Out of memory");				\
	} while (0)

#define FREE(x) (free((void *) x))

struct message
{
  char           *data;
  short           len;
  struct message *next;
};

struct conc_list
{
  struct conc_list *next;
  int             sock, current, status;
  struct descriptor_data *firstd;
  struct message *first, *last;
  char           *incoming, *outgoing;
  int             ilen, olen;
}              *firstc = 0;

void            queue_message(struct conc_list * c, char *data, int len);
void            start_log();
struct timeval  timeval_sub(struct timeval now, struct timeval then);
struct timeval  msec_add(struct timeval t, int x);
struct timeval  update_quotas(struct timeval last, struct timeval current);
void            main_loop();
int             notify(dbref player, const char *msg);
void            process_output(struct conc_list * c);
int             process_input(struct descriptor_data * d, char *buf, int got);
void            process_commands();
void            dump_users(struct descriptor_data * e, char *user);
void            free_text_block(struct text_block * t);
int             main(int argc, char **argv);
void            set_signals();
int             msec_diff(struct timeval now, struct timeval then);
void            clearstrings(struct descriptor_data * d);
void            shutdownsock(struct descriptor_data * d);
struct descriptor_data *initializesock(struct sockaddr_in * a);
struct text_block *make_text_block(const char *s, int n);
void            add_to_queue(struct text_queue * q, const char *b, int n);
int             flush_queue(struct text_queue * q, int n);
int             queue_write(struct descriptor_data * d, const char *b, int n);
int             queue_string(struct descriptor_data * d, const char *s);
void            freeqs(struct descriptor_data * d);
void            welcome_user(struct descriptor_data * d);
void            do_motd(dbref);
char           *strsave(const char *s);
void            save_command(struct descriptor_data * d, const char *command);
void            set_userstring(char **userstring, const char *command);
int             do_command(struct descriptor_data * d, char *command);
void            check_connect(struct descriptor_data * d, const char *msg);
void            parse_connect(const char *msg, char *command, char *user, char *pass);
void            close_sockets();
void            emergency_shutdown(void);
void            boot_off(dbref player);
int             bailout(int sig, int code, struct sigcontext * scp);
char           *time_format_1(long dt);
char           *time_format_2(long dt);
#ifdef CONNECT_MESSAGES
void            announce_connect(dbref);
void            announce_disconnect(dbref);
#endif
int             sigshutdown(int, int, struct sigcontext *);

int             logsynch();
char           *logfile = LOG_FILE;

int             debug;
void            chg_userid();
/* void            file_date(struct descriptor_data * d, const char *file); */

static const char *connect_fail = "Either that player does not exist, or has a different password.\n";
#ifndef REGISTRATION
static const char *create_fail = "Either there is already a player with that name, or that name is illegal.\n";
#endif
static const char *flushed_message = "<Output Flushed>\n";
static const char *shutdown_message = "Going down - Bye\n";

int             sock;
int             shutdown_flag = 0;

int             port = TINYPORT;
int             intport = INTERNAL_PORT;

/* Tunable Timing Parameters for process_commands() and I/O */
long		slice_msecs = COMMAND_TIME_MSEC;
long		burst_quota = COMMAND_BURST_SIZE;
long		timeout_msecs = 100;
long		poll_msecs = 10;
long		allow_extra = COMMAND_ALLOW_EXTRA;

/* Number of players with commands waiting */
static int	waiting_cmds = 0;

start_port()
{
  int             temp;
  struct sockaddr_in sin;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 1)
  {
    perror("socket");
    exit(-1);
  }
  temp = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&temp, sizeof(temp));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(intport);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  temp = bind(sock, (struct sockaddr *) & sin, sizeof(sin));
  if (temp < 0)
  {
    perror("bind");
    exit(-1);
  }
  temp = listen(sock, 5);
  if (temp < 0)
  {
    perror("listen");
    exit(-1);
  }
}

struct timeval
                timeval_sub(struct timeval now, struct timeval then)
{
                  now.tv_sec -= then.tv_sec;
  now.tv_usec -= then.tv_usec;
  if (now.tv_usec < 0)
  {
    now.tv_usec += 1000000;
    now.tv_sec--;
  }
  return now;
}

struct timeval
                msec_add(struct timeval t, int x)
{
                  t.tv_sec += x / 1000;
  t.tv_usec += (x % 1000) * 1000;
  if (t.tv_usec >= 1000000)
  {
    t.tv_sec += t.tv_usec / 1000000;
    t.tv_usec = t.tv_usec % 1000000;
  }
  return t;
}

struct timeval
                update_quotas(struct timeval last, struct timeval current)
{
  int             nslices;
  struct descriptor_data *d;
  struct conc_list *c;

  nslices = msec_diff(current, last) / slice_msecs;

  if (nslices > 0)
  {
    for (c = firstc; c; c = c->next)
      for (d = c->firstd; d; d = d->next)
      {
	d->quota += COMMANDS_PER_TIME * nslices;
	if (d->quota > burst_quota)
	  d->quota = burst_quota;
      }
  }
  return msec_add(last, nslices * slice_msecs);
}

int
                notify(dbref player2, const char *msg)
{
  struct descriptor_data *d;
  struct conc_list *c;
  int             retval = 0;

#ifdef COMPRESS
  extern const char *uncompress(const char *);

  msg = uncompress(msg);
#endif
  for (c = firstc; c; c = c->next)
    for (d = c->firstd; d; d = d->next)
    {
      if (d->connected && d->player == player2)
      {
	queue_string_nl(d, msg);
	retval++;
      }
    }
  return (retval);
}

int
                process_input(d, buf, got)
  struct descriptor_data *d;
  char           *buf;
  int             got;
{
  char           *p, *pend, *q, *qend;

  d->last_time = time(0);
  if (!d->raw_input)
  {
    MALLOC(d->raw_input, char, MAX_COMMAND_LEN);
    d->raw_input_at = d->raw_input;
  }
  p = d->raw_input_at;
  pend = d->raw_input + MAX_COMMAND_LEN - 1;
  for (q = buf, qend = buf + got; q < qend; q++)
  {
    if (*q == '\n')
    {
      *p = '\0';
      if (p > d->raw_input)
	save_command(d, d->raw_input);
      p = d->raw_input;
    } else
    if (p < pend && isascii(*q) && isprint(*q))
    {
      *p++ = *q;
    }
  }
  if (p > d->raw_input)
  {
    d->raw_input_at = p;
  } else
  {
    FREE(d->raw_input);
    d->raw_input = 0;
    d->raw_input_at = 0;
  }
  return 1;
}

void            process_commands()
{
  struct descriptor_data *d, *dnext, *dlast;
  struct conc_list *c;
  struct text_block *t;
  char            header[4];
  int overquota, underquota, allow_over=0;

  /* Check if there is a player with a command under quota */
  if (allow_extra)
  { for (c = firstc, allow_over=1; c && allow_over; c = c->next)
    { for (d = c->firstd; d; d = d->next)
      { if (d->quota > 0 && d->input.head ||
	    d->output.head)
        { allow_over = 0; break; }
      }
    }
  }

  /* Process commands while there are people under quota or time left */
  do
  { underquota = overquota = 0;		/* Initialize counts */

    for (c = firstc; c; c = c->next)
    { dlast = 0;

      for (d = c->firstd; d; d = dnext)
      { dnext = d->next;
        if (t = d->input.head)
	{ if (d->quota > 0 || allow_over)
	  { if (d->quota > 0 && --d->quota > 0) underquota++;
  
	    if (!do_command(d, t->start))	/* 0 => QUIT */
	    { header[0] = 0;
	      header[1] = 2;
	      header[2] = d->num;
	      queue_message(c, header, 3);
  
	      if (dlast)	dlast->next = dnext;
	      else		c->firstd = dnext;
	      shutdownsock(d);
	      FREE(d);
	      break;
	    }
	    else
	    { d->input.head = t->nxt;
	      if (!d->input.head)
		d->input.tail = &d->input.head;
	      free_text_block(t);
  	    }
	  }
	  else /* At least one player has a command in the queue over quota */
	  { overquota++; }
        }
  
	dlast = d;
      }
    }

  } while (underquota > 0);
  
  waiting_cmds = underquota + overquota;
}

void dump_users(struct descriptor_data * e, char *user)
{
  static struct conc_list *rwclist[NOFILE];
  static struct descriptor_data *rwdlist[NOFILE];
  int             ccount, dcount, dloop, cloop;

  struct descriptor_data *d;
  struct conc_list *c;
  long            now;
  int             counter = 0;
  static int	  maxcounter = 0;
  int             wizard, god, reversed, tabular;
  char            buf[1024], flagbuf[16];
  static long	  lastcounted = 0;


# ifdef DO_WHOCHECK
  writelog("WHO CHECK %d\n", sizeof(rwclist));
  writelog("WHO CHECK %d\n", sizeof(rwdlist));
# endif

  while (*user && isspace(*user))
    user++;
  if (!*user)
    user = NULL;

  reversed = e->connected && Flag(e->player, REVERSED_WHO);
  tabular = e->connected && Flag(e->player, TABULAR_WHO);

  time(&now);

  queue_string(e, tabular ? "Player Name          On For Idle\n" :
	       "Current Players:\n");
#ifdef GOD_MODE
  god = wizard = e->connected && God(e->player);
#else
  god = e->connected && God(e->player);
  wizard = e->connected && Wizard(e->player);
#endif

  if (reversed)
  {
    ccount = 0;
    for (c = firstc; c; c = c->next)
      rwclist[ccount++] = c;
    for (cloop = ccount - 1; cloop >= 0; --cloop)
    {
      dcount = 0;
      for (d = rwclist[cloop]->firstd; d; d = d->next)
	rwdlist[dcount++] = d;
      for (dloop = dcount - 1; dloop >= 0; --dloop)
      {
	d = rwdlist[dloop];
	if (d->connected &&
	    ++counter && /* Count everyone connected */
	    (!user || string_prefix(db[d->player].name, user)))
	{
	  if (tabular)
	  {
	    sprintf(buf, "%-16s %10s %4s",
		    db[d->player].name,
		    time_format_1(now - d->connected_at),
		    time_format_2(now - d->last_time));

	    if (wizard)
	      sprintf(buf, "%s  %s", buf, d->hostname);
	  } else
	  {
	    sprintf(buf, "%s idle %d seconds",
		    db[d->player].name, now - d->last_time);
	    if (wizard)
	      sprintf(buf, "%s from host %s", buf, d->hostname);
	  }
	  queue_string_nl(e, buf);
	}
      }
    }
  } else
  {
    for (c = firstc; c; c = c->next)
    {
      for (d = c->firstd; d; d = d->next)
      {
	if (d->connected &&
	    ++counter && /* Count everyone connected */
	    (!user || string_prefix(db[d->player].name, user)))
	{
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

	  if (tabular)
	  {
	    sprintf(buf, "%-16s %10s %4s",
		    db[d->player].name,
		    time_format_1(now - d->connected_at),
		    time_format_2(now - d->last_time));

	    if (wizard)
	      sprintf(buf, "%s  %s%s", buf, flagbuf, d->hostname);
	  } else
	  {
	    sprintf(buf, "%s idle %d seconds",
		    db[d->player].name, now - d->last_time);
	    if (wizard)
	      sprintf(buf, "%s %sfrom host %s", buf, flagbuf, d->hostname);
	  }
	  queue_string_nl(e, buf);
	}
      }
    }
  }
  
  if (counter > maxcounter)
  { maxcounter = counter;
    if (counter >= 60 ||
    	(time (0) - lastcounted) > 3600)
    { writelog ("%d users logged in\n", counter);
      lastcounted = time (0);
    }
  }
  
  sprintf(buf, "%d user%s connected.\n", counter,
	  counter == 1 ? " is" : "s are");
  queue_string(e, buf);
}

void free_text_block(struct text_block * t)
{
                  FREE(t->buf);
  FREE((char *)t);
}

#ifndef BOOLEXP_DEBUGGING
int main(int argc, char **argv)
{
  int             pid;

  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s infile dumpfile [port iport logfile]\n", *argv);
    exit(1);

  }
  if (argc > 3)
    port = atoi(argv[3]);
  if (argc > 4)
    intport = atoi(argv[4]);
  if (argc > 5)
    logfile = argv[5];

  start_log();

  if (init_game(argv[1], argv[2]) < 0)
  {
    writelog("INIT: Couldn't load %s\n", argv[1]);
    exit(2);
  }
  pid = vfork();
  if (pid < 0)
  {
    perror("fork");
    exit(-1);
  }
  if (pid == 0)
  {
    char            pstr[32], istr[32], clvl[32];

    /* Add port argument to concentrator */
    sprintf(pstr, "%d", port);
    sprintf(istr, "%d", intport);
    sprintf(clvl, "%d", 1);
    execl("concentrate", "conc", pstr, istr, clvl, 0);
  }
  set_signals();
  start_port(port);
  main_loop();
  close_sockets();
  dump_database();
  exit(0);
  return(0);
}

#endif

void start_log()
{
#ifdef DETACH
  if              (!debug)
  {
    int             i;

    if (fork() != 0)
      exit(0);

    i = open("/dev/tty", O_RDWR, 0);
    if (i != -1)
    {
      ioctl(i, TIOCNOTTY, 0);
      close(i);
    }
  }
  freopen(logfile, "a", stderr);
  setbuf(stderr, NULL);
#endif
}

void set_signals(void)
{
  int             dump_status(void);
  signal(SIGPIPE, SIG_IGN);

  signal(SIGINT,  (void *)sigshutdown);
  signal(SIGTERM, (void *)sigshutdown);

#ifdef DETACH
  signal(SIGUSR2, (void *)logsynch);
#else
  signal(SIGUSR2, (void *)bailout);
#endif

  if (debug)
    return;

# ifdef NOCOREDUMP
  signal(SIGQUIT, (void *)bailout);
  signal(SIGILL, (void *)bailout);
  signal(SIGTRAP, (void *)bailout);
  signal(SIGIOT, (void *)bailout);
  signal(SIGEMT, (void *)bailout);
  signal(SIGFPE, (void *)bailout);
  signal(SIGBUS, (void *)bailout);
  signal(SIGSEGV, (void *)bailout);
  signal(SIGSYS, (void *)bailout);
  signal(SIGTERM, (void *)bailout);
  signal(SIGXCPU, (void *)bailout);
  signal(SIGXFSZ, (void *)bailout);
  signal(SIGVTALRM, (void *)bailout);
# endif
}

int
                msec_diff(struct timeval now, struct timeval then)
{
                  return ((now.tv_sec - then.tv_sec) * 1000
		       +               (now.tv_usec - then.tv_usec) / 1000);
}

void
                clearstrings(struct descriptor_data * d)
{
  if              (d->output_prefix)
  {
                    FREE(d->output_prefix);
    d->output_prefix = 0;
  }
  if (d->output_suffix)
  {
    FREE(d->output_suffix);
    d->output_suffix = 0;
  }
}

void
                shutdownsock(struct descriptor_data * d)
{
  if              (d->connected)
  {
                    writelog("DISCONNECT descriptor %d,%d player %s(%d) %s\n",
			     d->descriptor, d->num,
			     db[d->player].name, d->player, d->hostname);
#ifdef CONNECT_MESSAGES
    announce_disconnect(d->player);
#endif
  } else
  {
    writelog("DISCONNECT descriptor %d,%d %s never connected\n",
	     d->descriptor, d->num, d->hostname);
  }
  clearstrings(d);
  freeqs(d);
}

struct descriptor_data *
                initializesock(struct sockaddr_in * a)
{
  struct descriptor_data *d;

  MALLOC(d, struct descriptor_data, 1);
  d->connected = 0;
  d->output_prefix = 0;
  d->output_suffix = 0;
  d->output_size = 0;
  d->output.head = 0;
  d->output.tail = &d->output.head;
  d->input.head = 0;
  d->input.tail = &d->input.head;
  d->raw_input = 0;
  d->raw_input_at = 0;
  d->quota = burst_quota;
  d->last_time = 0;
  d->address = *a;		       /* This will be the address of the
				        * concentrator */
  d->hostname = "";		       /* This will be set during connect */

  welcome_user(d);
  return d;
}

struct text_block *
                make_text_block(const char *s, int n)
{
  struct text_block *p;

  MALLOC(p, struct text_block, 1);
  MALLOC(p->buf, char, n);
  bcopy(s, p->buf, n);
  p->nchars = n;
  p->start = p->buf;
  p->nxt = 0;
  return p;
}

void
                add_to_queue(struct text_queue * q, const char *b, int n)
{
  struct text_block *p;

  if (n == 0)
    return;

  p = make_text_block(b, n);
  p->nxt = 0;
  *q->tail = p;
  q->tail = &p->nxt;
}

int
                flush_queue(struct text_queue * q, int n)
{
  struct text_block *p;
  int             really_flushed = 0;

  n += strlen(flushed_message);

  while (n > 0 && (p = q->head))
  {
    n -= p->nchars;
    really_flushed += p->nchars;
    q->head = p->nxt;
    free_text_block(p);
  }
  p = make_text_block(flushed_message, strlen(flushed_message));
  p->nxt = q->head;
  q->head = p;
  if (!p->nxt)
    q->tail = &p->nxt;
  really_flushed -= p->nchars;
  return really_flushed;
}

int
                queue_write(struct descriptor_data * d, const char *b, int n)
{
  int             space;

  space = MAX_OUTPUT - d->output_size - n;
  if (space < 0)
    d->output_size -= flush_queue(&d->output, -space);
  add_to_queue(&d->output, b, n);
  d->output_size += n;
  return n;
}

int
                queue_string(struct descriptor_data * d, const char *s)
{
                  return queue_write(d, s, strlen(s));
}

int
                queue_string_nl(struct descriptor_data * d, const char *str)
{ char buf[MAX_OUTPUT], *t;
  const char *s;
  int len = 0, retval;
  
  for (s=str, t=buf; *s && ++len < MAX_OUTPUT; ) *t++ = *s++;
  if (++len < MAX_OUTPUT)
  { *t++ = '\n';
    retval = queue_write(d, buf, len);
  }
  else
  { queue_string (d, str);
    retval = queue_write (d, "\n", 1);
  }
  
  return retval;
}

void
                freeqs(struct descriptor_data * d)
{
  struct text_block *cur, *next;

  cur = d->output.head;
  while (cur)
  {
    next = cur->nxt;
    free_text_block(cur);
    cur = next;
  }
  d->output.head = 0;
  d->output.tail = &d->output.head;

  cur = d->input.head;
  while (cur)
  {
    next = cur->nxt;
    free_text_block(cur);
    cur = next;
  }
  d->input.head = 0;
  d->input.tail = &d->input.head;

  if (d->raw_input)
    FREE(d->raw_input);
  d->raw_input = 0;
  d->raw_input_at = 0;
}

void
                welcome_user(struct descriptor_data * d)
{
  queue_string(d, WELCOME_MESSAGE);
  file_date(d, NEWS_FILE);
# ifdef CONNECT_FILE
  do_connect_msg(d, CONNECT_FILE);
# endif
}

void
                goodbye_user(struct descriptor_data * d)
{
                  queue_string(d, LEAVE_MESSAGE);
}

char           *
                strsave(const char *s)
{
  char           *p;

  MALLOC(p, char, strlen(s) + 1);

  if (p)
    strcpy(p, s);
  return p;
}

void
                save_command(struct descriptor_data * d, const char *command)
{
                  add_to_queue(&d->input, command, strlen(command) + 1);
}

void
                set_userstring(char **userstring, const char *command)
{
  if              (*userstring)
  {
                    FREE(*userstring);
    *userstring = 0;
  }
  while (*command && isascii(*command) && isspace(*command))
    command++;
  if (*command)
    *userstring = strsave(command);
}

int
                do_command(struct descriptor_data * d, char *command)
{
  if              (!strcmp(command, QUIT_COMMAND))
  {
                    goodbye_user(d);
    return 0;
  } else
  if (!strncmp(command, WHO_COMMAND, strlen(WHO_COMMAND)))
  {
    if (d->output_prefix)
    {
      queue_string_nl(d, d->output_prefix);
    }
    dump_users(d, command + strlen(WHO_COMMAND));
    if (d->output_suffix)
    {
      queue_string_nl(d, d->output_suffix);
    }
  } else if (d->connected &&
	!strncmp(command, PREFIX_COMMAND, strlen(PREFIX_COMMAND)))
  {
#ifdef ROBOT_MODE
    if (!Robot(d->player))
    {
      notify(d->player,
	     "Only zombies can use OUTPUTPREFIX; contact a Wizard.");
      return 1;
    }
    if (!d->connected)
      return 1;
#endif 
    set_userstring(&d->output_prefix, command + strlen(PREFIX_COMMAND));
  } else
    if (d->connected &&
	!strncmp(command, SUFFIX_COMMAND, strlen(SUFFIX_COMMAND)))
  {
#ifdef ROBOT_MODE
    if (!Robot(d->player))
    {
      notify(d->player,
	     "Only zombies can use OUTPUTSUFFIX; contact a Wizard.");
      return 1;
    }
#endif
    set_userstring(&d->output_suffix, command + strlen(SUFFIX_COMMAND));
  } else
  {
    if (d->connected)
    {
      if (d->output_prefix)
      {
	queue_string_nl(d, d->output_prefix);
      }
      process_command(d->player, command);
      if (d->output_suffix)
      {
	queue_string_nl(d, d->output_suffix);
      }
    } else
    {
      check_connect(d, command);
    }
  }
  return 1;
}

void
                check_connect(struct descriptor_data * d, const char *msg)
{
  char            command[MAX_COMMAND_LEN];
  char            user[MAX_COMMAND_LEN];
  char            password[MAX_COMMAND_LEN];
  dbref           player;

  parse_connect(msg, command, user, password);

  if (!strncmp(command, "co", 2))
  {
    player = connect_player(user, password);
    if (player == NOTHING)
    {
      queue_string(d, connect_fail);
      writelog("FAILED CONNECT %s on descriptor %d,%d %s\n",
		user, d->descriptor, d->num, d->hostname);
    } else
    {
      writelog("CONNECTED %s(%d) on descriptor %d,%d %s\n",
	       db[player].name, player, d->descriptor, d->num, d->hostname);
      d->connected = 1;
      d->connected_at = time(NULL);
      d->player = player;
      do_motd(player);
      do_look_around(player);
#ifdef CONNECT_MESSAGES
      announce_connect(player);
#endif
    }
  } else
  if (!strncmp(command, "cr", 2))
  {
#ifndef REGISTRATION
    player = create_player(user, password);
    if (player == NOTHING)
    {
      queue_string(d, create_fail);
      writelog("FAILED CREATE %s on descriptor %d,%d %s\n",
	       user, d->descriptor, d->num, d->hostname);
    } else
    {
      writelog("CREATED %s(%d) on descriptor %d,%d %s\n",
	       db[player].name, player, d->descriptor, d->num, d->hostname);
      d->connected = 1;
      d->connected_at = time(0);
      d->player = player;
      do_motd(player);
      do_look_around(player);
#ifdef CONNECT_MESSAGES
      announce_connect(player);
#endif
    }
#else
    queue_string(d, REGISTER_MESSAGE);
#endif
  } else
  {
    welcome_user(d);
  }
}

void
                parse_connect(const char *msg, char *command, char *user, char *pass)
{
  char           *p;

  while (*msg && isascii(*msg) && isspace(*msg))
    msg++;
  p = command;
  while (*msg && isascii(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
  while (*msg && isascii(*msg) && isspace(*msg))
    msg++;
  p = user;
  while (*msg && isascii(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
  while (*msg && isascii(*msg) && isspace(*msg))
    msg++;
  p = pass;
  while (*msg && isascii(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
}

void
                close_sockets(void)
{
  struct descriptor_data *d, *dnext;
  struct conc_list *c;
  char            header[4];

  for (c = firstc; c; c = c->next)
  {
    /* conc.c now handles printing the Going Down - Bye message */
    shutdown(c->sock, 0);
    close(c->sock);
  }
  close(sock);
}

void
                emergency_shutdown(void)
{
                  close_sockets();
}

int
                bailout(int sig, int code, struct sigcontext * scp)
{
  long           *ptr;
  int             i;

  writelog("BAILOUT: caught signal %d code %d", sig, code);
  ptr = (long *)scp;
  for (i = 0; i < sizeof(struct sigcontext); i++)
    writelog("  %08lx\n", *ptr);
  panic("PANIC on spurious signal");
  _exit(7);
  return 0;
}

char           *
                time_format_1(long dt)
{
  register struct tm *delta;
  static char     buf[64];

  delta = gmtime(&dt);
  if (delta->tm_yday > 0)
  {
    sprintf(buf, "%dd %02d:%02d",
	    delta->tm_yday, delta->tm_hour, delta->tm_min);
  } else
  {
    sprintf(buf, "%02d:%02d",
	    delta->tm_hour, delta->tm_min);
  }
  return buf;
}

char           *
                time_format_2(long dt)
{
  register struct tm *delta;
  static char     buf[64];

  delta = gmtime(&dt);
  if (delta->tm_yday > 0)
  {
    sprintf(buf, "%dd", delta->tm_yday);
  } else
  if (delta->tm_hour > 0)
  {
    sprintf(buf, "%dh", delta->tm_hour);
  } else
  if (delta->tm_min > 0)
  {
    sprintf(buf, "%dm", delta->tm_min);
  } else
  {
    sprintf(buf, "%ds", delta->tm_sec);
  }
  return buf;
}

#ifdef CONNECT_MESSAGES
void
                announce_connect(dbref player)
{
  dbref           loc;
  char            buf[BUFFER_LEN];

  if ((loc = getloc(player)) == NOTHING)
    return;
  if (Dark(player) || Dark(loc))
    return;

  sprintf(buf, "%s has connected.", db[player].name);

  notify_except(db[loc].contents, player, buf);
}

void
                announce_disconnect(dbref player)
{
  dbref           loc;
  char            buf[BUFFER_LEN];

  if ((loc = getloc(player)) == NOTHING)
    return;
  if (Dark(player) || Dark(loc))
    return;

  sprintf(buf, "%s has disconnected.", db[player].name);

  notify_except(db[loc].contents, player, buf);
}

#endif

int
                sigshutdown(int sig, int code, struct sigcontext * scp)
{
                  writelog("SHUTDOWN: on signal %d code %d\n", sig, code);
  shutdown_flag = 1;
  return 0;
}

#ifdef DETACH
int
                logsynch()
{
                  freopen(logfile, "a", stderr);
  setbuf(stderr, NULL);
  writelog("log file reopened\n");
  return 0;
}

#endif

#include <sys/stat.h>

int
                file_date(struct descriptor_data * d, char *file)
{
  static char     buf[80];
/*  extern char    *ctime(long *clock); */
  struct stat     statb;
  char           *tstring;
  char           *cp;

  if (stat(file, &statb) == -1)
    return;

  tstring = ctime(&statb.st_mtime);
  if ((cp = (char *)index(tstring, '\n')) != NULL)
    *cp = '\0';

  strcpy(buf, "News last updated ");
  strcat(buf, tstring);
  strcat(buf, "\n\n");

  queue_string(d, (char *)buf);
}

void            main_loop()
{
  struct message *ptr;
  unsigned int    newsock, lastsock, len, loop;
  int             accepting;
  struct sockaddr_in sin;
  fd_set          in, out;
  char            data[1025], *p1, *p2, buffer[1025], header[4];
  struct conc_list *c, *tempc, *nextc, *lastc;
  struct descriptor_data *d, *tempd, *nextd;
  struct timeval  last_slice, current_time;
  struct timeval  next_slice;
  struct timeval  timeout, slice_timeout, poll_timeout;
  short           templen;

  accepting = 1;
  lastsock = sock + 1;

  while (!shutdown_flag)
  {
    gettimeofday(&current_time, (struct timezone *) 0);
    last_slice = update_quotas(last_slice, current_time);

    process_commands();

    if (shutdown_flag)
      break;
      
    /* Set timeout intervals */
    slice_timeout.tv_sec = timeout_msecs / 1000;
    slice_timeout.tv_usec = (timeout_msecs * 1000) % 1000000;

    poll_timeout.tv_sec =  poll_msecs / 1000;
    poll_timeout.tv_usec =  (poll_msecs * 1000) % 1000000;

    FD_ZERO(&in);
    FD_ZERO(&out);

    FD_SET(sock, &in);
    for (c = firstc; c; c = c->next)
    { process_output(c); }
    for (c = firstc; c; c = c->next)
    { if (c->sock)
      {
	if (c->ilen < BUFSIZE)
	  FD_SET(c->sock, &in);
	len = c->first ? c->first->len : 0;
	while (c->first && ((c->olen + len + 2) < BUFSIZE))
	{
	  templen = c->first->len;
	  bcopy(&templen, c->outgoing + c->olen, 2);
	  bcopy(c->first->data, c->outgoing + c->olen + 2, len);
	  c->olen += len + 2;
	  ptr = c->first;
	  c->first = ptr->next;
	  FREE(ptr->data);
	  FREE(ptr);
	  if (c->last == ptr)
	    c->last = 0;
	  len = c->first ? c->first->len : 0;
	}
	if (c->olen)
	  FD_SET(c->sock, &out);
      }
    }

    /* Check for changes on inputs or outputs */
    timeout = (waiting_cmds) ? poll_timeout : slice_timeout;
    
    if (select(lastsock, &in, &out, (fd_set *) 0, &timeout) < 0)
    { continue; }

    if (accepting && FD_ISSET(sock, &in))
    {
      len = sizeof(sin);
      newsock = accept(sock, (struct sockaddr *) & sin, &len);
      if (newsock >= 0)
      {
	if (newsock >= lastsock)
	  lastsock = newsock + 1;
	if (fcntl(newsock, F_SETFL, FNDELAY) == -1)
	{
	  perror("make_nonblocking: fcntl");
	}
	MALLOC(tempc, struct conc_list, 1);
	tempc->next = firstc;
	tempc->firstd = 0;
	tempc->first = 0;
	tempc->last = 0;
	tempc->status = 0;
	/* Imcomming and outgoing I/O buffers */
	MALLOC(tempc->incoming, char, BUFSIZE);
	tempc->ilen = 0;
	MALLOC(tempc->outgoing, char, BUFSIZE);
	tempc->olen = 0;
	firstc = tempc;
	firstc->sock = newsock;
	writelog("CONCENTRATOR CONNECT: sock %d, addr %x\n", newsock, sin.sin_addr.s_addr);
      }
    }
    for (c = firstc; c; c = nextc)
    {
      nextc = c->next;
#ifdef CHECKC
      if (!(c->sock))
	writelog("CONSISTENCY CHECK: Concentrator found with null socket #\n");
#endif
      if ((FD_ISSET(c->sock, &in)) && (c->ilen < BUFSIZE))
      {
	int             i;
	len = recv(c->sock, c->incoming + c->ilen,
		   BUFSIZE - c->ilen, 0);
	if (len == 0)
	{
	  struct message *mptr, *tempm;
	  writelog("CONCENTRATOR DISCONNECT: %d\n", c->sock);
	  close(c->sock);
	  d = c->firstd;
	  while (d)
	  {
	    shutdownsock(d);
	    tempd = d;
	    d = d->next;
	    FREE(tempd);
	  }
	  if (firstc == c)
	    firstc = firstc->next;
	  else
	    lastc->next = c->next;
	  FREE(c->incoming);
	  FREE(c->outgoing);
	  mptr = c->first;
	  while (mptr)
	  {
	    tempm = mptr;
	    mptr = mptr->next;
	    FREE(mptr->data);
	    FREE(mptr);
	  }
	  FREE(c);
	  break;
	} else
	if (len < 0)
	{
	  writelog("recv: %s\n", strerror(errno));
	} else
	{
	  int             num;

	  c->ilen += len;
	  while (c->ilen > 2)
	  {
	    bcopy(c->incoming, &templen, 2);
#ifdef CHECKC
	    if (templen < 1)
	      writelog("CONSISTENCY CHECK: Message received with length < 1\n");
#endif
	    if (c->ilen >= (templen + 2))
	    {
	      num = *(c->incoming + 2);
	      /* Is it coming from the command user #? */
	      if (num == 0)
	      {
		/* Proccess commands */
		switch (*(c->incoming + 3))
		{
		case 1:	       /* connect */
		  d = initializesock(&sin);
		  d->descriptor = c->sock;
		  d->next = c->firstd;
		  c->firstd = d;
		  d->num = *(c->incoming + 4);
		  MALLOC(d->hostname, char,
			 templen - 5);
		  bcopy(c->incoming + 9, d->hostname,
			templen - 6);
		  *(d->hostname + templen - 7) = 0;
#ifdef DEBUG
		  writelog("USER CONNECT %d,%d from host %s\n", c->sock, d->num, d->hostname);
#endif
		  break;
		case 2:	       /* disconnect */
		  tempd = 0;
		  d = c->firstd;
		  num = *(c->incoming + 4);
		  while (d)
		  {
		    if (d->num == num)
		    {
		      writelog("USER ABORTED CONNECTION %d,%d %s\n",
			       c->sock, d->num, d->hostname);

		      shutdownsock(d);
		      if (c->firstd == d)
			c->firstd = d->next;
		      else
			tempd->next = d->next;
		      FREE(d);
		      break;
		    }
		    tempd = d;
		    d = d->next;
		  }
#ifdef CHECKC
		  if (!d)
		    writelog("CONSISTENCY CHECK: Disconnect Received for unknown user %d,%d\n", c->sock, num);
#endif
		  break;

		  /*
		   * This take a message from a concentrator, and logs it in
		   * the log file 
		   */
		case 4:
		  {
		    char            buffer[2048];
		    bcopy(c->incoming + 4, buffer,
			  templen - 2);
		    *(buffer + templen - 1) = 0;
		    writelog(buffer);
		    break;
		  }
#ifdef CHECKC
		default:
		  writelog("CONSISTENCY CHECK: Received unknown command from concentrator\n");
#endif
		}
	      } else
	      {
		d = c->firstd;
		while (d)
		{
		  if (d->num == num)
		  {
		    process_input(d, c->incoming + 3,
				  templen - 1);
		    break;
		  }
		  d = d->next;
		}
#ifdef CHECKC
		if (!d)
		  writelog("CONSISTENCY CHECK: Message received for unknown user %d,%d\n", c->sock, num);
#endif
	      }
	      bcopy(c->incoming + templen + 2, c->incoming,
		    (c->ilen - templen - 2));
	      c->ilen = c->ilen - templen - 2;
	    } else
	      break;
	  }
	}
      }
      lastc = c;
    }
    /* Send data loop */
    for (c = firstc; c; c = c->next)
    {
      if (FD_ISSET(c->sock, &out))
      {
	if (c->olen)
	{
	  len = send(c->sock, c->outgoing, c->olen, 0);
	  if (len > 0)
	  {
	    c->olen -= len;
	    bcopy(c->outgoing + len, c->outgoing, c->olen);
	  }
	}
      }
    }
  }
}

void            process_output(struct conc_list * c)
{
  struct descriptor_data *d;
  struct text_block **qp, *cur;
  short           templen;

  for (d = c->firstd; d; d = d->next)
  {
    qp = &d->output.head;
    cur = *qp;
    if (cur)
    {
      if (cur->nchars < 512)
      {
	if ((c->olen + cur->nchars + 3) < BUFSIZE)
	{
	  templen = cur->nchars + 1;
	  bcopy(&templen, c->outgoing + c->olen, 2);
	  *(c->outgoing + c->olen + 2) = d->num;
	  strncpy(c->outgoing + c->olen + 3, cur->start, cur->nchars);
	  d->output_size -= cur->nchars;
	  c->olen += cur->nchars + 3;
	  if (!cur->nxt)
	    d->output.tail = qp;
	  *qp = cur->nxt;
	  free_text_block(cur);
	}
      } else
      {
	if ((c->olen + 512 + 3) < BUFSIZE)
	{
	  templen = 512 + 1;
	  bcopy(&templen, c->outgoing + c->olen, 2);
	  *(c->outgoing + c->olen + 2) = d->num;
	  strncpy(c->outgoing + c->olen + 3, cur->start, 512);
	  d->output_size -= 512;
	  c->olen += 512 + 3;
	  cur->nchars -= 512;
	  cur->start += 512;
	}
      }
    }
  }
}

void            boot_off(dbref player)
{
  struct conc_list *c;
  struct descriptor_data *d, *lastd;
  char            header[4];

  for (c = firstc; c; c = c->next)
  {
    lastd = 0;
    for (d = c->firstd; d; d = d->next)
    {
      if (d->connected && d->player == player)
      {
	header[0] = 0;
	header[1] = 2;
	header[2] = d->num;
	queue_message(c, header, 3);
	process_output(c);
	if (lastd)
	  lastd->next = d->next;
	else
	  c->firstd = d->next;
	shutdownsock(d);
	FREE(d);
	return;
      }
      lastd = d;
    }
  }
}

void            queue_message(struct conc_list * c, char *data, int len)
{
  struct message *ptr;

  MALLOC(ptr, struct message, 1);
  MALLOC(ptr->data, char, len);
  ptr->len = len;
  bcopy(data, ptr->data, len);
  ptr->next = 0;
  if (c->last == 0)
    c->first = ptr;
  else
    c->last->next = ptr;
  c->last = ptr;
}

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

void do_tune (dbref player, const char *varname, const char *valstr)
{
  int vnlen = strlen (varname);
  long value;
  char buf[BUFFER_LEN];

  if (!God (player))
  { notify (player, "What tune do you want to hear?");
    return;
  }
  
  if (!varname || !*varname  || !valstr || !*valstr)
  { sprintf (buf, "Current server settings:\n  slice %d ms, burst %d cmds\n  timeout %d ms, poll %d ms, extra %s",
  	     slice_msecs, burst_quota, timeout_msecs, poll_msecs,
	     allow_extra ? "yes" : "no");
    notify (player, buf);
    return;
  }

  if (!strncmp (varname, "extra", vnlen))
  { if (atoi (valstr) > 0 || *valstr == 'y' || *valstr == 'Y')
    { allow_extra = 1;
      notify (player, "Extra commands allowed when otherwise idle.");
    }
    else
    { allow_extra = 0;
      notify (player, "Extra commands disallowed (quotas strictly enforced).");
    }
    return;
  }
  
  if (!isdigit (*valstr) || (value = atoi (valstr)) <= 0)
  { notify (player,
  	    "Please enter a positive integer value for the second parameter.");
    return;
  }
  
  if (!strncmp (varname, "slice", vnlen))
  { slice_msecs = value;
    sprintf (buf, "Time slice set to %d ms.", slice_msecs);
  }
  else if (!strncmp (varname, "burst", vnlen))
  { burst_quota = value;
    sprintf (buf, "Maximum burst size set to %d cmds.", burst_quota);
  }
  else if (!strncmp (varname, "timeout", vnlen))
  { timeout_msecs = value;
    sprintf (buf, "Timeout for select call set to %d ms.", timeout_msecs);
  }
  else if (!strncmp (varname, "poll", vnlen))
  { poll_msecs = value;
    sprintf (buf, "Timeout for select poll set to %d ms.", poll_msecs);
  }
  else
  { strcpy (buf, "Variables are: slice, burst, timeout, poll, extra"); }
  
  notify (player, buf);
}
