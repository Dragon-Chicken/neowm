FLAGS=-O3 -Wall -Wextra -Wpedantic
PREFIX = /usr/local

.PHONY: nwm nwmc dir

build: dir nwm nwmc

dir:
	mkdir -p build

nwm:
	gcc $(FLAGS) -lpthread -lX11 -lXcursor src/*.c -o build/nwm

nwmc:
	gcc $(FLAGS) src/nwmc/*.c -o build/nwmc

xephyr:
	startx ./build/nwm -- /usr/bin/Xephyr -screen 1290x720 :1

clean:
	rm build/*

install: build
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f build/* ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwm
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwmc

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwm\
		${DESTDIR}${PREFIX}/bin/nwmc
