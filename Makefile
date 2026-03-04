build:
	gcc -O0 -Wall -Wextra -lX11 src/*.c -o nwm

xephyr:
	startx ./nwm -- /usr/bin/Xephyr -screen 1290x720 :1

clean:
	rm nwm
