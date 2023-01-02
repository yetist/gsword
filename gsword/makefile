all:


module-check:
	g++ -o aa.so --shared gsw-module.* `pkg-config --cflags --libs sword glib-2.0` -fPIC
status-check:
	g++ -o aa.so --shared gsw-status-reporter.* `pkg-config --cflags --libs sword glib-2.0` -fPIC
modinfo-check:
	gcc -o aa.so --shared gsw-modinfo.* `pkg-config --cflags --libs sword gobject-2.0` -fPIC
search-hit-check:
	gcc -o aa.so --shared gsw-search-hit.* `pkg-config --cflags --libs sword gobject-2.0` -fPIC
installer-check:
	g++ -o aa.so --shared gsw-status-reporter.* gsw-manager.* gsw-installer* `pkg-config --cflags --libs sword glib-2.0` -fPIC

manager-check:
	g++ -o aa.so --shared gsw-manager.* gsw-module.* `pkg-config --cflags --libs sword glib-2.0` -fPIC

test:
	g++ -o test gsw-manager.* gsw-module.* gsw-modinfo.* test.c `pkg-config --cflags --libs sword gobject-2.0`
