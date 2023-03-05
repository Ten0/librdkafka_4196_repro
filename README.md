# How to reproduce

## Clone this repository

```bash
git clone https://github.com/Ten0/librdkafka_4196_repro.git --recurse-submodules
cd librdkafka_4196_repro
```

## Get yourself a kafka up and running

Personally I use bitnami's docker-compose, but I suspect you may already have a server somewhere...

```bash
curl -sSL https://raw.githubusercontent.com/bitnami/containers/main/bitnami/kafka/docker-compose.yml > docker-compose.yml
docker-compose up
```

In order to access the server directly from the host, this seems to require to update the hosts file, so that when the server identifies itself as its container ID, your host can resolve that as a hostname:

```bash
docker ps
```
find your container id

```bash
docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' <container id>
```
get container ip

```bash
echo <ip> <container id> >> /etc/hosts
```

probably don't forget to clean it up after

## Create the topic

```bash
docker exec -it <container id> kafka-topics.sh --create --topic 4196-repro --bootstrap-server localhost:9092
```
(obviously if not running in docker you may have to update this)

## Run the test

### Compile librdkafka
```bash
cd librdkafka && ./configure && make && cd ..
```

### Fill topic
```bash
BROKERS=<container ip> make fill-topic
```
This will get 6 000 000 messages in the `4196-repro` topic.

### Run perf test
```bash
BROKERS=<container ip> make perf-test
```

Note that this perf test configures itself such that all 5 000 000 messages it will try to read are buffered before starting timer (this can be checked in the output stats, `fetchq_cnt` > 5 000 000).
This benchmarks in conditions where we have enough messages pre-loaded in the buffer.
