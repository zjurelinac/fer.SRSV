C_FLAGS = -Wall -Werror

all: generator posluzitelj

generator: generator.c util.h util.c
	@gcc $(C_FLAGS) -o generator generator.c util.c -lrt -lpthread

posluzitelj: posluzitelj.c util.h util.c
	@gcc $(C_FLAGS) -o posluzitelj posluzitelj.c util.c -lrt -lpthread
