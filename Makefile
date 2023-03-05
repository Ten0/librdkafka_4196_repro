include librdkafka/mklove/Makefile.base

CFLAGS += -I../src
CXXFLAGS += -I../src-cpp

perf-test: consumer
	perf record --call-graph dwarf -g -F999 ./consumer ${BROKERS}
	perf script -F +pid > ./perf.perf

consumer: ./librdkafka/src/librdkafka.a consumer.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $@.c -o $@ $(LDFLAGS) \
		./librdkafka/src/librdkafka.a $(LIBS)

producer: ./librdkafka/src/librdkafka.a producer.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $@.c -o $@ $(LDFLAGS) \
		./librdkafka/src/librdkafka.a $(LIBS)

fill-topic: producer
	./producer ${BROKERS}

./librdkafka/src/librdkafka.a:
	make -C librdkafka src/librdkafka.a

clean:
	rm -f consumer
