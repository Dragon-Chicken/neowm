FLAGS=-O0 -Wall -Wextra

build: nwm nwmc

.PHONY: nwm
nwm:
	gcc $(FLAGS) -lpthread -lX11 src/*.c -o nwm

.PHONY: nwmc
nwmc:
	gcc $(FLAGS) src/nwmc/*.c -o nwmc

xephyr:
	startx ./nwm -- /usr/bin/Xephyr -screen 1290x720 :1

clean:
	rm nwm
	rm nwmc
