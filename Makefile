myShell: main.cpp myShell.cpp myShell.h
	g++ -std=gnu++11 -Wall -Werror -pedantic -o myShell main.cpp myShell.cpp
clean:
	rm myShell *~
