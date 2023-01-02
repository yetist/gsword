all:


module-check:
	g++ -o aa.so --shared gsw-module.* `pkg-config --cflags --libs sword glib-2.0` -fPIC
status-check:
	g++ -o aa.so --shared gsw-status-reporter.* `pkg-config --cflags --libs sword glib-2.0` -fPIC
modinfo-check:
	gcc -o aa.so --shared gsw-modinfo.* `pkg-config --cflags --libs sword gobject-2.0` -fPIC
