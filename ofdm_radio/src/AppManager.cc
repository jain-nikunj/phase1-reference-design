/* AppManager.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include "AppManager.h"

using namespace std;

AppManager::AppManager(float run_time, bool debug)
{
    this->run_time = run_time;
    this->debug = debug;

    if (run_time >= 0.0) {
        run_time_is_enforced = true;
    } else {
        run_time_is_enforced = false;
    }
    manual_termination_requested = false;
    if (gettimeofday(&app_runtime, NULL) != 0) {
        cerr << "ERROR: AppManager unable to access system time." << endl; 
        exit(EXIT_FAILURE);
    }

    update_status_ctr = 0;
}
//////////////////////////////////////////////////////////////////////////


AppManager::~AppManager()
{   
}
//////////////////////////////////////////////////////////////////////////


float AppManager::getRunTime()
{
    return(run_time);
}
//////////////////////////////////////////////////////////////////////////


float AppManager::getElapsedTime()
{
    if (gettimeofday(&current_time, NULL) != 0) {
        cerr << "ERROR: AppManager unable to access system time." << endl; 
        exit(EXIT_FAILURE);
    }
    return(  (float)(current_time.tv_sec - app_runtime.tv_sec) + \
            1.0E-6 * (float)(current_time.tv_usec - app_runtime.tv_usec)  );
}
//////////////////////////////////////////////////////////////////////////


int AppManager::startAppEventTimer()
{
    if (gettimeofday(&app_event_time_begin, NULL) != 0) {
        cerr << "ERROR: AppManager unable to access system time." << endl; 
        exit(EXIT_FAILURE);
    }
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


float AppManager::getAppEventTime()
{
    if (gettimeofday(&app_event_time_end, NULL) != 0) {
        cerr << "ERROR: AppManager unable to access system time." << endl; 
        exit(EXIT_FAILURE);
    }
    return(  (float)(app_event_time_end.tv_sec - app_event_time_begin.tv_sec) + \
            1.0E-6 * (float)(app_event_time_end.tv_usec - app_event_time_begin.tv_usec)  );
}
//////////////////////////////////////////////////////////////////////////


int AppManager::updateStatus()
{   
    //PLACEHOLDER for other checks, administrative tasks, etc.

    update_status_ctr++;
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


bool AppManager::isContinuing() 
{  
	if (manual_termination_requested) {
		return(false);
	}

    if (run_time_is_enforced) {
        if(getElapsedTime() < run_time) { 
            return(true);
        } else {
            return(false);
        }
        
    } else {
        //PLACEHOLDER for other checks
        return(true);
    }
}
//////////////////////////////////////////////////////////////////////////


unsigned int AppManager::getUpdateStatusCtr()
{
    return(update_status_ctr);
}
//////////////////////////////////////////////////////////////////////////


int AppManager::logActualConfiguration(
        Logger *app_log_ptr, 
        RadioHardwareConfig *rhc_ptr,
        RadioScheduler *rs_ptr
    )
{
    std::stringstream report;
    
    report << scientific << getElapsedTime() << "    AppManager: ";
    report << "UHD manual receive tuning bug ";
    if ( rhc_ptr->isUhdRxTuningBugPresent() ) {
        report << "is detected; workaround flag is TRUE.";
    } else {
        report << "is not detected; workaround flag is FALSE.";
    }
    report << "\n";
    
    report << scientific << getElapsedTime() << "    AppManager: ";
    report << "Radio medium tuning range [Hz] = ";
    report << scientific << rhc_ptr->getFhWindowMedium();
    report << "\n";
    
    report << scientific << getElapsedTime() << "    AppManager: ";
    report << "Frequency hop rate for 1 frame per hop [hops/sec] = ";
    report << dec << rs_ptr->getFhTaskSchedSize();
    report << "\n";

    report << scientific << getElapsedTime() << "    AppManager: ";
    report << "Frequency hop interdwell spacing [sec] = ";
    report << scientific << rs_ptr->getFhInterdwellSpacing();
    
    app_log_ptr->log(report.str());     
    app_log_ptr->write_log();
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////


int AppManager::doAppLogReport(
        Logger *app_log_ptr,
        AppLogReportType app_log_report
    )
{
    std::stringstream report;

    report << scientific << getElapsedTime();
    report << "    AppManager: ";
    
    switch (app_log_report) {
        case APP_LOG_REPORT_STARTED :
            report << "Application log started.";
            break;
        
        case APP_LOG_REPORT_INIT_DONE :
            report << "Application initialization complete.";
            break;
    
        case APP_LOG_REPORT_SWITCHED_TO_NORMAL :
            report << "Application switched to normal communications mode.";
            break;
            
        case APP_LOG_REPORT_SWITCHED_TO_ANTIJAM :
            report << "Application switched to anti-jam communications mode.";
            break;
            
        case APP_LOG_REPORT_SCHEDULE_COUNT :
            report << "Application completed scheduled tasks in batch: ";
            report << dec << getUpdateStatusCtr();
            break;
            
        case APP_LOG_REPORT_RUN_COMPLETE :
            report << "Application reached end of user-specified run conditions.";
            break;
            
        case APP_LOG_REPORT_FINALIZATION_DONE :
            report << "Application finalized and terminating normally.";
            break;
            
        default :
            cerr << "\nERROR in AppManager::doAppLogReport -- ";
            cerr << "Unknown type of app_log_report\n" << endl;
            exit(EXIT_FAILURE);
            break;
    }
    
    app_log_ptr->log(report.str());
    app_log_ptr->write_log();
    
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////
bool AppManager::getManualTerminationState()
{
    return(manual_termination_requested);
}
//////////////////////////////////////////////////////////////////////////


int AppManager::setManualTerminationState(
        bool specified_manual_termination_state
    )
{
    manual_termination_requested = specified_manual_termination_state;
    return(EXIT_SUCCESS);
}
//////////////////////////////////////////////////////////////////////////

