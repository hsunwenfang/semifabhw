
#include "hal.h"
#include "data.h"
#include "logger.h"
#include "chamber.h"
#include "controller.h"
#include <iostream>
#include <csignal>
#include <atomic>



struct Task
{
    void(*run)();
    data::TimeRep runtime;
    data::TimeRep last_ran;
};

// Wrappers match void() signature
Chamber ch;
Controller ctl;
data::TempC reading;
data::TempC adj;
void sensor_read()    { reading = ch.measure(); }
void pid_update()     { adj = ctl.update_compute(reading); }
void actuator_write() { ch.adjust(adj); }

// Now they fit in the task table
Task tasks[] = {
    {sensor_read,    10, 0},
    {pid_update,     10, 0},
    {actuator_write, 10, 0},
};

class scheduler{
    // TODO
};