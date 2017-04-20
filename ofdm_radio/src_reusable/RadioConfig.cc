/* RadioConfig.cc
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include "RadioConfig.hh"
#include <cmath>
#include <getopt.h>
#include <stdlib.h>
#ifndef no_argument
#define no_argument                     0
#endif
#ifndef required_argument
#define required_argument               1
#endif
#ifndef optional_argument
#define optional_argument               2
#endif

using namespace std;
RadioConfig::RadioConfig(int argc, char **argv)
{
	//Load default values

	//Application Behavior
	config_file = "ofdm_default.cfg";
	debug = false;
	run_time = 180.0;
	app_log_file = "ofdm_app.log";
	rf_log_file = "ofdm_rf.log";
    alloc_log_file = "ofdm_allocation.log";
    packet_log_file = "ofdm_packets.log";
    using_tun_tap = true;
    u4 = true;

	//Radio Hardware Configuration
	radio_hardware = "USRP_N210";
	radio_hardware_clock = "CLOCK_REF_NONE";
	rf_gain_rx = 20.0;
	rf_gain_tx = 20.0;
	usrp_address_is_specified = false;
	usrp_address_name = "";

	//Waveform Configuration
	node_is_basestation = false;
	normal_freq = 2.5e9;
	sample_rate = 5.0e6;
    fdd_separation = 20e6;
	num_channels = 1;
    frame_size = 1024;
	fh_freq_min = 400.0e6;
	fh_freq_max = 4400.0e6;
	fh_prohibited_ranges = list<double>();
    num_fh_prohibited_ranges = 0;
    mitigation_timeout = 10.0;
    mitigation_reenable_timeout = 190.0;
    close_hole_timeout = 30.0;
    jamming_threshold = 50.0;
    hardened = false;
    uplink = true;


    slow = false;
	ofdma_tx_window = .025;
	mc_tx_window = .025;
    anti_jam = true;
    manual_mode = false;
    sba = 60.0;
    software_backoff = .1f;

    // After validation of fh_prohibited_ranges, if any, the following are updated
    // num_fh_prohibited_ranges, fh_prohibited_range_begin, fh_prohibited_range_end
	node_id = 1;
    // After validation of node_id, node_ip_address is generated
    // After validation of network_node_id the following are generated
    //   num_nodes_in_net, nodes_in_net, network_node_ip
    lookup_app_log_file = true;
    lookup_rf_log_file = true;
    lookup_alloc_log_file = true;
    lookup_packet_log_file = true;
    lookup_normal_freq = true;
    lookup_rf_gain_tx = true;
    lookup_rf_gain_rx = true;
    lookup_anti_jam = true;
    lookup_run_time = true;
	config_t cfg;
	//config_setting_t * cfg_setting = NULL;
	config_init(&cfg);

	//Optionally read in non-default config file or turn on debugging
	bool config_override_active = false;
	int c;
	static struct option long_options[] = {
		{"help",                no_argument,       0, 'h'},
		{"config-file",         required_argument, 0, 'C'},
		{"debug",		no_argument,	   0, 'd'},
		{"app_log_file",	required_argument, 0, 'a'},
		{"rf_log_file",		required_argument, 0, 'l'},
        {"alloc_log_file",  required_argument, 0, 'i'},
        {"packet_log_file", required_argument, 0, 'j'},
		{"normal_freq",		required_argument, 0, 'f'},
		{"rf_gain_tx", 		required_argument, 0, 't'},
		{"rf_gain_rx", 		required_argument, 0, 'r'},
		{"slow", 		no_argument,	   0, 's'},
		{"mc_tx_window",	required_argument, 0, 'z'},
		{"ofdma_tx_window",	required_argument, 0, 'y'},
        {"anti_jam_mode", no_argument,        0, 'n'},
        {"manual_mode",     no_argument,       0, 'm'},
		
	};
	int option_index = 0;

	while (1) 
	{
		c = getopt_long(argc, argv, "hC:da:l:f:t:r:y:z:b:g:e:i:j:nsm",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h' :
				showUserPrefsHelp();
				exit(EXIT_SUCCESS);
				break;
			case 'C' :
				config_file = optarg;
				if(! config_read_file(&cfg, config_file.c_str()) ) {
					fprintf(stderr, "ERROR: problem processing configuration file %s on line %d: %s\n", \
							config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
					config_destroy(&cfg);
					exit(EXIT_FAILURE);
				}
				config_override_active = true;
				break;
			case 'd' :
				debug = true;
				break;
			case 'a':
				app_log_file = optarg;
				lookup_app_log_file = false;
				break;
			case 'l':
				rf_log_file = optarg;
				lookup_rf_log_file = false;
				break;
			case 'f':
				normal_freq = atof(optarg);
				lookup_normal_freq = false;
				break;
			case 't':
				rf_gain_tx = atof(optarg);
				lookup_rf_gain_tx = false;
				break;
			case 'r':
				rf_gain_rx = atof(optarg);
				lookup_rf_gain_rx = false;
				break;
		        case 's':
				slow = true;
				break;
			case 'y':
				ofdma_tx_window = atof(optarg);
				break;
			case 'z':
				mc_tx_window = atof(optarg);
				break;
            case 'n':
                anti_jam = false;
                lookup_anti_jam = false;
                break;
            case 'm':
                manual_mode = true;
                break;
            case 'b':
                sba = atof(optarg);
                break;
            case 'g':
                software_backoff = atof(optarg);
                break;
            case 'e':
                run_time = atof(optarg);
                lookup_run_time = false;
                break;
            case 'i':
                alloc_log_file = optarg;
                lookup_alloc_log_file = false;
                break;
            case 'j':
                packet_log_file = optarg;
                lookup_packet_log_file = false;
                break;
			default :
				fprintf(stderr,"WARNING: Unrecognized option %s ignored \n\n", optarg);
				showUserPrefsHelp();
				exit(EXIT_FAILURE);
				break;
		}
	}

	//Read values from config file
	double dtmp;
	int itmp;
	const char * stmp;
    config_setting_t *ptmp;
    int ctr;

	if ( !config_override_active ) {
		if(! config_read_file(&cfg, config_file.c_str()) ) {
			fprintf(stderr, "ERROR: problem processing configuration file %s on line %d: %s\n", \
					config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
			config_destroy(&cfg);
			exit(EXIT_FAILURE);
		}
	}

    if(lookup_run_time)
    {
        if( config_lookup_float(&cfg, "run_time", &dtmp) ) {
            run_time = dtmp;
        }
    }
    if( config_lookup_int(&cfg, "using_tun_tap", &itmp) ) {
        if(itmp == 1)
            using_tun_tap = true;
        else
            using_tun_tap = false;
    }
    
    if( config_lookup_int(&cfg, "u4", &itmp) ) {
        if(itmp == 1)
            u4 = true;
        else
            u4 = false;
    }

    if( config_lookup_int(&cfg, "hardened", &itmp) ) {
        if(itmp == 1)
            hardened = true;
        else
            hardened = false;
    }

    if(lookup_app_log_file)
    {
	    if( config_lookup_string(&cfg, "app_log_file", &stmp) ) {
		    app_log_file = string(stmp);
	    }
    }
    if(lookup_alloc_log_file)
    {
	    if( config_lookup_string(&cfg, "alloc_log_file", &stmp) ) {
		    alloc_log_file = string(stmp);
	    }
    }
    if(lookup_rf_log_file)
    {
	    if( config_lookup_string(&cfg, "rf_log_file", &stmp) ) {
		    rf_log_file = string(stmp);
	    }
    }
    if(lookup_packet_log_file)
    {
        if( config_lookup_string(&cfg, "packet_log_file", &stmp) ) {
            packet_log_file = string(stmp);
        }
    }
    if( config_lookup_string(&cfg, "radio_hardware", &stmp) ) {
        radio_hardware = string(stmp);
	}
	
    if( config_lookup_string(&cfg, "radio_hardware_clock", &stmp) ) {
        radio_hardware_clock = string(stmp);
    }
    if(lookup_rf_gain_rx){
	    if( config_lookup_float(&cfg, "rf_gain_rx", &dtmp) ) {
		    rf_gain_rx = dtmp;
	    }
    }
    if(lookup_rf_gain_tx){
	    if( config_lookup_float(&cfg, "rf_gain_tx", &dtmp) ) {
		    rf_gain_tx = dtmp;
	    }
    }

    if( config_lookup_string(&cfg, "usrp_address_name", &stmp) ) {
	    usrp_address_is_specified = true;
	    usrp_address_name = stmp;
    }

    if( config_lookup_int(&cfg, "node_is_basestation", &itmp) ) {
	    if (itmp == 1) {
		    node_is_basestation = true;
	    } else {
		    node_is_basestation = false;
	    }
    }

    if(lookup_anti_jam)
    {
        if(config_lookup_int(&cfg, "anti_jam_mode", &itmp) ) 
        {
            if(itmp == 1) {
                anti_jam = true;
            } else {
                anti_jam = false;
            }
        }
    }

    if(config_lookup_int(&cfg, "uplink", &itmp) )
    {
        if(itmp == 1)
        {
            uplink = true;
        }
        else
        {
            uplink = false;
        }
    }
    if(lookup_normal_freq)
    { 
	    if( config_lookup_float(&cfg, "normal_freq", &dtmp) ) {
		    normal_freq = dtmp;
	    }
    }
    if( config_lookup_float(&cfg, "sample_rate", &dtmp) ) {
        sample_rate = dtmp;
    }
    if( config_lookup_float(&cfg, "fdd_separation", &dtmp) ) {
        fdd_separation = dtmp;
    }
    if( config_lookup_float(&cfg, "mitigation_timeout", &dtmp) ) {
        mitigation_timeout = dtmp;
    }
    if( config_lookup_float(&cfg, "mitigation_reenable_timeout", &dtmp) ) {
        mitigation_reenable_timeout = dtmp;
    }
    if( config_lookup_float(&cfg, "close_hole_timeout", &dtmp) ) {
        close_hole_timeout = dtmp;
    }
    if( config_lookup_float(&cfg, "jamming_threshold", &dtmp) ) {
        jamming_threshold = dtmp;
    }

    if( config_lookup_int(&cfg, "num_channels", &itmp) ) {
	    num_channels = (unsigned int)itmp;
    }

    if( config_lookup_int(&cfg, "frame_size", &itmp) ) {
	    frame_size = (unsigned int)itmp;
    } 
    if( config_lookup_float(&cfg, "fh_freq_min", &dtmp) ) {
        fh_freq_min = dtmp;
    }
    
    if( config_lookup_float(&cfg, "fh_freq_max", &dtmp) ) {
        fh_freq_max = dtmp;
    }
    
    ptmp = config_lookup(&cfg, "fh_prohibited_ranges");
    if(ptmp != NULL) {
        for (ctr = 0; ctr < config_setting_length(ptmp); ctr++) {
            dtmp = config_setting_get_float_elem(ptmp, ctr);
            fh_prohibited_ranges.push_back(dtmp);
        }
    }
    
	if( config_lookup_int(&cfg, "node_id", &itmp) ) {
		node_id = (unsigned char)itmp;
	}

	if( config_lookup_string(&cfg, "node_ip_address", &stmp) ) {
		node_ip_address = string(stmp);
	}

    ptmp = config_lookup(&cfg, "network_node_id");
    if(ptmp != NULL) {
        for (ctr = 0; ctr < config_setting_length(ptmp); ctr++) {
            itmp = config_setting_get_int_elem(ptmp, ctr);
            network_node_id.push_back((unsigned char)itmp);
        }
    }
    
    // Validate optional list of prohibited frequencies
    // Check for *pairs* of prohibited frequencies
    num_fh_prohibited_ranges = floor(fh_prohibited_ranges.size()/2);
    if (num_fh_prohibited_ranges > 0) {
        if( ((float)fh_prohibited_ranges.size()/2.0) != num_fh_prohibited_ranges) {
            cerr << "ERROR:" <<endl;
            cerr << "       The list of fh_prohibited_ranges must be in pairs";
            cerr << " but the length is: "<<dec <<fh_prohibited_ranges.size();
            cerr << "\n" <<endl;
            exit(EXIT_FAILURE);
        }
    }
    if (num_fh_prohibited_ranges > 0) {
        fh_prohibited_range_begin = new double[num_fh_prohibited_ranges];
        fh_prohibited_range_end = new double[num_fh_prohibited_ranges];
        unsigned int pair_ctr = 0;
        for ( list<double>::iterator it = fh_prohibited_ranges.begin(); 
                it != fh_prohibited_ranges.end(); it++) {
            
            fh_prohibited_range_begin[pair_ctr] = *it;
            it++;
            fh_prohibited_range_end[pair_ctr] = *it;
            pair_ctr++;
        }
    }
    
    // Validate node ID and set of radio net node IDs
    if ((node_id < 1) || (node_id >99)) {
        cerr << "ERROR: In configuration file setting for node_id "<< endl; 
        cerr << "       node_id must be in range [1, 99], inclusive" << endl;
        exit(EXIT_FAILURE);
    }
    node_ip_address = "10.10.10." + std::to_string(node_id);
    
    num_nodes_in_net = network_node_id.size();
    if (num_nodes_in_net < 2) {
	    cerr << "ERROR: In configuration file setting for node id list "<< endl; 
	    cerr << "       Need at least two nodes in a radio net" << endl;
	    exit(EXIT_FAILURE);
    } 
    nodes_in_net = new unsigned char[num_nodes_in_net];
    
    bool node_is_in_net = false;
    unsigned int node_ctr = 0;
    list<unsigned char>::iterator it;
    for (it = network_node_id.begin(); it != network_node_id.end(); it++) {
        if ((*it < 0) || (*it > 99)) {
            cerr << "ERROR: In configuration file setting for node id list "<< endl; 
            cerr << "       all node IDs must be in range [0, 99], inclusive" << endl;
            cerr << "       where 0 is is a reserved broadcast/test id" << endl;
            exit(EXIT_FAILURE);
        } else {
            if (*it == node_id) {
                node_is_in_net = true;
            }
            nodes_in_net[node_ctr] = *it;
            node_ctr++;
            network_node_ip.push_back( "10.10.10." + std::to_string(*it));
        }
    }
    if ( !node_is_in_net ) {
        cerr << "ERROR: In configuration file setting for node IDs"<< endl; 
        cerr << "       the node_id is not in the list of network_node_id" << endl;
        exit(EXIT_FAILURE);
    }
    
    
}


RadioConfig::~RadioConfig()
{
    if (num_fh_prohibited_ranges > 0) {
        delete[] fh_prohibited_range_begin;
        delete[] fh_prohibited_range_end;
    }   
    
    if (num_nodes_in_net > 0) {
        delete[] nodes_in_net;
    }
}


int RadioConfig::showUserPrefsHelp() {

	printf("\n");
	printf("USAGE:\n  ofdm_reference [OPTIONS]\n");
	printf("\n");
	printf("This program supports specifying options either with a single \n");
	printf("hyphen followed by a single letter, then whitespace and then the\n");
	printf("parameter's value, or a double hyphen followed by a full option name,\n");
	printf("either whitespace or an equals = character, and then the parameter\n");
	printf("value.  These two styles of specification are known as \"short\" \n");
	printf("and \"long\" option styles, respectively.  Both tolerate, but do not\n");
	printf("require, double quotes around the names of files. Note that all\n");
	printf("options specified on the command line override any default setting\n");
	printf("or any setting in a configuration file. \n");
	printf("\nOPTIONS:\n");
	printf("  -h, --help                    Show this message on usage\n");
	printf("  -C, --config-file             Filename of parameter definitions \n");
	printf("                                [default:\"U1_default.cfg\"]\n");
    printf("  -d --debug                    Print a line for every received packet\n");
    printf("  -a --app_log_file             Location to save the app log file\n");
    printf("  -l --rf_log_file              Location to save the rf log file\n");
    printf("  -i --alloc_log_file              Location to save the rf log file\n");
    printf("  -j --packet_log_file          Location to save the packet log file\n");
    printf("  -f --normal_freq              Center frequency of basestation. Mobiles use normal_freq + 20MHz\n");
    printf("  -t --rf_gain_tx               Transmission gain in db\n");
    printf("  -r --rf_gain_rx               Reception gain in db\n");
    printf("  -g                            Change the software backoff\n");
    printf("  -b                            Adjusts stop band attentuation of resampler\n");
    printf("  -e                            Change the radio's total runtime\n");
    printf("  -n                            Disable anti-jam mode\n");
	printf("EXAMPLE: \n");
	printf("The following example assumes the program, ofdm_reference, and the \n");
	printf("output files are in all in the current working directory, ./ \n\n");
	printf("This example instructs the program to use configuration in \"my.cfg\"\n");
	printf(" ./ofdm_reference --config-file ./my.cfg \n");
	printf("\n\n");

	return(EXIT_SUCCESS);
}

void RadioConfig::display_config()
{

    cout << "RadioConfig settings" << endl;
    cout << "-------------------------------------------" << endl;
    cout << "Application Behavior:" << endl;
	cout << "  config_file:                 " << config_file << endl;
	cout << "  debug:                       " << debug << endl;
	cout << "  run_time:                    " << run_time << endl;
	cout << "  app_log_file:                " << app_log_file << endl;
	cout << "  rf_log_file:                 " << rf_log_file << endl;
	cout << "  alloc_log_file:              " << alloc_log_file << endl;
	cout << "  packet_log_file:             " << packet_log_file << endl;
    cout << " " << endl;
    cout << "Radio Hardware Configuration:" << endl;
	cout << "  radio_hardware:              " << radio_hardware << endl;
    cout << "  radio_hardware_clock:        " << radio_hardware_clock << endl;
	cout << "  rf_gain_rx:                  " << rf_gain_rx << endl;
	cout << "  rf_gain_tx:                  " << rf_gain_tx << endl;
    if (usrp_address_is_specified) {
        cout << "  USRP address:              ";
        cout << usrp_address_name << endl;
    } else {
        cout << "  (no USRP address)" << endl;
    }
    cout << " " << endl;
    cout << "Waveform Configuration:" << endl;
    cout << "  node_is_basestation:         " << node_is_basestation << endl;
	cout << "  normal_freq:                 " << normal_freq << endl;
	cout << "  sample_rate:                 " << sample_rate << endl;
    cout << "  fdd_separation:              " << fdd_separation << endl;
	cout << "  node_id:                     " << (int)node_id << endl;
	cout << "  node_ip_address:             " << node_ip_address << endl;
    cout << "  anti_jam_mode:               " << anti_jam << std::endl;
    cout << "  hardened:                    " << hardened << std::endl;
    cout << "  uplink:                      " << uplink << std::endl;
    cout << "  frame_size:                  " << frame_size << std::endl;
    cout << "  mitigation_timeout:          " << mitigation_timeout << std::endl;
    cout << "  mitigation_reenable_timeout: " << mitigation_reenable_timeout << std::endl;
    cout << "  close_hole_timeout:          " << close_hole_timeout << std::endl;
    cout << "  jamming_threshold:           " << jamming_threshold << "kbps" << std::endl;

    cout << "  radio net node IDs (hex):    ";
    list<unsigned char>::iterator it;
    for (it = network_node_id.begin(); it != network_node_id.end(); it++) {
        cout << dec << (int)*it << ", ";
    }
    cout << " " << endl;

    cout << "  corresponding node IP addresses:\n    ";
    for (it = network_node_id.begin(); it != network_node_id.end(); it++) {
	   cout << "10.10.10." + std::to_string(*it) +",  ";
    }
    cout << " " << endl;
    cout << "-------------------------------------------\n" << endl;
    
}
