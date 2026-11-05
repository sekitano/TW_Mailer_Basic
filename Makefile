CCOMPILER=g++
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++17 
#need c++17 because of std::filesystem in server.cpp

all: twmailer-server twmailer-client

twmailer-client: client.cpp
	$(CCOMPILER) $(CFLAGS) -o twmailer-client client.cpp

twmailer-server: server.cpp
	$(CCOMPILER) $(CFLAGS) -o twmailer-server server.cpp

clean: 
	rm -f twmailer-server twmailer-client
