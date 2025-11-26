CC = gcc
CFLAGS = -Wall -O2 `sdl2-config --cflags`
LIBS = `sdl2-config --libs` -lSDL2_image -lpthread

CLIENT_OBJS = client.o client_system.o window.o
SERVER_OBJS = server.o server_system.o 

all: client server

client: $(CLIENT_OBJS)
	$(CC) -o $@ $(CLIENT_OBJS) $(LIBS)

server: $(SERVER_OBJS)
	$(CC) -o $@ $(SERVER_OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) client server
