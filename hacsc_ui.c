#include "hacsc_ui.h"
#include "hacsc_sensor.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <string.h>
#include <limits.h>
#include <sys/sysinfo.h>

#define STATUS_GREEN 8
#define STATUS_YELLOW 9
#define STATUS_RED 10

#define COL_LEFT        0
#define ROW_POWER_TOTAL 1
#define ROW_POWER_DC1   2
#define ROW_POWER_DC2   3
#define ROW_TEMPERATURE 5
#define ROW_SUNRISE     6
#define ROW_SUNSET      7
#define ROW_DATE        8
#define ROW_UPTIME      9
#define ROW_CPU         10
#define ROW_IP          11

#define CPU_TEMPERATURE_PATH "/sys/devices/virtual/thermal/thermal_zone0/temp"

static void refresh_time(hacsc_win_t *ui);
static void refresh_uptime(hacsc_win_t *ui);
static void refresh_ip(hacsc_win_t *ui, const char *device_name);
static char* get_ip(const char *device_name);
static void refresh_cpu_temperature(hacsc_win_t *ui);
static int get_cpu_temperature();
static void refresh_sensor(hacsc_win_t *ui, hacsc_sensor_t *sensor);

hacsc_win_t* hacsc_win_create() 
{    
	initscr();
	cbreak();
	noecho();
	curs_set(0);

	if (has_colors()) {
		start_color();
		init_pair(STATUS_GREEN, COLOR_GREEN, COLOR_BLACK);
		init_pair(STATUS_YELLOW, COLOR_YELLOW, COLOR_BLACK);
		init_pair(STATUS_RED, COLOR_RED, COLOR_BLACK);
	}

	hacsc_win_t *ui = malloc(sizeof(hacsc_win_t));

	int rows, cols;
	ui->mainwindow = newwin(12, 0, 0, 0);
	getmaxyx(ui->mainwindow, rows, cols);
	ui->rows = rows;
	ui->cols = cols;
	box(ui->mainwindow, 0, 0);

	return ui;
}

void hacsc_win_destroy(hacsc_win_t *ui) 
{
	if (ui == NULL) {
		return;
	}
	wclear(ui->mainwindow);
	wrefresh(ui->mainwindow);
	delwin(ui->mainwindow);
	free(ui);
	endwin();
}

void hacsc_win_refresh(hacsc_win_t *ui, hacsc_sensor_t *sensor, char *net_device) 
{
	if (!ui) {
		return;
	}
	wclear(ui->mainwindow);
	refresh_time(ui);
	refresh_uptime(ui);
	refresh_ip(ui, net_device);
	refresh_cpu_temperature(ui);
	refresh_sensor(ui, sensor);
	wrefresh(ui->mainwindow);
}

static void print_progress(hacsc_win_t *ui, int y, int x, int max_val, double cur_val, int bar_width) 
{
	double cur_progress = (cur_val / max_val) * 100.0;
	int num_filled_bars = (cur_progress * bar_width) / 100;

	int treshold_yellow = (40 * bar_width) / 100;
	int treshold_red = (70 * bar_width) / 100;

	wmove(ui->mainwindow, y, x);
	waddch(ui->mainwindow, '[');

	int active_color = STATUS_GREEN;
	wattron(ui->mainwindow, COLOR_PAIR(active_color));
	for (int i = 0; i < num_filled_bars; i++) {
		if (i >= treshold_red) {
			wattroff(ui->mainwindow, COLOR_PAIR(active_color));
			active_color = STATUS_RED;
			wattron(ui->mainwindow, COLOR_PAIR(active_color));
		} else if (i >= treshold_yellow) {
			wattroff(ui->mainwindow, COLOR_PAIR(active_color));
			active_color = STATUS_YELLOW;
			wattron(ui->mainwindow, COLOR_PAIR(active_color));
		}
		waddch(ui->mainwindow, '|');
	}
	wattroff(ui->mainwindow, COLOR_PAIR(active_color));
	for (int i = num_filled_bars; i < bar_width; i++) {
		waddch(ui->mainwindow, ' ');
	}
	wprintw(ui->mainwindow, "] %5.1f%%", cur_progress);
}

static struct tm* to_localtime(struct tm utc) 
{
	// Get the current system's timezone information
	time_t now = time(NULL);
	struct tm *local_time = localtime(&now);

	// Set the timezone in the parsed struct tm to the local timezone
	utc.tm_isdst = local_time->tm_isdst; 
	utc.tm_gmtoff = local_time->tm_gmtoff;

	time_t tstamp = mktime(&utc);
	return localtime(&tstamp);
}

