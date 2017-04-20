/* RadioConfig.hh
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef RADIOCONFIG_H_
#define RADIOCONFIG_H_

#include <iostream>
#include <list>
#include <string>
#include <libconfig.h>
class RadioConfig
{
	public:
		RadioConfig(int argc, char **argv);
        ~RadioConfig();                
		int showUserPrefsHelp();	
		void display_config();

		//Application Behavior
		std::string config_file;
		bool debug;
		float run_time;
		std::string app_log_file;
		std::string rf_log_file;
        std::string alloc_log_file;
        std::string packet_log_file;
        bool using_tun_tap;
        bool u4;
        bool hardened;
        bool uplink;
 
		//Radio Hardware Configuration
        std::string radio_hardware;
        std::string radio_hardware_clock;
        double rf_gain_rx;
        double rf_gain_tx;
        bool usrp_address_is_specified;
        std::string usrp_address_name;

		//Waveform Configuration
        bool node_is_basestation;
        double normal_freq;
        double sample_rate;
        double fdd_separation;
        unsigned int num_channels;
        unsigned int frame_size;
        double fh_freq_min;
        double fh_freq_max;
        std::list<double> fh_prohibited_ranges;
        unsigned int num_fh_prohibited_ranges;
        double *fh_prohibited_range_begin;
        double *fh_prohibited_range_end;

        double mitigation_timeout;
        double mitigation_reenable_timeout;
        double close_hole_timeout;
        double jamming_threshold;
        
        unsigned char node_id;
        std::string node_ip_address;
        std::list<unsigned char> network_node_id;
        unsigned int num_nodes_in_net;
        unsigned char* nodes_in_net;
        std::list<std::string> network_node_ip;

        //Booleans to override config file setting with command line flag
        bool lookup_app_log_file;
        bool lookup_rf_log_file;
        bool lookup_alloc_log_file;
        bool lookup_packet_log_file;
        bool lookup_normal_freq;
        bool lookup_rf_gain_tx;
        bool lookup_rf_gain_rx;
        bool lookup_anti_jam;
        bool lookup_run_time;

	bool slow;
    bool anti_jam;
	float ofdma_tx_window;
	float mc_tx_window;
    bool manual_mode;
    float sba;
    float software_backoff;

};

#endif // RADIOCONFIG_H_
