# DEBUG = -g
testprog: my_malloc.o
	gcc -o $@ *.o
%.o: %.c
	gcc $(DEBUG) -I. -c -o $@ $<
clean:
	rm -f *.o