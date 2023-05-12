#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !defined(__MACH__)
#include <malloc.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <regex.h>
#include <mosquitto.h>

char *localhost = "localhost";

typedef struct _mqttattr {
        char *src_mqtt_host;
        int src_mqtt_port;
	int src_qos;
        char *src_mqtt_user;
        char *src_mqtt_password;
        char *dst_mqtt_host;
        int dst_mqtt_port;
        int dst_qos;
        int dst_retain;
        char *dst_mqtt_user;
        char *dst_mqtt_password;
        char cid[20];
	int verbose;
	int tlen;
        char **topics;
	int flen;
	char **filters;
        int dryrun;
} mqttattr;

mqttattr create_mqttattr() {
        mqttattr c;
        c.src_mqtt_host = NULL;
        c.src_mqtt_port = 1883;
	c.src_qos = 0;
        c.src_mqtt_user = NULL;
        c.src_mqtt_password = NULL;
        c.dst_mqtt_host = NULL;
        c.dst_mqtt_port = 1883;
        c.dst_qos = 0;
        c.dst_retain = 0;
        c.dst_mqtt_user = NULL;
        c.dst_mqtt_password = NULL;
	c.verbose = 0;
	c.tlen = 0;
	c.topics = NULL;
	c.flen = 0;
	c.filters = NULL;
        c.dryrun = 0;
        return(c);
}

int add_topic(mqttattr *mqtta, char *topic) {
	if (mosquitto_sub_topic_check(topic) == MOSQ_ERR_SUCCESS) {
		char **p;
        	p = (char**) realloc(mqtta->topics, (mqtta->tlen + 1) * sizeof(char*));
        	if (p != NULL) {
                	mqtta->topics = p;
                	mqtta->topics[mqtta->tlen] = topic;
                	mqtta->tlen++;
                	return(mqtta->tlen);
        	}
	} else {
		if (mqtta->verbose) printf("Warning: '%s' is not a valid topic\n", topic);
	}
        return(0);
}

int add_filter(mqttattr *mqtta, char *filter) {
	char **p;
	p = (char**) realloc(mqtta->filters, (mqtta->flen + 1) * sizeof(char*));
	if (p != NULL) {
		mqtta->filters = p;
		mqtta->filters[mqtta->flen] = filter;
		mqtta->flen++;
		return(mqtta->flen);
	}
	return(0);
}

void destroy_mqttattr(mqttattr *mqtta) {
        if (mqtta) {
                if (mqtta->topics) free(mqtta->topics);
                mqtta->tlen = 0;
		if (mqtta->filters) free(mqtta->filters);
		mqtta->flen = 0;
        }
	return;
}

char *now(char *ts) {
        struct tm *timeinfo;
        struct timeval tv;

        gettimeofday(&tv, NULL);
        timeinfo = localtime(&tv.tv_sec);

        sprintf(ts, "%.4d%.2d%.2d%.2d%.2d%.2d.%.6ld", timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec,tv.tv_usec);
        return(ts);
}

int publish(char *cid, char *host, int port, char *topic, char *payload, int qos, int retain, char *user, char *password, int verbose)
{
        int rc = 1;
        static struct mosquitto *mosq = NULL;
        bool r = retain?true:false;
        char timestamp[24];
        now(timestamp);

        if (!mosq) {
                mosq = mosquitto_new(cid, true, NULL);
                if (mosq) {
                        if (user && password) mosquitto_username_pw_set(mosq, user, password);
                        if (mosquitto_connect(mosq, host, port, 10)) {
                                if (verbose) printf("[%s] publish: Error mosquitto_connect failed.\n", timestamp);
                                mosquitto_destroy(mosq);
                                mosq = NULL;
                        } else {
                                if (verbose) printf("[%s] Destination MQTT broker connected.\n", timestamp);
                        }
                } else {
                        if (verbose) printf("[%s] publish: Error mosquitto_new or mosquitto_connect failed.\n", timestamp);
                }

        }
        if (mosq) {
                rc = mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, qos, r);
                if (rc) {
                        if (verbose) printf("[%s] publish: Error >%s<\n", timestamp, mosquitto_strerror(rc));
                        mosquitto_disconnect(mosq);
                        mosquitto_destroy(mosq);
                        mosq = NULL;
                }
        }
        return(rc);
}

