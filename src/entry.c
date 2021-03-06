/*
  Copyright (C) 2006, 2007, 2008, 2009  Anthony Catel <a.catel@weelya.com>

  This file is part of APE Server.
  APE is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  APE is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with APE ; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* entry.c */

#include "plugins.h"
#include "main.h"
#include "sock.h"

#include "config.h"
#include "cmd.h"

#include "channel.h"

#include <signal.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include "utils.h"
#include "ticks.h"
#include "proxy.h"
#include "events.h"
#include "transports.h"
#include "servers.h"
#include "dns.h"

#include <grp.h>
#include <pwd.h>

#include <errno.h>

#define _VERSION "1.0-BETA1"

static void signal_handler(int sign)
{
	printf("\nShutdown...!\n\n");
	exit(1);
}

static int inc_rlimit(int nofile)
{
	struct rlimit rl;
	
	rl.rlim_cur = nofile;
	rl.rlim_max = nofile;
	
	return setrlimit(RLIMIT_NOFILE, &rl);
}

static void ape_daemon()
{
	if (0 != fork()) { 
		exit(0);
	}
	if (-1 == setsid()) {
		
		exit(0);
	}
	signal(SIGHUP, SIG_IGN);
	
	if (0 != fork()) {
		exit(0);
	}
	printf("Starting daemon.... pid : %i\n\n", getpid());
}


