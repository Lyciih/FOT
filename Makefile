fot: main.cpp
	g++ main.cpp -o fot -Wall -lxcb -lxcb-keysyms -ltesseract -llept

.PHONY: clean run install uninstall

clean:
	rm -f fot

run:
	./fot

install:
	cp fot /usr/local/bin/


uninstall:
	rm /usr/local/bin/fot
