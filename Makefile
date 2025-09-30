FOT: main.cpp
	g++ main.cpp -o fot -Wall -lxcb -lxcb-keysyms -ltesseract -llept

PHONY: clean run

clean:
	rm -f fot

run:
	./fot
