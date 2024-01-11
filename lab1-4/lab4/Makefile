# DEBUG = -g

all: compute tcompute report manage

compute: compute.o
	gcc -o $@ $^ -lrt -lpthread

tcompute: tcompute.o
	gcc -o $@ $^ -lrt -lpthread

report: report.o
	gcc -o $@ $^ -lrt -lpthread

manage: manage.o
	gcc -o $@ $^ -lrt -lpthread

tcompute.o: tcompute.c
	gcc $(DEBUG) -c -o $@ $< -lpthread

%.o: %.c
	gcc $(DEBUG) -c -o $@ $<

clean:
	rm -f *.o compute tcompute report manage
