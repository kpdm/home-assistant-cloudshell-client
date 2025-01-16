#ifndef HACSC_UI_H
#define HACSC_UI_H

#include <ncurses.h>
#include "hacsc_sensor.h"

typedef struct hacsc_win {
	WINDOW *mainwindow;
	int rows;
	int cols;
} hacsc_win_t;

hacsc_win_t* hacsc_win_create();
void hacsc_win_refresh(hacsc_win_t *ui, hacsc_sensor_t *sensor, char *net_device);
void hacsc_win_destroy(hacsc_win_t *ui);

#endif /* HACSC_UI_H */
