/* Logger.cc 
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

#include <Logger.hh>
#include <string>
#include <iostream>
#include <fstream>
using namespace std;
Logger::Logger(string file)
	:filename(file), write_pending(false)
{
}

void Logger::log(string msg)
{
	log_string += msg;
	log_string += "\n";
	write_pending = true;
}

void Logger::log(string msg, uhd::usrp::multi_usrp::sptr usrp)
{
    log_string += msg;
    log_string += "\n";
    log_string += usrp->get_mboard_sensor("gps_gpgga").to_pp_string();
    log_string += "\n";
    write_pending = true;
}

void Logger::log_now(string msg)
{
	ofstream output;
	output.open(filename.c_str(), ios::app);
	output << msg << endl;
	output.close();
}

void Logger::log_now(string msg, uhd::usrp::multi_usrp::sptr usrp)
{
	ofstream output;
	output.open(filename.c_str(), ios::app);
	output << msg << endl;
    output << usrp->get_mboard_sensor("gps_gpgga").to_pp_string() << endl;
	output.close();
}

void Logger::write_log()
{
	if(write_pending)
	{
		ofstream output;
		output.open(filename.c_str(), ios::app);
		output << log_string;
		output.close();
		log_string = "";
		write_pending = false;
	}
}
