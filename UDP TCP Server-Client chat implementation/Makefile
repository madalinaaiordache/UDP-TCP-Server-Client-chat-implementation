CFLAGS = -Wall -g

all: server subscriber 

# Compileaza server.cpp
server: server.cpp helpers.h

# Compileaza subscriber.cpp
subscriber: subscriber.cpp helpers.h

.PHONY: clean run_server run_client

# Ruleaza serverul
run_server: server
	./server ${PORT}

# Ruleaza clientul TCP
run_subscriber: subscriber
	./subscriber ${CLIENT_ID} ${IP_SERVER} ${PORT}

# Ruleaza clientul UDP
run_udp_client:
	python3 pcom_hw2_udp_client/udp_client.py ${IP_SERVER} ${PORT}

clean:
	rm -f server client
