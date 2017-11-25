myShell: main.cpp myShell.cpp myShell.h
	g++ -std=gnu++98 -Wall -Werror -pedantic -o myShell main.cpp myShell.cpp
clean:
	rm myShell *~
