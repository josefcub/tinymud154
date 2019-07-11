/* tinyMUD port concentrator by Robert Hood */
/* Revision 2.0 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <netdb.h>
#include "config.h"

void            queue_message(int port, char *data, int len);
void            writelog(const char *fmt,...);

#define BUFLEN 65536
#define CONC_MESSAGE "[ Connected to the TinyMUD port concentrator ]\n"
#define PANIC_MESSAGE "\nGoing Down - Bye!\n"

#define CLOSED_TIME	(8 * 3600) /* 8 hours */
#define WARN1_MSG \
"[  This concentrator will close in 1 hour, please QUIT and reconnect  ]\n"
#define WARN1_TIME	(CLOSED_TIME - 3600)  /* 60 minutes */
#define WARN2_MSG \
"[  This concentrator will close in 30 minutes, please QUIT and reconnect  ]\n"
#define WARN2_TIME	(CLOSED_TIME - 30 * 60)  /* 30 minutes */
#define WARN3_MSG \
"[  This concentrator will close in 10 minutes, please QUIT and reconnect  ]\n"
#define WARN3_TIME	(CLOSED_TIME - 10 * 60)  /* 10 minutes */
#define CLOSED_MSG \
"[  This concentrator is now closed, please reconnect  ]\n"

struct message
{
  char           *data;
  short           len;
  struct message *next;
};

struct conc_list
{
  char            status;

  /*
   * Status: 0 = Not connected 1 = Connected 2 = Disconnecting (waiting till
   * queue is empty) 
   */
  struct message *first, *last;
}              *clist;

int             mud_sock;
int             sock;
int             pid;

int             port = TINYPORT;
int             intport = INTERNAL_PORT;
int             clvl = 1;

main(argc, argv)
  int             argc;
  char           *argv[];
{
  int             l;

  if (argc > 1)
    port = atoi(argv[1]);
  if (argc > 2)
    intport = atoi(argv[2]);
  if (argc > 3)
    clvl = atoi(argv[3]);

  signal(SIGPIPE, SIG_IGN);	       /* Ignore I/O signals */
  for (l = 3; l < NOFILE; ++l)	       /* Close all files from last process */
    close(l);			       /* except stdin, stdout, stderr */
  pid = 1;
  connect_mud();		       /* Connect to interface.c */
  setup();			       /* Setup listen port */
  mainloop();			       /* main loop */
}

connect_mud()
{
  int             temp;
  struct sockaddr_in sin;

  mud_sock = 0;
  while (mud_sock == 0)
  {
    mud_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (mud_sock < 0)
    {
      perror("socket");
      mud_sock = 0;
    } else
    {
      temp = 1;
      setsockopt(mud_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&temp, sizeof(temp));
      sin.sin_family = AF_INET;
      sin.sin_port = htons(intport);
      sin.sin_addr.s_addr = htonl(0x7F000001);
      temp = connect(mud_sock, (struct sockaddr *) & sin, sizeof(sin));
      if (temp < 0)
      {
	perror("connect");
	close(mud_sock);
	mud_sock = 0;
      }
    }
    if (mud_sock == 0)
    {
      sleep(1);
      fputs("retrying....\n", stderr);
    }
  }
  if (fcntl(mud_sock, F_SETFL, FNDELAY) == -1)
  {
    perror("make_nonblocking: fcntl");
  }
  if (fcntl(mud_sock, F_SETFD, 1) == -1)
  {
    perror("close on execve: fcntl");
  }
#ifdef DEBUG
  fputs("connected!\n", stderr);
#endif
}

setup()
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
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  temp = bind(sock, (struct sockaddr *) & sin, sizeof(sin));
  if (temp < 0)
  {
    perror("bind");
    exit(1);
  }
  temp = listen(sock, 5);
  if (temp < 0)
  {
    perror("listen");
    exit(1);
  }
}