int main(int argc, char **argv) 
{
	apeconfig *srv;
	
	int random, im_r00t = 0;
	unsigned int getrandom = 0;
	
	struct _fdevent fdev;
	
	char cfgfile[512] = APE_CONFIG_FILE;
	
	register acetables *g_ape;
	
	if (argc > 1 && strcmp(argv[1], "--version") == 0) {
		printf("\n   AJAX Push Engine Serveur %s - (C) Anthony Catel <a.catel@weelya.com>\n   http://www.ape-project.org/\n\n", _VERSION);
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "--help") == 0) {
		printf("\n   AJAX Push Engine Serveur %s - (C) Anthony Catel <a.catel@weelya.com>\n   http://www.ape-project.org/\n", _VERSION);
		printf("\n   usage: aped [options]\n\n");
		printf("   Options:\n     --help             : Display this help\n     --version          : Show version number\n     --cfg <config path>: Load a specific config file (default is %s)\n\n", cfgfile);
		return 0;
	} else if (argc > 2 && strcmp(argv[1], "--cfg") == 0) {
		strncpy(cfgfile, argv[2], 512);
		cfgfile[strlen(argv[2])] = '\0';
		
	} else if (argc > 1) {
		printf("\n   AJAX Push Engine Serveur %s - (C) Anthony Catel <a.catel@weelya.com>\n   http://www.ape-project.org/\n\n", _VERSION);
		printf("   Unknown parameters - check \"aped --help\"\n\n");
		return 0;		
	}
	if (NULL == (srv = ape_config_load(cfgfile))) {
		printf("\nExited...\n\n");
		exit(1);
	}
	
	if (getuid() == 0) {
		im_r00t = 1;
	}
	
	printf("   _   ___ ___ \n");
	printf("  /_\\ | _ \\ __|\n");
	printf(" / _ \\|  _/ _| \n");
	printf("/_/ \\_\\_| |___|\nAJAX Push Engine\n\n");

	printf("Bind on port %i\n\n", atoi(CONFIG_VAL(Server, port, srv)));
	printf("Version : %s\n", _VERSION);
	printf("Build   : %s %s\n", __DATE__, __TIME__);
	printf("Author  : Weelya (contact@weelya.com)\n\n");

	signal(SIGINT, &signal_handler);
	signal(SIGPIPE, SIG_IGN);
	
	if (TICKS_RATE < 1) {
		printf("[ERR] TICKS_RATE cant be less than 1\n");
		return 0;
	}
	
	random = open("/dev/urandom", O_RDONLY);
	if (!random) {
		printf("Cannot open /dev/urandom... exiting\n");
		return 0;
	}
	read(random, &getrandom, 3);
	srand(getrandom);
	close(random);
	
	g_ape = xmalloc(sizeof(*g_ape));
	g_ape->basemem = 512;
	
	#ifdef USE_EPOLL_HANDLER
	fdev.handler = EVENT_EPOLL;
	#endif
	#ifdef USE_KQUEUE_HANDLER
	fdev.handler = EVENT_KQUEUE;
	#endif

	g_ape->co = xmalloc(sizeof(*g_ape->co) * g_ape->basemem);
	memset(g_ape->co, 0, sizeof(*g_ape->co) * g_ape->basemem);
	
	g_ape->srv = srv;
	g_ape->events = &fdev;
	events_init(g_ape, &g_ape->basemem);
	
	ape_dns_init(g_ape);
	
	servers_init(g_ape);
	
	if (im_r00t) {
		struct group *grp = NULL;
		struct passwd *pwd = NULL;
		
		if (inc_rlimit(atoi(CONFIG_VAL(Server, rlimit_nofile, srv))) == -1) {
			printf("[WARN] Cannot set the max filedescriptos limit (setrlimit) %s\n", strerror(errno));
		}
		
		/* Get the user information (uid section) */
		if ((pwd = getpwnam(CONFIG_VAL(uid, user, srv))) == NULL) {
			printf("[ERR] Can\'t find username %s\n", CONFIG_VAL(uid, user, srv));
			return -1;
		}
		if (pwd->pw_uid == 0) {
			printf("[ERR] %s uid can\'t be 0\n", CONFIG_VAL(uid, user, srv));
			return -1;			
		}
		
		/* Get the group information (uid section) */
		if ((grp = getgrnam(CONFIG_VAL(uid, group, srv))) == NULL) {
			printf("[ERR] Can\'t find group %s\n", CONFIG_VAL(uid, group, srv));
			return -1;
		}
		if (grp->gr_gid == 0) {
			printf("[ERR] %s gid can\'t be 0\n", CONFIG_VAL(uid, group, srv));
			return -1;
		}
		
		setgid(grp->gr_gid);
		setgroups(0, NULL);

		initgroups(CONFIG_VAL(uid, user, srv), grp->gr_gid);
		
		setuid(pwd->pw_uid);
	} else {
		printf("[WARN] You have to run \'aped\' as root to increase r_limit\n");
	}
	
	if (strcmp(CONFIG_VAL(Server, daemon, srv), "yes") == 0) {
		ape_daemon();
	}
	
	g_ape->cmd_hook = NULL;
	
	g_ape->hLogin = hashtbl_init();
	g_ape->hSessid = hashtbl_init();

	g_ape->hLusers = hashtbl_init();
	g_ape->hPubid = hashtbl_init();
	
	g_ape->proxy.list = NULL;
	g_ape->proxy.hosts = NULL;
	
	g_ape->hCallback = hashtbl_init();
	
	g_ape->bufout = NULL;
	
	g_ape->uHead = NULL;
	
	g_ape->nConnected = 0;
	g_ape->plugins = NULL;
	
	g_ape->properties = NULL;
	
	g_ape->timers.timers = NULL;
	g_ape->timers.ntimers = 0;
	
	add_ticked(check_timeout, g_ape);
	
	do_register(g_ape);
	
	proxy_init_from_conf(g_ape);
	transport_start(g_ape);	
	findandloadplugin(g_ape);
	
	
	/* Starting Up */
	sockroutine(g_ape); /* loop */
	/* Shutdown */
	
	hashtbl_free(g_ape->hLogin);
	hashtbl_free(g_ape->hSessid);
	hashtbl_free(g_ape->hLusers);
	
	hashtbl_free(g_ape->hCallback);
	
	free(g_ape->plugins);
	//free(srv);
	free(g_ape);
	
	return 0;
}

