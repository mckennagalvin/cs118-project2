all: sender receiver

sender : sender.o
	gcc -std=c99 sender.o -o sender

sender.o : sender.c
	gcc -std=c99 -c sender.c

receiver : receiver.o
	gcc -std=c99 receiver.o -o receiver

receiver.o : receiver.c
	gcc -std=c99 -c receiver.c

clean: 
	rm receiver sender receiver.o sender.o makefile.deps

-include makefile.deps

makefile.deps:
	gcc -std=c99 -MM *.c > makefile.deps
