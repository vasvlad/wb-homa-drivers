#include <mosquittopp.h>
#include <fstream>
#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include "sysfs_adc.h"

using namespace std;

struct THandlerConfig
{
    std::string DeviceName = "Adcs";
    bool Debug = false;
    vector<TChannel> Channels;
};

class TMQTTAdcHandler : public TMQTTWrapper
{
public:
    explicit TMQTTAdcHandler(const TMQTTAdcHandler::TConfig& mqtt_config, const THandlerConfig handler_config) ;

    void OnConnect(int rc) ;
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsAdcChannel& channel) const;
    void UpdateChannelValues();
    virtual void UpdateValue() ;
private:
    THandlerConfig Config;
    vector<std::unique_ptr<TSysfsAdc>> AdcHandlers;
    vector<std::shared_ptr<TSysfsAdcChannel>> Channels;
};

