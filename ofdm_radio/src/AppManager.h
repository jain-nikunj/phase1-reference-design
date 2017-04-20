/* AppManager.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef APPMANAGER_H_
#define APPMANAGER_H_


#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

//U1 application headers
#include "Logger.hh"
#include "RadioHardwareConfig.h"
#include "RadioScheduler.h"


enum AppLogReportType {
    APP_LOG_REPORT_STARTED,
    APP_LOG_REPORT_INIT_DONE,
    APP_LOG_REPORT_SWITCHED_TO_NORMAL,
    APP_LOG_REPORT_SWITCHED_TO_ANTIJAM,
    APP_LOG_REPORT_SCHEDULE_COUNT,
    APP_LOG_REPORT_RUN_COMPLETE,
    APP_LOG_REPORT_FINALIZATION_DONE
};
//------------------------------------------------------------------------
class RadioHardwareConfig;

class AppManager
{
public:
    AppManager(
        float run_time, 
        bool debug
    );
    ~AppManager();
    float getRunTime();
    float getElapsedTime();
    int updateStatus();
    bool isContinuing();
    unsigned int getUpdateStatusCtr();
    int startAppEventTimer();
    float getAppEventTime();
    int logActualConfiguration(
        Logger *app_log_ptr,
        RadioHardwareConfig *rhc_ptr,
        RadioScheduler *rs_ptr
    );
    int doAppLogReport(
        Logger *app_log_ptr,
        AppLogReportType app_log_report
    );
    int setManualTerminationState(
		    bool specified_manual_termination_state
		    );
    bool getManualTerminationState();

    
private:
    float run_time;
    bool debug;
    bool run_time_is_enforced;
    bool manual_termination_requested;
    struct timeval app_runtime;
    struct timeval current_time;
    unsigned update_status_ctr;
    struct timeval app_event_time_begin;
    struct timeval app_event_time_end;
};


#endif // APPMANAGER_H_
