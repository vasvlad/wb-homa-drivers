#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_w1.h"
#include <bitset>

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

TMaybe<char> TSysfsOnewireDevice::ReadOutput(int channel_number) const
{
    bool flag=false;

    unsigned char c = 0;
    unsigned char result = 0;
    std::ifstream file;
//    std::string fileName=DeviceDir +"/state";
    std::string fileName=DeviceDir +"/output";
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

TMaybe<char> TSysfsOnewireDevice::ReadState(int channel_number) const
{
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

void TSysfsOnewireDevice::WriteOutput(int channel_number, int value)
{
    unsigned char c = 0;
    unsigned char c1 = 0;
    unsigned char c2 = 0;
    int attempt = 3;
    while (attempt >=0){
        std::fstream iofile;
        std::string fileName=DeviceDir +"/output";
        iofile.open(fileName.c_str(), std::ios_base::in|std::ios_base::binary);
        if (iofile.is_open()) {
    	    c = iofile.get();
            c1 = c;
        }else
            return;
        iofile.close();
        iofile.open(fileName.c_str(), std::ios_base::out|std::ios_base::binary);
        cout << "C before = " << (bitset<8>) c << endl; 
        if (value == 0)
            c &= ~(1<<channel_number);
        else
            c |= (1<<channel_number);
    
        cout << "C after = " << (bitset<8>) c << endl; 
        iofile.put(c);
        iofile.flush();
        iofile.close();
        iofile.open(fileName.c_str(), std::ios_base::in|std::ios_base::binary);
        if (iofile.is_open()) {
    	    c2 = iofile.get();
        }else
            return;
        iofile.close();
        if (c1 != c2)
            return;
        cout << "attempt"  << endl; 

        attempt--;
    }
    
}
void TSysfsOnewireDevice::SwitchLight(int output_bit_number, int state_bit_number, int on)
{
    unsigned char c = 0;
    unsigned char state_bit = 0;
    unsigned char output_bit = 0;

    std::string fileName;
    printf("TSysfsOnewireDevice::SwitchLight\n");
    std::fstream iofile;
    auto result = ReadState(state_bit_number);
    if (result.Defined()) {
        state_bit = *result;
    }else{
        return;
    }

    printf("State bit %i\n", (bool)state_bit);
    iofile.close();

    if (on == state_bit)
        return;

    fileName=DeviceDir +"/output";
    iofile.open(fileName.c_str(), std::ios_base::in|std::ios_base::binary);
    if (iofile.is_open()) {
	c = iofile.get();
    }else
        return;
    if ((c & (1<<output_bit_number))>0)
        output_bit = 1;
    else
        output_bit = 0;

    iofile.close();

//    iofile.open(fileName.c_str(), std::ios_base::out|std::ios_base::binary);
    printf("C1 before %i\n", c);
    cout << "C1 before = " << (bitset<8>) c << endl; 

    printf("Output bit %i\n", (bool)output_bit);
    if (on == 1){
        if (output_bit == 1) {
            WriteOutput(output_bit_number, 0); 

    	    printf("11111\n");
        }else{
            WriteOutput(output_bit_number, 0); 
            WriteOutput(output_bit_number, 1); 

    	    printf("22222\n");
        }
    }else{
        if (output_bit == 1) {
            WriteOutput(output_bit_number, 0); 
    	    printf("33333\n");
        }else{
            WriteOutput(output_bit_number, 1); 
            WriteOutput(output_bit_number, 0); 
            WriteOutput(output_bit_number, 1); 
    	    printf("4444\n");
        }
    }
    iofile.close();

    result = ReadState(state_bit_number);
    if (result.Defined()) {
        state_bit = *result;
    }else{
        return;
    }


    printf("State bit %i\n", (bool)state_bit);

}

void TSysfsOnewireManager::RescanBus()
{
}
