FLAGS=-O0 -Wall -Wextra
PREFIX = /usr/local

build: nwm nwmc

nwm:
	gcc $(FLAGS) -lpthread -lX11 src/*.c -o nwm

nwmc:
	gcc $(FLAGS) src/nwmc/*.c -o nwmc

xephyr:
	startx ./nwm -- /usr/bin/Xephyr -screen 1290x720 :1

clean:
	rm nwm
	rm nwmc

install: build
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f nwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwm
	cp -f nwmc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwmc

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwm\
		${DESTDIR}${PREFIX}/bin/nwmc

.PHONY: nwm nwmc