mainloop()
{
  unsigned int    found, newsock, lastsock, len, loop;
  int             accepting = 1, current = 0;
  long		  closedat = 0, now = 0, lasttime = 0;
  int		  warn = 0;
  int             temp;
  struct timeval  tv;
  struct sockaddr_in sin;
  struct hostent *hent;
  fd_set          in, out;
  char           *data;
  char           *buf, header[4];
  struct conc_list *cptr;
  struct message *tmsg;
  short           templen;
  char           *mainbuf, *outbuf;
  int             mainlen, outlen;
  int             command;
  int             hlen;
  char           *hostnm;
  char		  warnmsg[512];

  /* Allocate huge buffer */
  data = (char *)malloc(65536);
  /* Allocate array, one for each possible socket */
  clist = (struct conc_list *) malloc(sizeof(struct conc_list) * NOFILE);
  /* Allocate I/O buffers for main I/O socket */
  mainbuf = (char *)malloc(BUFLEN);
  mainlen = 0;
  outbuf = (char *)malloc(BUFLEN);
  outlen = 0;
  if (!data || !clist || !mainbuf || !outbuf)
  {
    perror("malloc");
    exit(1);
  }
  /* Init array */
  for (loop = 0; loop < NOFILE; ++loop)
  {
    cptr = &(clist[loop]);
    cptr->status = 0;
    cptr->first = 0;
    cptr->last = 0;
  }

  /*
   * Accept connections flag ON accepting = 1; /* lastsock for select() 
   */
  lastsock = sock + 1;
  /* mud_sock has already been established */
  clist[mud_sock].status = 1;
  /* Special port # for control messages */
  clist[0].status = 1;
  while (1)
  {
    if (pid < 0)
    {
      pid = vfork();
    }
    if (pid == 0)
    {
      char            pstr[32], istr[32], cstr[32];
      sprintf(pstr, "%d", port);
      sprintf(istr, "%d", intport);
      sprintf(cstr, "%d", clvl + 1);
      execlp("concentrate", "conc", pstr, istr, cstr, 0);
      writelog("CONC %d:ACK!!!!!! exec failed! Exiting...\n", clvl);
      exit(1);
      /* Gee...now what? Should I try again? */
    }
    /* zero out port selector masks */
    FD_ZERO(&in);
    FD_ZERO(&out);

#ifdef CONCTIMEOUT
    if (!accepting)
    { /* Test for time out */
      now = time (0);
      warnmsg[0] = '\0';
    
      if (now > lasttime + 60)
      { sprintf (warnmsg, "[ The time is %20.20s ]\n", ctime (&now) + 4);
        lasttime = now;
      }
      
      if (warn < 1 && now > closedat + WARN1_TIME)
      { strcpy (warnmsg, WARN1_MSG); warn = 1; }
      else if (warn < 2 && now > closedat + WARN2_TIME)
      { strcpy (warnmsg, WARN2_MSG); warn = 2; }
      else if (warn < 3 && now > closedat + WARN3_TIME)
      { strcpy (warnmsg, WARN3_MSG); warn = 3; }
      else if (warn < 4 && now > closedat + CLOSED_TIME)
      { strcpy (warnmsg, CLOSED_MSG); warn = 4; }

      /* If messages to send, send to all players */
      if (warnmsg[0])
      { for (loop = mud_sock + 1; loop < NOFILE; ++loop)
        { cptr = &(clist[loop]);
          if (cptr ->status == 1)
	  { queue_message (loop, warnmsg, strlen (warnmsg)); }
        }
      }
    }
#endif 

    /* set apropriate bit masks for I/O */
    if (accepting)
      FD_SET(sock, &in);
    for (loop = 1; loop < NOFILE; ++loop)
    {
      cptr = &(clist[loop]);
      if (cptr->status)
      {
	FD_SET(loop, &in);
	if (cptr->first)
	  FD_SET(loop, &out);
      }
    }
    if (outlen > 0)
      FD_SET(loop, &out);

    /* timeout for select */
    tv.tv_sec = 1000;
    tv.tv_usec = 0;

    /* look for ports waiting for I/O */
    found = select(lastsock, &in, &out, (fd_set *) 0, &tv);
    /* None found, skip the rest... */
    if (found < 0)
      continue;

    /* New connection? */
    if (accepting && FD_ISSET(sock, &in))
    {
      len = sizeof(sin);
      newsock = accept(sock, (struct sockaddr *) & sin, &len);
      /* This limits the # of connections per concentrator */
      if (newsock >= (NOFILE - 5))
      {
	close(sock);
	accepting = 0;
	closedat = time (0);
	pid = -1;
      }
      if (newsock >= lastsock)
	lastsock = newsock + 1;
      cptr = &(clist[newsock]);
      cptr->status = 1;
      cptr->first = 0;
      cptr->last = 0;
      /* set to non-blocking mode */
      if (fcntl(newsock, F_SETFL, FNDELAY) == -1)
      {
	perror("make_nonblocking: fcntl");
      }
      /* set to close on execv */
      if (fcntl(newsock, F_SETFD, 1) == -1)
      {
	perror("close on execv: fcntl");
      }
      temp = 1;
      if (setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, (char *)&temp,
		     sizeof(temp)) < 0)
      {
	perror("keepalive setsockopt");
      }
      queue_message(newsock, CONC_MESSAGE, sizeof(CONC_MESSAGE) - 1);
      /* build control code for connect */
      data[0] = 0;
      data[1] = 1;		       /* connect */
      data[2] = newsock;
      bcopy(&(sin.sin_addr.s_addr), data + 3, 4);
      hent = gethostbyaddr((char *)&(sin.sin_addr.s_addr),
			   sizeof(sin.sin_addr.s_addr), AF_INET);
      if (hent)
	strcpy(data + 7, hent->h_name);
      else
	strcpy(data + 7, (char *)inet_ntoa(sin.sin_addr.s_addr));
      queue_message(mud_sock, data, 7 + strlen(data + 7));
#ifdef DEBUG
      writelog("CONC %d: USER CONNECT: sock %d, host %s\n", clvl,
	       newsock, data + 7);
#endif
    }
    /* receive data from ports */
    for (loop = 0; loop < NOFILE; ++loop)
    {
      cptr = &(clist[loop]);
      if (cptr->status && FD_ISSET(loop, &in))
      {
	if (loop == 0)
	{
	} else
	if (loop == mud_sock)
	{
	  if (mainlen < BUFLEN)
	  {
	    len = recv(loop, mainbuf + mainlen,
		       BUFLEN - mainlen, 0);
	    if (len <= 0)
	    {
	      /* This is quite useless, but what else am I supposed to do? */
	      writelog("CONC %d: Lost Connection\n", clvl);
	      panic();
	    }
	    mainlen += len;
	  }
	  while (mainlen > 2)
	  {
	    bcopy(mainbuf, &templen, 2);
	    if (mainlen >= (templen + 2))
	    {
	      queue_message(*(mainbuf + 2), mainbuf + 3,
			    templen - 1);
	      mainlen = mainlen - templen - 2;
	      bcopy(mainbuf + templen + 2, mainbuf, mainlen);
	    } else
	      break;
	  }
	} else
	{
	  /* data + 1 so we can add port later w/o a bcopy */
	  len = recv(loop, data + 1, 65530, 0);
	  if (len == 0)
	  {
	    disconnect(loop);
	  } else
	  if (len < 0)
	  {
	    /* Hmm..... */
	    writelog("CONC %d: recv: %s\n", clvl, strerror(errno));
	  } else
	  {
	    /* Add the port # to the data, and send it to interface.c */
	    data[0] = loop;
	    queue_message(mud_sock, data, len + 1);
	  }
	}
      }
    }
    /* Handle output */
    for (loop = 0; loop < NOFILE; ++loop)
    {
      cptr = &(clist[loop]);
      if ((loop == 0) && (cptr->first))
      {
	command = *(cptr->first->data);
	switch (command)
	{
	case 2:		       /* disconnect */
	  if (clist[*(cptr->first->data + 1)].status)
	    clist[*(cptr->first->data + 1)].status = 2;
	  else
	    writelog("CONC %d: Received disconnect for unknown user\n", clvl);
	  break;
	default:
	  writelog("CONC %d: Received unknown command %d\n", clvl, command);
	  break;
	}
	free(cptr->first->data);
	tmsg = cptr->first;
	cptr->first = cptr->first->next;
	free(tmsg);
	if (!cptr->first)
	  cptr->last = 0;
      } else
	if ((loop == mud_sock) && FD_ISSET(mud_sock, &out) &&
	    ((cptr->first) || (outlen > 0)))
      {
	while ((cptr->first) &&
	       ((BUFLEN - outlen) > (cptr->first->len + 2)))
	{
	  templen = cptr->first->len;
	  bcopy(&(templen), outbuf + outlen, 2);
	  bcopy(cptr->first->data, outbuf + outlen + 2, templen);
	  outlen += templen + 2;
	  free(cptr->first->data);
	  tmsg = cptr->first;
	  cptr->first = cptr->first->next;
	  free(tmsg);
	  if (!cptr->first)
	    cptr->last = 0;
	}

	if (outlen)
	{
	  len = send(mud_sock, outbuf, outlen, 0);
	  if (len > 0)
	  {
	    outlen -= len;
	    bcopy(outbuf + len, outbuf, outlen);
	  }
	  else
	  {
	    panic();
	  }
	}
      } else
      if (FD_ISSET(loop, &out) && (cptr->first))
      {
	len = send(loop, cptr->first->data, cptr->first->len, 0);
	free(cptr->first->data);
	tmsg = cptr->first;
	cptr->first = cptr->first->next;
	free(tmsg);
	if (!cptr->first)
	  cptr->last = 0;
      }
      /* Test for pending disconnect */
      else
      if ((cptr->status == 2) && (cptr->first == 0))
      {
	cptr->status = 0;
	shutdown(loop, 0);
	close(loop);
      }
    }
    /* Test for emptyness */
    if (!accepting)
    {
      for (loop = mud_sock + 1; loop < NOFILE; ++loop)
	if (clist[loop].status)
	  break;
      if (loop == NOFILE)
	exit(0);
    }
#ifdef CONCTIMEOUT
    /*
     * If timed out, close all connections (still loop once to allow all
     * I/O to finish.  I dont know whether that is necessary. -- Fuzzy)
     */

    if (warn == 4)
    { for (loop = mud_sock + 1; loop < NOFILE; ++loop)
      { if (clist[loop].status == 1) disconnect (loop); }
      
      writelog ("CONC %d: TIME OUT", clvl);
    }
#endif
  }
}

