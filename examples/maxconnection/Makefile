all:echo
	
echo:echo.o main.o	
	g++ $^ -o $@ -lmuduo_net -lmuduo_base -lpthread

%.o:%.c
	g++ -c $^

clean:
	rm -rf *.o echo
