build:
	gcc -O0 -Wall -Wextra nwmc/server.c -o server
	gcc -O0 -Wall -Wextra nwmc/client.c -o client

run:
	./nwm bind "some bindings" "some args"

cnwm:
	gcc -O0 -Wall -Wextra -lX11 src/*.c -o nwm

xephyr:
	startx ./nwm -- /usr/bin/Xephyr -screen 1290x720 :1

clean:
	rm nwm
