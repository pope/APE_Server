EXEC=bin/aped

prefix		= /usr/local
bindir		= $(prefix)/bin


SRC=src/entry.c src/sock.c src/hash.c src/handle_http.c src/cmd.c src/users.c src/channel.c src/config.c src/json.c src/json_parser.c src/plugins.c src/http.c src/extend.c src/utils.c src/ticks.c src/proxy.c src/base64.c src/pipe.c src/raw.c src/events.c src/event_kqueue.c src/event_epoll.c src/transports.c src/servers.c src/dns.c

CFLAGS=-Wall -g -minline-all-stringops -rdynamic
LFLAGS=-ldl -lm
CC=gcc -std=c99 -D_GNU_SOURCE
RM=rm -f

all: aped

aped: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(EXEC) $(LFLAGS) ./libs/udns-0.0.9/libudns.a -I ./libs/udns-0.0.9/
install: 
	install -d $(bindir)
	install -m 755 $(EXEC) $(bindir)

uninstall:
	$(RM) $(bindir)/aped

clean:
	$(RM) $(EXEC)
