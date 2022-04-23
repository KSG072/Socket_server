server_exe : server.o
	gcc -o server_exe server.o

server.o : server.c
	gcc -c -o server.o server.c

clean :
	rm *.o server_exe