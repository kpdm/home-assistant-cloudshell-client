#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <mosquitto.h>
#include <jansson.h>

#include "hacsc_sensor.h"
#include "hacsc_ui.h"

#define BL_PATH "/sys/class/backlight/fb_hktft32/bl_power"

#define REFRESH_DISPLAY_ENABLED 30
#define REFRESH_DISPLAY_DISABLED 1

// main struct for user data:
// sensor data
// ui handle
// mosquitto handle
// display status
typedef struct user_data {
	hacsc_sensor_t *sensor;
	hacsc_win_t *ui;
	struct mosquitto *mosq;
	char *net_device;
	bool display_enabled;
} user_data_t;

// set display under BL_PATH either enabled or disabled
static bool is_display_enabled() 
{
	FILE *fp = fopen(BL_PATH, "r");
	if (fp == NULL) {
		return false;
	}
	int c = 0;
	fscanf(fp, "%d", &c);
	fclose(fp);
	return c == 1;
}

// set display under BL_PATH either enabled or disabled
static void enable_display(bool enable) 
{
	FILE *fp = fopen(BL_PATH, "w");
	if (fp == NULL) {
		return;
	}
	fprintf(fp, "%d", enable);
	fclose(fp);
}

// parse a timestamp in the json payload
// expected to be in iso 8601 format: e.g. "2025-01-15 06:36:54.758817+00:00"
static struct tm get_json_timestamp(const char *ts) 
{
	struct tm tstamp;
	strptime(ts, "%FT%TZ", &tstamp);
	return tstamp;
}


// parse the json payload. 
//
static int parse_payload(const char *json_string, user_data_t *data) 
{
	if (!json_string) {
		fprintf(stderr, "No json payload available.\n");
		return -1;
	}
	json_t *root = json_loads(json_string, 0, NULL);
	if (!root) {
		fprintf(stderr, "Error: Failed to parse JSON string.\n");
		return 1;
	}

	// id is mandatory
	const char *id = json_string_value(json_object_get(root, "id"));
	if (!id) {
		fprintf(stderr, "Error: No id found.\n");
		return 1;
	}

	if (strcmp(id, "sun.sun") == 0) { 
		struct tm sunrise = get_json_timestamp(json_string_value(json_object_get(root, "next_rising")));
		struct tm sunset = get_json_timestamp(json_string_value(json_object_get(root, "next_setting")));
		struct tm tstamp = get_json_timestamp(json_string_value(json_object_get(root, "last_updated")));

		data->sensor->sun_next_sunrise.value = sunrise;
		data->sensor->sun_next_sunrise.last_updated = tstamp;

		data->sensor->sun_next_sunset.value = sunset;
		data->sensor->sun_next_sunset.last_updated = tstamp;

	} else if (strcmp(id, "sunset") == 0) {
		data->display_enabled = false;
		enable_display(false);

	} else if (strcmp(id, "sunrise") == 0) {
		data->display_enabled = true;
		enable_display(true);

	} else if (strcmp(id, "sensor.inverter_ac_power") == 0) {
		data->sensor->inverter_power_ac.value = json_number_value(json_object_get(root, "value"));
		data->sensor->inverter_power_ac.last_updated = get_json_timestamp(json_string_value(json_object_get(root, "last_updated")));

	} else if (strcmp(id, "sensor.inverter_temperature") == 0) {
		data->sensor->inverter_temp.value = json_number_value(json_object_get(root, "value"));
		data->sensor->inverter_temp.last_updated = get_json_timestamp(json_string_value(json_object_get(root, "last_updated")));

	} else if (strcmp(id, "sensor.inverter_port_1_dc_power") == 0) {
		data->sensor->inverter_power_dc1.value = json_number_value(json_object_get(root, "value"));
		data->sensor->inverter_power_dc1.last_updated = get_json_timestamp(json_string_value(json_object_get(root, "last_updated")));

	} else if (strcmp(id, "sensor.inverter_port_2_dc_power") == 0) {
		data->sensor->inverter_power_dc2.value = json_number_value(json_object_get(root, "value"));
		data->sensor->inverter_power_dc2.last_updated = get_json_timestamp(json_string_value(json_object_get(root, "last_updated")));
	} 

	/*
	   wprintw(data->ui->mainwindow, "ID: %s\n", id);
	   wprintw(data->ui->mainwindow, "VALUE: %f\n", value);
	   wprintw(data->ui->mainwindow, "TSTAMP: %s\n", tstamp);
	   wrefresh(data->ui->mainwindow);
	   */

	// Free the JSON object
	json_decref(root);
	return 0;
}

