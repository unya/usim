
SRC = main.c decode.c ucode.c

usim: $(SRC)
	cc -o usim -g $(SRC)
	./usim >xx
