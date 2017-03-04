#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "dirent.h"
#include <getopt.h>
#include <chrono>

#include <mosquittopp.h>

#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>



#include "sysfs_w1.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

class TMQTTOnewireHandler : public TMQTTWrapper
{
    //~ typedef pair<TGpioDesc, TSysfsGpio> TChannelDesc;

	public:
        TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config);
		~TMQTTOnewireHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);

        inline string GetChannelTopic(const TSysfsOnewireDevice& device);
        void RescanBus();

        void UpdateChannelValues();
        inline void SetPrepareInit(bool b) { PrepareInit = b; }// changing bool PrepareInit
        inline bool GetPrepareInit() { return PrepareInit; }
    private:
        vector<TSysfsOnewireDevice> Channels;
        bool PrepareInit;// needed for cleaning mqtt messages before start working
        string Retained_old;// we need some message to be sure, that we got all retained messages in starting

};




TMQTTOnewireHandler::TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config)
    : TMQTTWrapper(mqtt_config)
    , PrepareInit(true)
{
	Connect();

    Retained_old = string("/tmp/") + MQTTConfig.Id + "/retained_old";

}

TMQTTOnewireHandler::~TMQTTOnewireHandler() {}

void TMQTTOnewireHandler::OnConnect(int rc)
{
        printf("Connected with code %d.\n", rc);
	if(rc == 0){
                // Meta
        string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
        Publish(NULL, path, "1-wire Thermometers", 0, true);


        if (PrepareInit){
            string controls = string("/devices/") + MQTTConfig.Id + "/controls/+";
            Subscribe(NULL, controls);
            string controls_switches = string("/devices/") + MQTTConfig.Id + "/controls/+/+/light/on";
            Subscribe(NULL, controls_switches);
            Subscribe(NULL, Retained_old);
            Publish(NULL, Retained_old, "1", 0, false);
         }else{
            RescanBus();
         }
	}
}

template<typename T>
void UnorderedVectorDifference(const vector<T> &first, const vector<T>& second, vector<T> & result)
{
    for (auto & el_first: first) {
        bool found = false;
        for (auto & el_second: second) {
            if (el_first == el_second) {
                found = true;
                break;
            }
        }

        if (!found) {
            result.push_back(el_first);
        }
    }
}

void TMQTTOnewireHandler::RescanBus()
{
    vector<TSysfsOnewireDevice> current_channels;

    fprintf(stderr,"Rescan\n");
    DIR *dir;
    struct dirent *ent;
    string entry_name;
    if ((dir = opendir (SysfsOnewireDevicesPath.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            //printf ("%s\n", ent->d_name);
            entry_name = ent->d_name;
            if (StringStartsWith(entry_name, "28-") ||
                StringStartsWith(entry_name, "29-") ||
                StringStartsWith(entry_name, "10-") ||
                StringStartsWith(entry_name, "3a-") ||
                StringStartsWith(entry_name, "12-") ||
                StringStartsWith(entry_name, "22-") )
            {
                    current_channels.emplace_back(entry_name);
            }
        }
        closedir (dir);
    } else {
        cerr << "ERROR: could not open directory " << SysfsOnewireDevicesPath << endl;
    }

    vector<TSysfsOnewireDevice> new_channels;
    UnorderedVectorDifference(current_channels, Channels, new_channels);

    vector<TSysfsOnewireDevice> absent_channels;
    UnorderedVectorDifference(Channels, current_channels, absent_channels);


    Channels.swap(current_channels);

    for (const TSysfsOnewireDevice& device: new_channels) {
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResThermometer) 
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "temperature", 0, true);
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2408){
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "switch", 0, true);
        }  
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2413){
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "switch", 0, true);
        }  
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2406){
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "switch", 0, true);
        }  
    }

    //delete retained messages for absent channels
    for (const TSysfsOnewireDevice& device: absent_channels) {
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResThermometer){ 
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
            Publish(NULL, GetChannelTopic(device), "", 0, true);
        }
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2408){ 
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
            Publish(NULL, GetChannelTopic(device), "", 0, true);
        }
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2413){ 
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
            Publish(NULL, GetChannelTopic(device), "", 0, true);
        }
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2406){ 
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
            Publish(NULL, GetChannelTopic(device), "", 0, true);
        }

    }
}

