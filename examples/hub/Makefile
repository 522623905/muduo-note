all:hub sub pub
hub:codec.o hub.o
	g++ -g $^ -o $@ -lmuduo_net -lmuduo_base -lpthread
sub:sub.o codec.o pubsub.o
	g++ -g $^ -o $@ -lmuduo_net -lmuduo_base -lpthread
pub:pub.o codec.o pubsub.o
	g++ $^ -o $@ -lmuduo_net -lmuduo_base -lpthread
%.o:%.cc
	g++ -g -c $^ 
clean:
	rm -rf *.o hub sub pub