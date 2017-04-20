/* Logger.hh
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef LOGGER_H_
#define LOGGER_H_


#include <string>
#include <uhd/usrp/multi_usrp.hpp>

class Logger
{
	public:
		Logger(std::string file);
		void log(std::string msg);
        void log(std::string msg, uhd::usrp::multi_usrp::sptr uspr);
		void log_now(std::string msg);
        void log_now(std::string msg, uhd::usrp::multi_usrp::sptr uspr);
		void write_log();
	private:
		std::string log_string;
		std::string filename;
		bool write_pending;
};


#endif // LOGGER_H_
