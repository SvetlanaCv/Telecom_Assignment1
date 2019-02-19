make: 	client.c proxy.c
	gcc -o proxy proxy.c -lpthread
	gcc -o client client.c

clean: 
	rm -f *.exe
	rm -f *.o
