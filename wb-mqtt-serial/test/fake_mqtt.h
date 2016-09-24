#pragma once
#include <set>
#include <list>
#include <wbmqtt/mqtt_wrapper.h>
#include "testlog.h"

class TFakeMQTTClient: public TMQTTClientBase
{
public:
    TFakeMQTTClient(const std::string& id, TLoggedFixture& fixture);
    void Connect();
    int Publish(int *mid,
                const std::string& topic,
                const std::string& payload = "",
                int qos = 0,
                bool retain = false);
    int DoPublish(bool external,
                  int *mid,
                  const std::string& topic,
                  const std::string& payload = "",
                  int qos = 0,
                  bool retain = false);
    int Subscribe(int *mid, const std::string& sub, int qos = 0);
    std::string Id() const { return MQTTId; }

private:
    std::string MQTTId;
    bool Connected;
    TLoggedFixture& Fixture;
    std::set<std::string> Subscriptions;
};

typedef std::shared_ptr<TFakeMQTTClient> PFakeMQTTClient;
