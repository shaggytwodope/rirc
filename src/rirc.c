#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <poll.h>
#include <time.h>
#include <termios.h>
#include <getopt.h>

#include "common.h"

/* Values parsed from getopts */
struct
{
	int port;
	char *connect;
	char *join;
	char *nicks;
} opts;

void splash(void);
void startup(void);
void cleanup(void);
void configure(void);
void main_loop(void);
void usage(void);
void getopts(int, char**);
void signal_sigwinch(int);

extern int numfds;

struct termios oterm, nterm;
struct pollfd fds[MAXSERVERS + 1] = {{0}};

int
main(int argc, char **argv)
{
	getopts(argc, argv);
	configure();
	startup();
	main_loop();

	return EXIT_SUCCESS;
}

void
usage(void)
{
	puts(
	"\n"
	"rirc version " VERSION " ~ Richard C. Robbins <mail@rcr.io>\n"
	"\n"
	"Usage:\n"
	"  rirc [-c server [OPTIONS]]\n"
	"\n"
	"Help:\n"
	"  -h, --help             Print this message\n"
	"\n"
	"Options:\n"
	"  -c, --connect=SERVER   Connect to SERVER\n"
	"  -p, --port=PORT        Connect using PORT\n"
	"  -j, --join=CHANNELS    Comma separated list of channels to join\n"
	"  -n, --nicks=NICKS      Comma/space separated list of nicks to use\n"
	"  -v, --version          Print rirc version and exit\n"
	"\n"
	"Examples:\n"
	"  rirc -c server.tld -j '#chan' -n nick\n"
	"  rirc -c server.tld -p 1234 -j '#chan1,#chan2' -n 'nick, nick_, nick__'\n"
	);
}

void
splash(void)
{
	newline(rirc, 0, "--", "      _          ", 0);
	newline(rirc, 0, "--", " _ __(_)_ __ ___ ", 0);
	newline(rirc, 0, "--", "| '__| | '__/ __|", 0);
	newline(rirc, 0, "--", "| |  | | | | (__ ", 0);
	newline(rirc, 0, "--", "|_|  |_|_|  \\___|", 0);
	newline(rirc, 0, "--", " ", 0);
	newline(rirc, 0, "--", " - version " VERSION, 0);
}

void
getopts(int argc, char **argv)
{
	opts.port = 0;
	opts.connect = NULL;
	opts.join    = NULL;
	opts.nicks   = NULL;

	int c, opt_i = 0;

	static struct option long_opts[] =
	{
		{"connect", required_argument, 0, 'c'},
		{"port",    required_argument, 0, 'p'},
		{"join",    required_argument, 0, 'j'},
		{"nick",    required_argument, 0, 'n'},
		{"version", no_argument,       0, 'v'},
		{"help",    no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "c:p:n:j:vh", long_opts, &opt_i))) {

		if (c == -1)
			break;

		switch(c) {

			/* Connect to server */
			case 'c':
				if (*optarg == '-') {
					puts("-c/--connect requires an argument");
					exit(EXIT_FAILURE);
				} else {
					opts.connect = optarg;
				}
				break;

			/* Connect using port */
			case 'p':
				if (*optarg == '-') {
					puts("-p/--port requires an argument");
					exit(EXIT_FAILURE);
				} else {
					/* parse out the port */
					int port = 0;
					char *ptr = optarg;

					while (*ptr) {
						if (!isdigit(*ptr)) {
							fprintf(stderr, "Invalid port number: %s\n",
								optarg);
							exit(EXIT_FAILURE);
						}

						port = port * 10 + (*ptr++ - '0');

						if (port > 65534) {
							fprintf(stderr, "Port number out of range: %s\n",
								optarg);
							exit(EXIT_FAILURE);
						}
					}

					opts.port = port;
				}
				break;

			/* Comma/space separated list of nicks to use */
			case 'n':
				if (*optarg == '-') {
					puts("-n/--nick requires an argument");
					exit(EXIT_FAILURE);
				} else {
					opts.nicks = optarg;
				}
				break;

			/*Comma separated list of channels to join */
			case 'j':
				if (*optarg == '-') {
					puts("-j/--join requires an argument");
					exit(EXIT_FAILURE);
				} else {
					opts.join = optarg;
				}
				break;

			/* Print rirc version and exit */
			case 'v':
				puts("rirc version " VERSION );
				exit(EXIT_SUCCESS);

			/* Print rirc usage and exit */
			case 'h':
				usage();
				exit(EXIT_SUCCESS);

			default:
				printf("%s --help for usage\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
}

void
configure(void)
{
	if (opts.connect) {
		config.auto_connect = opts.connect;
		config.auto_port = opts.port ? opts.port : 6667;
		config.auto_join = opts.join;
		config.nicks = opts.nicks ? opts.nicks : "";
	} else {
		/* TODO: parse a configuration file. for now set defaults here */
		config.auto_connect = NULL;
		config.auto_port = 0;
		config.auto_join = NULL;
		config.nicks = "";
	}

	/* TODO: parse a configuration file. for now set defaults here */
	config.username = "rirc_v" VERSION;
	config.realname = "rirc v" VERSION;
	config.join_part_quit_threshold = 100;
}

void
signal_sigwinch(int unused)
{
	resize();
	if (signal(SIGWINCH, signal_sigwinch) == SIG_ERR)
		fatal("signal handler: SIGWINCH");
}

void
startup(void)
{
	setbuf(stdout, NULL);

	/* Initialize the fds for polling */
	int i;
	fds[0].fd = 0; /* stdin */
	for (i = 0; i < MAXSERVERS + 1; i++)
		fds[i].events = POLLIN;

	/* Set terminal to raw mode */
	tcgetattr(0, &oterm);
	memcpy(&nterm, &oterm, sizeof(struct termios));
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");

	/* Set mousewheel event handling */
	printf("\x1b[?1000h");

	srand(time(NULL));

	confirm = 0;

	rirc = cfirst = ccur = new_channel("rirc");

	splash();

	/* Set sigwinch, init draw */
	signal_sigwinch(0);

	/* Register cleanup for exit */
	atexit(cleanup);

	exit_fatal = 0;

	if (config.auto_connect)
		con_server(config.auto_connect, config.auto_port);
}

void
cleanup(void)
{
	/* Reset terminal modes */
	tcsetattr(0, TCSADRAIN, &oterm);

	/* Reset mousewheel event handling */
	printf("\x1b[?1000l");

	ccur = cfirst;
	do {
		channel_close();
	} while (cfirst != rirc);

	free_channel(rirc);

	/* Don't clear screen on fatal exit to preserve error message */
	if (!exit_fatal)
		printf("\x1b[H\x1b[J");
}

void
main_loop(void)
{
	char buff[BUFFSIZE];
	int i, ret, count = 0, time = 200;

	while (1) {

		ret = poll(fds, numfds, time);

		/* Poll timeout */
		if (ret == 0) {
			if (count) {
				buff[count] = '\0'; /* FIXME, temporary fix */
				inputc(buff, count);
				count = 0;
			}
			time = 200;
		/* Check input on stdin */
		} else if (fds[0].revents & POLLIN) {
			count = read(0, buff, BUFFSIZE);
			time = 0;
		/* Check all open server sockets */
		} else {
			for (i = 1; i < numfds; i++) {
				if (fds[i].revents & POLLIN) {
					if ((count = read(fds[i].fd, buff, BUFFSIZE)) == 0)
						con_lost(fds[i].fd);
					else
						recv_mesg(buff, count, fds[i].fd);
					time = count = 0;
				}
			}
		}

		/* Skip if no draw bits set */
		if (draw)
			redraw();
	}
}
