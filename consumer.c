#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>


#include <librdkafka/rdkafka.h>


static volatile sig_atomic_t run   = 1;
static volatile sig_atomic_t stats = 0;

/**
 * @brief Signal termination of program
 */
static void stop(int sig) {
        run = 0;
}

static int stats_cb(rd_kafka_t *rk, char *json, size_t json_len, void *opaque) {
        if (stats) {
                printf("%s\n", json);
        }
        return 0;
}


int main(int argc, char **argv) {
        rd_kafka_t *rk;          /* Consumer instance handle */
        rd_kafka_conf_t *conf;   /* Temporary configuration object */
        rd_kafka_resp_err_t err; /* librdkafka API error code */
        char errstr[512];        /* librdkafka API error reporting buffer */
        const char *brokers;     /* Argument: broker list */
        rd_kafka_topic_partition_list_t *subscription; /* Subscribed topics */

        /*
         * Argument validation
         */
        if (argc != 2) {
                fprintf(stderr, "%% Usage: %s <broker>\n", argv[0]);
                return 1;
        }

        brokers = argv[1];

        /*
         * Create Kafka client configuration place-holder
         */
        conf = rd_kafka_conf_new();

        /* Set bootstrap broker(s) as a comma-separated list of
         * host or host:port (default port 9092).
         * librdkafka will use the bootstrap brokers to acquire the full
         * set of brokers from the cluster. */
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }

        /* Set the consumer group id.
         * All consumers sharing the same group id will join the same
         * group, and the subscribed topic' partitions will be assigned
         * according to the partition.assignment.strategy
         * (consumer config property) to the consumers in the group. */
        if (rd_kafka_conf_set(conf, "group.id", "4196-repro", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }

        /* If there is no previously committed offset for a partition
         * the auto.offset.reset strategy will be used to decide where
         * in the partition to start fetching messages.
         * By setting this to earliest the consumer will read all messages
         * in the partition if there was no previously committed offset. */
        if (rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }

        /* Make test easy to run again and again without re-running producer. */
        if (rd_kafka_conf_set(conf, "enable.auto.offset.store", "false", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }

        /* Setup stats so we can get queue size and ensure it's full */
        if (rd_kafka_conf_set(conf, "statistics.interval.ms", "1000", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }
        rd_kafka_conf_set_stats_cb(conf, stats_cb);

        /* Make sure we got a bunch of messages buffered because we're gonna
         * read really fast*/
        if (rd_kafka_conf_set(conf, "queued.max.messages.kbytes", "500000",
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }
        if (rd_kafka_conf_set(conf, "queued.min.messages", "6000000", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                rd_kafka_conf_destroy(conf);
                return 1;
        }

        /*
         * Create consumer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
        if (!rk) {
                fprintf(stderr, "%% Failed to create new consumer: %s\n",
                        errstr);
                return 1;
        }

        conf = NULL; /* Configuration object is now owned, and freed,
                      * by the rd_kafka_t instance. */


        /* Redirect all messages from per-partition queues to
         * the main queue so that messages can be consumed with one
         * call from all assigned partitions.
         *
         * The alternative is to poll the main queue (for events)
         * and each partition queue separately, which requires setting
         * up a rebalance callback and keeping track of the assignment:
         * but that is more complex and typically not recommended. */
        rd_kafka_poll_set_consumer(rk);


        subscription = rd_kafka_topic_partition_list_new(1);
        rd_kafka_topic_partition_list_add(subscription, "4196-repro",
                                          /* the partition is ignored
                                           * by subscribe() */
                                          RD_KAFKA_PARTITION_UA);

        /* Subscribe to the list of topics */
        err = rd_kafka_subscribe(rk, subscription);
        if (err) {
                fprintf(stderr, "%% Failed to subscribe to %d topics: %s\n",
                        subscription->cnt, rd_kafka_err2str(err));
                rd_kafka_topic_partition_list_destroy(subscription);
                rd_kafka_destroy(rk);
                return 1;
        }

        fprintf(stderr,
                "%% Subscribed to %d topic(s), "
                "waiting for rebalance and messages...\n",
                subscription->cnt);

        rd_kafka_topic_partition_list_destroy(subscription);


        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        /* Subscribing to topics will trigger a group rebalance
         * which may take some time to finish, but there is no need
         * for the application to handle this idle period in a special way
         * since a rebalance may happen at any time.
         * Start polling for messages. */

        rd_kafka_message_t *rkm;
        while (!(rkm = rd_kafka_consumer_poll(rk, 100))) {
        }
        rd_kafka_message_destroy(rkm);
        printf("Got first message\n");
        // stats = 1;

        sleep(3);
        stats = 1;

        // At this point stats should show that fetchq_cnt contains our 5000000
        // messages

        clock_t start = clock();
        printf("Clock %ld\n", start);
        int i = 0;
        for (; i < 5000000 && run; i++) {
                rkm = rd_kafka_consumer_poll(rk, 100);
                if (!rkm)
                        continue; /* Timeout: no message within 100ms,
                                   *  try again. This short timeout allows
                                   *  checking for `run` at frequent intervals.
                                   */

                /* consumer_poll() will return either a proper message
                 * or a consumer error (rkm->err is set). */
                if (rkm->err) {
                        /* Consumer errors are generally to be considered
                         * informational as the consumer will automatically
                         * try to recover from all types of errors. */
                        fprintf(stderr, "%% Consumer error: %s\n",
                                rd_kafka_message_errstr(rkm));
                        rd_kafka_message_destroy(rkm);
                        continue;
                }

                rd_kafka_message_destroy(rkm);
        }
        clock_t elapsed = clock() - start;
        printf("Elapsed %ld\n", elapsed);
        double elapsed_s = ((double)elapsed) / CLOCKS_PER_SEC;

        /* Close the consumer: commit final offsets and leave the group. */
        fprintf(stderr, "%% Closing consumer\n");
        rd_kafka_consumer_close(rk);

        fprintf(stderr, "%% Read %d messages in %f seconds\n", i, elapsed_s);

        /* Destroy the consumer */
        rd_kafka_destroy(rk);

        return 0;
}