int regex_match(char *string, char *pattern) {
	regex_t preg;
	size_t nmatch = 1;
	regmatch_t pmatch[nmatch];

	if (regcomp(&preg, pattern, REG_EXTENDED|REG_NEWLINE)) {
		return(0);
	}
	if (regexec(&preg, string, nmatch, pmatch, 0) == REG_NOMATCH) {
		regfree(&preg);
		return(0);
	} else {
		regfree(&preg);
		return(1);
	}
}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	char timestamp[24];
	now(timestamp);
	mqttattr *mqtta = obj;

	if (!result) {
		mosquitto_subscribe_multiple(mosq, NULL, mqtta->tlen, (char *const *const)mqtta->topics, mqtta->src_qos, 0, NULL);

		if (mqtta->verbose) {
			printf("[%s] Source MQTT broker connected.\n", timestamp);
			int i;
			for (i = 0; i < mqtta->tlen; i++) {
				printf("[%s] topic '%s' subscribed.\n", timestamp, mqtta->topics[i]);
			}
			for (i = 0; i < mqtta->flen; i++) {
				printf("[%s] topics matching with '%s' will be filtered out.\n", timestamp, mqtta->filters[i]);
			}
		}
	}
	return;
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	bool match = 0;
	char timestamp[24];
	mqttattr *mqtta = obj;
	int i;

	now(timestamp);

	for (i = 0; i < mqtta->flen; i++) {
		if (regex_match(message->topic, mqtta->filters[i])) return;
	}

	for (i = 0; i < mqtta->tlen; i++) {
                mosquitto_topic_matches_sub(mqtta->topics[i], message->topic, &match);
                if (match) break;
        }

        if (mqtta->verbose) printf("[%s] >%s< >%.*s<\n", timestamp, message->topic, message->payloadlen, (char*)message->payload);

        if (!mqtta->dryrun) publish(mqtta->cid, mqtta->dst_mqtt_host, mqtta->dst_mqtt_port, message->topic, (char*)message->payload, mqtta->dst_qos, mqtta->dst_retain, mqtta->dst_mqtt_user, mqtta->dst_mqtt_password, mqtta->verbose);

	return;
}

