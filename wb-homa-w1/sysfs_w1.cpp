#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_w1.h"

bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second)
{
    return first.DeviceName == second.DeviceName;
}


TSysfsOnewireDevice::TSysfsOnewireDevice(const string& device_name)
    : DeviceName(device_name)
{
    //FIXME: fill family number

    if (StringStartsWith(device_name, "28-") or StringStartsWith(device_name, "10-")) 
        Family = TOnewireFamilyType::ProgResThermometer;
    else
        if (StringStartsWith(device_name, "29-")) 
            Family = TOnewireFamilyType::ProgResDS2408;
        else 
            Family = TOnewireFamilyType::Unknown;
    DeviceId = DeviceName;
    DeviceDir = SysfsOnewireDevicesPath + DeviceName;
}

TMaybe<float> TSysfsOnewireDevice::ReadTemperature() const
{
    std::string data;
    bool bFoundCrcOk=false;

    static const std::string tag("t=");

    std::ifstream file;
    std::string fileName=DeviceDir +"/w1_slave";
    file.open(fileName.c_str());
    if (file.is_open()) {
        std::string sLine;
        while (!file.eof()) {
            getline(file, sLine);
            size_t tpos;
            if (sLine.find("crc=")!=std::string::npos) {
                if (sLine.find("YES")!=std::string::npos) {
                    bFoundCrcOk=true;
                }
            } else if ((tpos=sLine.find(tag))!=std::string::npos) {
                data = sLine.substr(tpos+tag.length());
            }
        }
        file.close();
    }


    if (bFoundCrcOk) {
        int data_int = std::stoi(data);

        if (data_int == 85000) {
            // wrong read
            return NotDefinedMaybe;
        }

        if (data_int == 127937) {
            // returned max possible temp, probably an error
            // (it happens for chineese clones)
            return NotDefinedMaybe;
        }


        return (float) data_int/1000.0f; // Temperature given by kernel is in thousandths of degrees
    }

    return NotDefinedMaybe;
}

TMaybe<char> TSysfsOnewireDevice::ReadChannel(int channel_number) const
{
    std::string data;
    bool flag=false;

    unsigned char c = 0;
    unsigned char result = 0;
    std::ifstream file;
    std::string fileName=DeviceDir +"/state";
    file.open(fileName.c_str(), std::ifstream::binary);
    if (file.is_open()) {
	c = file.get();
        if (file.good() && c != 255) {
            if ((c & (1<<channel_number))>0)
                result = 1;
            else
                result = 0;
            flag = true;
        }
        file.close();
    }

    if (flag)
        return (char)result;
    else
        return NotDefinedMaybe;
}


void TSysfsOnewireManager::RescanBus()
{
}