static void refresh_sensor(hacsc_win_t *ui, hacsc_sensor_t *sensor) 
{
	int bar_width = 7;
	int progress_x = 16;

	mvwprintw(ui->mainwindow, ROW_POWER_TOTAL, COL_LEFT, "Total: %5.1f W", sensor->inverter_power_ac.value);
	print_progress(ui, ROW_POWER_TOTAL, progress_x, 800, sensor->inverter_power_ac.value, bar_width);

	mvwprintw(ui->mainwindow, ROW_POWER_DC1, COL_LEFT, "DC 1:  %5.1f W", sensor->inverter_power_dc1.value);
	print_progress(ui, ROW_POWER_DC1, progress_x, 400, sensor->inverter_power_dc1.value, bar_width);

	mvwprintw(ui->mainwindow, ROW_POWER_DC2, COL_LEFT, "DC 2:  %5.1f W", sensor->inverter_power_dc2.value);
	print_progress(ui, ROW_POWER_DC2, progress_x, 400, sensor->inverter_power_dc2.value, bar_width);

	mvwprintw(ui->mainwindow, ROW_TEMPERATURE, COL_LEFT, "Temp:    %.1f °C / %.1f °C", sensor->inverter_temp.value, sensor->weather_temperature.value);

	if (sensor->sun_next_sunrise.value.tm_mday == 0) { 
		mvwprintw(ui->mainwindow, ROW_SUNRISE, COL_LEFT, "Sunrise: N/A");
	} else {
		struct tm *sr =to_localtime(sensor->sun_next_sunrise.value);
		mvwprintw(ui->mainwindow, ROW_SUNRISE, COL_LEFT, "Sunrise: %02d:%02d:%02d", sr->tm_hour, sr->tm_min, sr->tm_sec);
	}
	if (sensor->sun_next_sunset.value.tm_mday == 0) { 
		mvwprintw(ui->mainwindow, ROW_SUNSET, COL_LEFT, "Sunset:  N/A");
	} else {
		struct tm *ss =to_localtime(sensor->sun_next_sunset.value);
		mvwprintw(ui->mainwindow, ROW_SUNSET, COL_LEFT, "Sunset:  %02d:%02d:%02d", ss->tm_hour, ss->tm_min, ss->tm_sec);
	}
}

static void refresh_ip(hacsc_win_t *ui, const char *device_name) 
{
	if (!device_name) {
		return;
	}
	char *ip = get_ip(device_name);
	if (!ip) {
		mvwprintw(ui->mainwindow, ROW_IP, COL_LEFT, "IP:      N/A");
	} else {
		mvwprintw(ui->mainwindow, ROW_IP, COL_LEFT, "IP:      %s", ip);
	}
}

static char* get_ip(const char *device_name) 
{
	struct ifaddrs *ifap, *ifa;
	getifaddrs (&ifap);

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			if (strcmp(ifa->ifa_name, device_name) == 0) {
				struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
				char *result = inet_ntoa(sa->sin_addr);
				freeifaddrs(ifap);
				return result;
			}
		}
	}
	freeifaddrs(ifap);
	return NULL;
}

static void refresh_cpu_temperature(hacsc_win_t *ui) 
{
	int temp = get_cpu_temperature();
	if (temp == INT_MIN) {
		return;
	}
	mvwprintw(ui->mainwindow, ROW_CPU, COL_LEFT, "CPU:    %5.1f °C", (temp/1000.0));
}


static int get_cpu_temperature()
{
        FILE *fp = fopen(CPU_TEMPERATURE_PATH, "r");
        if (fp == NULL) {
                return false;
        }
        int t = INT_MIN;
        fscanf(fp, "%d", &t);
        fclose(fp);
	return t;
}


static void refresh_time(hacsc_win_t *ui) 
{
	time_t t = time(NULL);
	struct tm *now = localtime(&t);
	mvwprintw(ui->mainwindow, ROW_DATE, COL_LEFT, "Date:    %02d.%02d.%04d", now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);
	mvwprintw(ui->mainwindow, ROW_DATE, ui->cols-8, "%02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
}

static void refresh_uptime(hacsc_win_t *ui) 
{
	struct sysinfo sinfo;
	if (sysinfo(&sinfo) != 0) {
		mvwprintw(ui->mainwindow, ROW_UPTIME, 0, "Up:      N/A");
		return;
	}

	int seconds_per_day = 86400;
	int seconds_per_hour = 3600;
	int days = sinfo.uptime / seconds_per_day; 
	int hours = (sinfo.uptime % seconds_per_day) / seconds_per_hour; 
	int minutes = ((sinfo.uptime % seconds_per_day) % seconds_per_hour) / 60;

	char *buf;
	int written = 0;
	if (days == 0) {
		written = asprintf(&buf, "%2d:%2d", hours, minutes);
	} else if (days == 1) {
		written = asprintf(&buf, "%d day, %2d:%2d", days, hours, minutes);
	} else {
		written = asprintf(&buf, "%d days, %2d:%2d", days, hours, minutes);
	}

	if (written != -1) {
		mvwprintw(ui->mainwindow, ROW_UPTIME, 0, "Up:      %s", buf);
		free(buf);
	} else {
		mvwprintw(ui->mainwindow, ROW_UPTIME, 0, "Up:      N/A");
	}
}

