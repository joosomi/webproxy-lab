# Makefile for Proxy Lab 

# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

# CC = gcc
# CFLAGS = -g -Wall 
# LDFLAGS = -lpthread

# all: proxy

# csapp.o: csapp.c csapp.h
# 	$(CC) $(CFLAGS) -c csapp.c

# proxy.o: proxy.c csapp.h
# 	$(CC) $(CFLAGS) -c proxy.c

# proxy: proxy.o csapp.o
# 	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# # Creates a tarball in ../proxylab-handin.tar that you can then
# # hand in. DO NOT MODIFY THIS!
# handin:
# 	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

# clean:
# 	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz


CC = gcc
CFLAGS = -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: echoclient echoserver

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

echoserver: echoserver.c csapp.o echo.o
	$(CC) $(CFLAGS) -o echoserver echoserver.c csapp.o echo.o $(LIB)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

echo.o: echo.c
	$(CC) $(CFLAGS) -c echo.c

clean:
	rm -f *.o echoclient echoserver *~
