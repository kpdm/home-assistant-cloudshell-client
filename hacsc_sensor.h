#ifndef HACSC_SENSOR_H
#define HACSC_SENSOR_H

#include <time.h>

typedef struct hacsc_sensor_tm_data {
	struct tm value;
	struct tm last_updated;
} hacsc_sensor_tm_data_t;

typedef struct hacsc_sensor_num_data {
	double value;
	struct tm last_updated;
} hacsc_sensor_num_data_t;

typedef struct hacsc_sensor {
	hacsc_sensor_num_data_t inverter_power_ac;
	hacsc_sensor_num_data_t inverter_power_dc1;
	hacsc_sensor_num_data_t inverter_power_dc2;
	hacsc_sensor_num_data_t inverter_temp;
	hacsc_sensor_tm_data_t sun_next_sunrise;
	hacsc_sensor_tm_data_t sun_next_sunset;
} hacsc_sensor_t;

#endif /* HACSC_SENSOR_H */
