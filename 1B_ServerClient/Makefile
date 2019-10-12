#@file server client Makefile
#@author Miriam Gehbauer <e11708473@student.tuwien.ac.at>
#@date 27.03.2019

all:client client.o server server.o

client.o:client.c
	gcc -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g -c client.c

client:client.o
	gcc -o client client.o

server.o:server.c
	gcc -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g -c server.c

server:server.o
	gcc -o server server.o

clean: 
	rm -f client client.o server.o server
