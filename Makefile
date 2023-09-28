LIBFLAG= -shared -fpic

nsenter.so: nsenter.c
	gcc nsenter.c -o nsenter.so $(LIBFLAG) -llua