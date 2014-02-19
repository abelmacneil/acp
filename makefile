VPATH=./src:./src/include
SRCDIR=src
BUILDDIR=obj
BINDIR=bin
CC=gcc
LIBS=-lm
CFLAGS=-O2 -Wall
INCLUDE=-I src/include 
MAIN=main
SERVER=server
CLIENT=client
OTHERSRCS=protocol.c crypto.c
CLIENTSRCS=$(CLIENT).c $(OTHERSRCS)
SERVERSRCS=$(SERVER).c $(OTHERSRCS)
CLIENTOBJS=$(CLIENTSRCS:%.c=$(BUILDDIR)/%.o)
SERVEROBJS=$(SERVERSRCS:%.c=$(BUILDDIR)/%.o)
CLIENTBIN=$(BINDIR)/$(CLIENT)
SERVERBIN=$(BINDIR)/$(SERVER)

compile : mkdirs $(SERVERBIN) $(CLIENTBIN)

$(SERVERBIN)	: $(SERVEROBJS)
	$(CC) $(CFLAGS) $(LIBS) $(SERVEROBJS) $(INCLUDE) -o $(SERVERBIN)

$(CLIENTBIN)	: $(CLIENTOBJS)
	$(CC) $(CFLAGS) $(LIBS) $(CLIENTOBJS) $(INCLUDE) -o $(CLIENTBIN)

$(SERVER).o 	: $(SERVER).c protocol.o
$(CLIENT).o 	: $(CLIENT).c protocol.o
protocol.o 		: protocol.h protocol.c

#-------------------------------------------------------------

run-server: compile
	$(SERVERBIN)

run-client: compile
	$(CLIENTBIN)

clean:
	rm -r $(BUILDDIR)
	rm -r $(BINDIR)

mkdirs:
	mkdir -p $(BUILDDIR)
	mkdir -p $(BINDIR)

build: clean compile	

lines :
	wc -l `find $(SRCDIR) -type f` | sort -nk 1
	
$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

#=============================================================#