/* Properly disconnect a user */
disconnect(user)
  int             user;
{
  char            header[4];

  /* make control message for disconnect */
  header[0] = 0;
  header[1] = 2;		       /* disconnect code */
  header[2] = user;
  queue_message(mud_sock, header, 3);

  /* shutdown this socket */
  clist[user].status = 0;
  close(user);
#ifdef DEBUG
  writelog("CONC %d: USER DISCONNECT: %d\n", clvl, user);
#endif
}

void            queue_message(int port, char *data, int len)
{
  struct message *ptr;

  ptr = (struct message *) malloc(sizeof(struct message));
  ptr->data = (char *)malloc(len);
  ptr->len = len;
  bcopy(data, ptr->data, len);
  ptr->next = 0;
  if (clist[port].last == 0)
    clist[port].first = ptr;
  else
    (clist[port].last)->next = ptr;
  clist[port].last = ptr;
}

/* Kill off all connections quickly */
panic()
{
  int             loop;

  for (loop = 1; loop < NOFILE; ++loop)
  {
    if (clist[loop].status)
    {
      send(loop, PANIC_MESSAGE, sizeof(PANIC_MESSAGE), 0);
      shutdown(loop, 0);
      close(loop);
    }
  }
  exit(1);
}

/* Modified to send stuff to the main server for logging */
void            writelog(const char *fmt,...)
{
  va_list         list;
  struct tm      *tm;
  long            t;
  char            buffer[2048];

  va_start(list, fmt);
  vsprintf(buffer + 2, fmt, list);
  buffer[0] = 0;
  buffer[1] = 4;		       /* remote log command */
  queue_message(mud_sock, buffer, strlen(buffer + 2) + 2);
  va_end(list);
}