void TMQTTOnewireHandler::OnMessage(const struct mosquitto_message *message)
{
    int light_output[7] = {5,4,3,2,4,5,4};
    int light_state[7]  = {7,0,6,1,3,2,6};

    printf("TMQTTOnewireHandler::OnMessage. %s\n", message->topic);
    string topic = message->topic;
    string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    if (topic == Retained_old) {// if we get old_message it means that we've read all retained messages
        Publish(NULL, Retained_old, "", 0, true);
        unsubscribe(NULL, Retained_old.c_str());
        unsubscribe(NULL, (controls_prefix + "+").c_str());
        PrepareInit = false;
    }else {
        string device = topic.substr(controls_prefix.length(), topic.length());
        // trim right part
        size_t startpos = device.find_first_of("/");
        if( string::npos != startpos )
            device = device.substr(0, startpos);
        string device_on = topic.substr(controls_prefix.length(), topic.length());
        printf("TMQTTOnewireHandler::OnMessage device %s\n", device.c_str());
        for (auto& current : Channels){
            if (device == current.GetDeviceId()){
                if (current.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2408){
                   size_t startpos2 = device_on.find("/light/on");
                   if (startpos2 == 24){ 
                        if( std::isdigit((char)device_on.c_str()[startpos2-1])) {
                            int light_number = std::stoi(device_on.substr(startpos2-1));
    			    string payload = static_cast<const char*>(message->payload);
			    printf (" Light number %i\n", light_number);
		            if (payload.size() >0){	
                                int param = std::stoi(payload);
                                printf(" payload %s %i\n", payload.c_str(), param);
                              //  current.WriteOutput(channel_number, param);
                                current.SwitchLight(light_output[light_number], light_state[light_number], param);
                            }
                        }
	            }
                }
                if (current.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2413){
			    printf (" Light number \n");
                   size_t startpos2 = device_on.find("/light/on");
                   if (startpos2 == 24){ 
                        if( std::isdigit((char)device_on.c_str()[startpos2-1])) {
                            int light_number = std::stoi(device_on.substr(startpos2-1));
    			    string payload = static_cast<const char*>(message->payload);
			    printf (" Light number %i\n", light_number);
		            if (payload.size() >0){	
                                int param = std::stoi(payload);
                                printf(" payload %s %i\n", payload.c_str(), param);
                              //  current.WriteOutput(channel_number, param);
                                current.SwitchLight3e(param);
                            }
                        }
	            }

                }
                return;
            }
        }
        printf("TMQTTOnewireHandler::OnMessage added device %s\n", device.c_str());
        Channels.emplace_back(device);
    }

}

void TMQTTOnewireHandler::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded.\n");
}

string TMQTTOnewireHandler::GetChannelTopic(const TSysfsOnewireDevice& device) {
    static string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    return (controls_prefix + device.GetDeviceId());
}

