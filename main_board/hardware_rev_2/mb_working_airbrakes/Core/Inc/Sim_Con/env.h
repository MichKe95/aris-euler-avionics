#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#ifndef ENV_H_
#define ENV_H_

#define GRAVITATION 9.81
#define GAMMA 1.4
#define R_star 8.3144598
#define M 0.028964
#define R_0 R_star / M
#define T_0 273.15
#define T_GRAD 0.0065
#define TEMPERATURE_REFERENCE 15 // ISA reference temperature at sea level [K]
#define PRESSURE_REFERENCE 101325 // ISA reference pressure at sea level [Pa]

typedef struct {
    float p_g; // Pressure on ground level [Pa]
    float T_g; // Temperatur on ground level [K]

    float T; // Temperature [K]
    float C; // speed of sound
} env_t;

void init_env(env_t *env);

void calibrate_env(env_t *env, float p_g, float T_g);

void update_env(env_t *env, float T);

float mach_number(env_t *env, float V_x);

void pressure2altitudeAGL(env_t *env, int n, float p[n], bool p_active[n], float h[n]);

void altitudeAGL2pressure(env_t *env, int n, float h[n], bool h_active[n], float p[n]);

float altitude_gradient(env_t *env, float p);

#endif