// mosquitto message callback 
static void my_message_callback(struct mosquitto *mosq, void *data, const struct mosquitto_message *message)
{
	if(message->payloadlen) {
		parse_payload(message->payload, data); 
	}
}

// periodically refresh ui
void *ui_loop(void *arg) 
{
	user_data_t *data = (user_data_t*)arg;
	while(1) {
		if (data->display_enabled) {
			hacsc_win_refresh(data->ui, data->sensor, data->net_device);
			sleep(REFRESH_DISPLAY_DISABLED);
		} else {
			sleep(REFRESH_DISPLAY_ENABLED);
		}
	}
	return (void *)0;
}

static void usage(char *name) 
{
	fprintf(stdout, "Usage:\n"
			"%s -u MQTT_USERNAME -p MQTT_PASSWORT -h MQTT_HOST -P MQTT_PORT -t MQTT_TOPIC\n", name); 
}

int main(int argc, char **argv) 
{
	user_data_t *data = malloc(sizeof(user_data_t));

	int opt;
	char *user = NULL;
	char *pass = NULL;
	char *host = NULL;
	int port = -1;
	char *topic = NULL;

	while ((opt = getopt(argc, argv, "u:p:h:P:t:n:")) != -1) {
		switch(opt) {
			case 'u': 
				user = optarg;
				break;
			case 'p':
				pass = optarg;
				break;
			case 'h':
				host = optarg;
				break;
			case 'P':
				port = atoi(optarg);
				break;
			case 't':
				topic = optarg;
				break;
			case 'n':
				data->net_device = optarg;
				break;
			case '?':
				usage(argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (!user || !pass || !host || port == -1 || !topic || !data->net_device)  {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	// init mosq
	struct mosquitto *mosq;
	mosq = mosquitto_new("hacsc-subscriber", true, data);
	if(!mosq) {
		fprintf(stderr, "Error: Cannot init mqtt.\n");
		return EXIT_FAILURE;
	}
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_username_pw_set(mosq, user, pass);
	int rc;
	rc = mosquitto_connect(mosq, host, port, 60);
	if(rc) {
		fprintf(stderr, "Client connect failed with code %d.\n", rc);
		return EXIT_FAILURE;
	}

	rc = mosquitto_subscribe(mosq, NULL, topic, 0);
	if(rc) {
		fprintf(stderr, "Subscribe failed with code: %d.\n", rc);
		return EXIT_FAILURE;
	}

	// init ui 
	data->ui = hacsc_win_create();
	data->sensor = malloc(sizeof(hacsc_sensor_t));
	data->mosq = mosq;
	data->display_enabled = is_display_enabled();

	pthread_t ui_thread;
	if (pthread_create(&ui_thread, NULL, ui_loop, data) != 0) {
		fprintf(stderr, "Error creating ui loop\n");
		return EXIT_FAILURE;
	}

	mosquitto_loop_forever(mosq, -1, 1); 

	// clean up
	pthread_join(ui_thread, NULL);
	mosquitto_destroy(mosq);
	hacsc_win_destroy(data->ui);
	free(data->sensor);
	free(data);

	return EXIT_SUCCESS;
}