void TMQTTOnewireHandler::UpdateChannelValues() {

    for (TSysfsOnewireDevice& device: Channels) {
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResThermometer){ 
           if (((std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + rand() % 20 - device.GetPublicationTime())>100)){
               auto result = device.ReadTemperature();
               if (result.Defined()) {
                   float temperature = *result;
                   device.SetPublicationTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
                   if (temperature != 0.0)
                       Publish(NULL, GetChannelTopic(device), to_string(*result), 0, true); // Publish current value (make retained)
               }
           }
        }
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2408 || device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2413){ 
            unsigned char previous_result = device.GetStateByte();
            auto result = device.ReadStateByte();
            if (result.Defined()) {
               // fprintf(stderr, "Diff %i\n", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - device.GetPublicationTime());
                if (((std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + rand() % 20 - device.GetPublicationTime())>60) || (previous_result != (unsigned char)*result)){
		    device.SetStateByte(*result);
                    device.SetPublicationTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
		    for (int i=0; i<=7; i++){
			    unsigned char status = *result;
			    int result_bit = status & (1<<i);
			    if (result_bit > 0)
				result_bit = 1;
			    else
				result_bit = 0;
			    Publish(NULL, GetChannelTopic(device)+ "/channel"+ std::to_string(i) + "/state", std::to_string(result_bit), 0, true); // Publish current value (make retained)
		    }
                }
            }
/*
            for (int i=0; i<=7; i++){
                auto result = device.ReadState(i);
                if (result.Defined()) {
                    Publish(NULL, GetChannelTopic(device)+ "/channel"+ std::to_string(i) + "/state", std::to_string(*result), 0, true); // Publish current value (make retained)
                }
//                result = device.ReadOutput(i);
//                if (result.Defined()) {
//                    Publish(NULL, GetChannelTopic(device)+ "/channel"+ std::to_string(i) + "/output", std::to_string(*result), 0, true); // Publish current value (make retained)
//                }

            }
*/
        }
        if (device.GetDeviceFamily() == TOnewireFamilyType::ProgResDS2406){ 
            unsigned char previous_result = device.GetStateByte();
            auto result = device.ReadChannelAccessByte();
            if (result.Defined()) {
               // fprintf(stderr, "Diff %i\n", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - device.GetPublicationTime());
                if (((std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + rand() % 20 - device.GetPublicationTime())>60) || (previous_result != (unsigned char)*result)){
		    device.SetStateByte(*result);
                    device.SetPublicationTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
		    for (int i=0; i<=7; i++){
			    unsigned char status = *result;
			    int result_bit = status & (1<<i);
			    if (result_bit > 0)
				result_bit = 1;
			    else
				result_bit = 0;
			    Publish(NULL, GetChannelTopic(device)+ "/channel"+ std::to_string(i) + "/channel_access", std::to_string(result_bit), 0, true); // Publish current value (make retained)
		    }
                }
            }
        }
    }
}


int main(int argc, char *argv[])
{
	int rc;
    TMQTTOnewireHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    int poll_interval = 10 * 1000; //milliseconds
    std::time_t end_time_of_rescan = 0;
    int c;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:i:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
        //~ case 'c':
            //~ printf ("option c with value '%s'\n", optarg);
            //~ config_fname = optarg;
            //~ break;
        case 'p':
            printf ("option p with value '%s'\n", optarg);
            mqtt_config.Port = stoi(optarg);
            break;
        case 'h':
            printf ("option h with value '%s'\n", optarg);
            mqtt_config.Host = optarg;
            break;
        case 'i':
			//poll_interval = stoi(optarg) * 1000;
			poll_interval = stoi(optarg) * 100;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }
	mosqpp::lib_init();

    mqtt_config.Id = "wb-w1";

    std::shared_ptr<TMQTTOnewireHandler> mqtt_handler(new TMQTTOnewireHandler(mqtt_config));
    mqtt_handler->Init();
    string topic = string("/devices/") + mqtt_config.Id + "/controls/+";

    auto time_last_published = steady_clock::now();
    mqtt_handler->RescanBus();
    end_time_of_rescan = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    while(1){
        rc = mqtt_handler->loop(poll_interval);
        //~ cout << "break in a loop! " << rc << endl;
        if(rc != 0) {
            mqtt_handler->reconnect();
        } else {
            // update current values
            if (!mqtt_handler->GetPrepareInit()){
                int time_elapsed = duration_cast<milliseconds>(steady_clock::now() - time_last_published).count() ;
                if (time_elapsed >= poll_interval ) { //checking is it time to look through all gpios
                    if ((std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - end_time_of_rescan) > 600){
	                   mqtt_handler->RescanBus();
                           end_time_of_rescan = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    }
	                //mqtt_handler->RescanBus();
	            mqtt_handler->UpdateChannelValues();
	            time_last_published = steady_clock::now();
		}
            }
        }
    }

	mosqpp::lib_cleanup();

	return 0;
}