int main(int argc, char **argv) {
	struct mosquitto *mosq;
	int rc = 0;
	int i = 0;
        int daemon = 0;

	mqttattr mqtta = create_mqttattr();

	while (i < argc) {
		if ((!strcmp(argv[i], "--src_host")) && (i+1 < argc)) mqtta.src_mqtt_host = argv[++i];
		if ((!strcmp(argv[i], "--src_port")) && (i+1 < argc)) mqtta.src_mqtt_port = atoi(argv[++i]);
		if ((!strcmp(argv[i], "--src_qos")) && (i+1 < argc)) mqtta.src_qos = atoi(argv[++i]);
                if ((!strcmp(argv[i], "--src_user")) && (i+1 < argc)) mqtta.src_mqtt_user = argv[++i];
                if ((!strcmp(argv[i], "--src_password")) && (i+1 < argc)) mqtta.src_mqtt_password = argv[++i];
                if ((!strcmp(argv[i], "--dst_host")) && (i+1 < argc)) mqtta.dst_mqtt_host = argv[++i];
                if ((!strcmp(argv[i], "--dst_port")) && (i+1 < argc)) mqtta.dst_mqtt_port = atoi(argv[++i]);
                if ((!strcmp(argv[i], "--dst_qos")) && (i+1 < argc)) mqtta.dst_qos = atoi(argv[++i]);
                if ((!strcmp(argv[i], "--dst_retain")) && (i+1 < argc)) mqtta.dst_retain = atoi(argv[++i]);
                if ((!strcmp(argv[i], "--dst_user")) && (i+1 < argc)) mqtta.dst_mqtt_user = argv[++i];
                if ((!strcmp(argv[i], "--dst_password")) && (i+1 < argc)) mqtta.dst_mqtt_password = argv[++i];
		if ((!strcmp(argv[i], "-t")) && (i+1 < argc)) add_topic(&mqtta, argv[++i]);
		if ((!strcmp(argv[i], "-f")) && (i+1 < argc)) add_filter(&mqtta, argv[++i]);
		if (!strcmp(argv[i], "-v")) mqtta.verbose = 1;
                if (!strcmp(argv[i], "-d")) daemon = 1;
                if (!strcmp(argv[i], "--dry")) mqtta.dryrun = 1;
		i++;
	}

	if (!mqtta.src_mqtt_host) mqtta.src_mqtt_host = localhost;
        if (!mqtta.dst_mqtt_host) mqtta.dst_mqtt_host = localhost;
	if (!mqtta.tlen) add_topic(&mqtta, "#");
	if ((mqtta.src_qos < 0) || (mqtta.src_qos > 2)) mqtta.src_qos = 0;
        if ((mqtta.dst_qos < 0) || (mqtta.dst_qos > 2)) mqtta.dst_qos = 0;

        if ((mqtta.src_mqtt_host == mqtta.dst_mqtt_host) && (mqtta.src_mqtt_port == mqtta.dst_mqtt_port)) {
               printf("MQTT bridge - Tool to transfer data from an MQTT broker to another MQTT broker\n\nusage: %s\n", basename(argv[0]));
               printf("\t\t\t--src_host <host> of the source MQTT broker (default: localhost)\n");
               printf("\t\t\t--src_port <port> of the source MQTT broker (default: 1883)\n");
               printf("\t\t\t--src_user <user> for login to the source MQTT broker\n");
               printf("\t\t\t--src_password <password> for login to the source MQTT broker\n");
               printf("\t\t\t--src_qos <0..2> QOS of the source MQTT broker (default: 0)\n");
               printf("\t\t\t--dst_host <host> of the target MQTT broker (default: localhost)\n");
               printf("\t\t\t--dst_port <port> of the target MQTT broker (default: 1883)\n");
               printf("\t\t\t--dst_user <user> for login to the target MQTT broker\n");
               printf("\t\t\t--dst_password <password> for login to the target MQTT broker\n");
               printf("\t\t\t--dst_qos <0..2> QOS of the target MQTT broker (default: 0)\n");
               printf("\t\t\t--dst_retain <0,1> retain the data in the target MQTT broker (default: 0)\n");
               printf("\t\t\t-t <topic> Topic will be subscribed at source and published to target (can occur multiple times)\n");
               printf("\t\t\t-f <regex> matching topics will be filtered out (can occur multiple times)\n");
               printf("\t\t\t--dry dryrun mode, subscribe the topics without publishing to target\n");
               printf("\t\t\t-v verbose mode\n");
               printf("\t\t\t-d daemon mode\n\n");
               printf("Example: %s --src_host host1 --dst_host host2 -t \"smarthome/#\" -t \"devices/+/temperature\"\n", basename(argv[0]));
               printf("\nError: MQTT source must be different to MQTT target broker\n");
               return(1);
        }

	sprintf(mqtta.cid, "bridge/%d", getpid());

        if (daemon) {
               mqtta.verbose = 0;
               mqtta.dryrun = 0;
               pid_t pid, sid;
               pid = fork();
               if (pid < 0)
                   exit(1);
               if (pid > 0)
                   exit(0);
               umask(0);
               sid = setsid();
               if (sid < 0)
                   exit(EXIT_FAILURE);
               if ((chdir("/")) < 0)
                   exit(EXIT_FAILURE);
               close(STDIN_FILENO);
               close(STDOUT_FILENO);
               close(STDERR_FILENO);
        }

	mosquitto_lib_init();
	mosq = mosquitto_new(mqtta.cid, true, &mqtta);

	if (mosq) {
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		if (mqtta.verbose) {
			int major, minor, revision;
			mosquitto_lib_version(&major, &minor, &revision);
			printf("%s (libmosquitto %d.%d.%d)\n", basename(argv[0]), major, minor, revision);
			printf("Connecting (%s) to %s:%d with qos=%d\n", mqtta.cid, mqtta.src_mqtt_host, mqtta.src_mqtt_port, mqtta.src_qos);
                        printf("Publishing to %s:%d with qos=%d retain=%d\n", mqtta.dst_mqtt_host, mqtta.dst_mqtt_port, mqtta.dst_qos, mqtta.dst_retain);
		}
                if (mqtta.src_mqtt_user && mqtta.src_mqtt_password) mosquitto_username_pw_set(mosq, mqtta.src_mqtt_user, mqtta.src_mqtt_password);
                rc = mosquitto_connect(mosq, mqtta.src_mqtt_host, mqtta.src_mqtt_port, 60);
                if (!rc) {
                        rc = mosquitto_loop_forever(mosq, -1, 1);
                } else {
                        if (mqtta.verbose) printf("Error: Could not connect to '%s:%d'\n", mqtta.src_mqtt_host, mqtta.src_mqtt_port);
                }

		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
	}
	mosquitto_lib_cleanup();
	destroy_mqttattr(&mqtta);
	return(rc);
}
