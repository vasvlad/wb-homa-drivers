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
    int attempt = 3;
    unsigned char array[3] = {255, 255, 255}; 
    std::ifstream file;
//    std::string fileName=DeviceDir +"/state";
    std::string fileName=DeviceDir +"/output";
    while (attempt >=0){
        for (int i = 0; i<3; i++){
            file.open(fileName.c_str(), std::ifstream::binary);
            if (file.is_open()) {
        	c = file.get();
                if (file.good()) {
                    if ((c & (1<<channel_number))>0)
                        array[i] = 1;
                    else
                        array[i] = 0;
                    flag = true;
                }
                file.close();
            }
        }
        if (array[0] == array[1] && array[1] == array[2]){
           result = array[0];
           break;
        }
        cout << "Read output attempt"  << endl; 
        attempt--;
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
    unsigned char array[3] = {255, 255, 255}; 
    int attempt = 3;
    std::ifstream file;
    std::string fileName=DeviceDir +"/state";

    while (attempt >=0){
        for (int i = 0; i<3; i++){
            file.open(fileName.c_str(), std::ifstream::binary);
            if (file.is_open()) {
        	    c = file.get();
                if (file.good()) {
                    if ((c & (1<<channel_number))>0)
                        array[i] = 1;
                    else
                        array[i] = 0;
                    flag = true;
                }
                file.close();
            }else{
        	cout << "File state is not openned"  << endl; 
            }
        }
        if (array[0] == array[1] && array[1] == array[2]){
           result = array[0];
           break;
        }
    
        cout << "Read state attempt"  << endl; 
        attempt--;
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
    int read_attempt = 5;
    bool read_success = false;
    while (attempt >=0){
        std::fstream iofile;
        std::string fileName=DeviceDir +"/output";

        while (read_attempt >=0){
            unsigned char array[3] = {255, 255, 255}; 
            for (int i = 0; i<3; i++){
                iofile.open(fileName.c_str(), std::ios_base::in|std::ios_base::binary);
                if (iofile.is_open()) {
            	    array[i] = iofile.get();
                    iofile.close();
                }else{
                    cout << "File output is not openned"  << endl; 
                }
            }
            if (array[0] == array[1] && array[1] == array[2]){
               c = array[0];
               c1 = c;
               read_success = true;
               break;
            }
            cout << "Read output attempt in Write Output"  << endl; 
            read_attempt--;
        }
        if (!read_success){
            cout << "File output is not correct"  << endl; 
            break;
        }
        iofile.open(fileName.c_str(), std::ios_base::out|std::ios_base::binary);
        cout << "C before = " << (bitset<8>) c << endl; 
        if (value == 0)
            c &= ~(1<<channel_number);
        else
            c |= (1<<channel_number);
    
        cout << "C after  = " << (bitset<8>) c << endl; 
        iofile.put(c);
        iofile.flush();
        iofile.close();
        iofile.open(fileName.c_str(), std::ios_base::in|std::ios_base::binary);
        if (iofile.is_open()) {
    	    c2 = iofile.get();
            iofile.close();
            if (c1 != c2)
                return;
        }
        cout << "attempt in WriteOutput"  << endl; 

        attempt--;
    }
    
}
void TSysfsOnewireDevice::SwitchLight(int output_bit_number, int state_bit_number, int on)
{
    unsigned char state_bit = 0;
    unsigned char output_bit = 0;
    int attempt = 3;

    printf("TSysfsOnewireDevice::SwitchLight\n");
    auto result = ReadState(state_bit_number);
    if (result.Defined()) {
        state_bit = *result;
    }else{
        return;
    }

    printf("State bit %i\n", (bool)state_bit);

    if (on == state_bit)
        return;

    while (attempt >=0){

        result = ReadOutput(output_bit_number);
        if (result.Defined()) {
            output_bit = *result;
        }else{
            return;
        }
    
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
    
        result = ReadState(state_bit_number);
        if (result.Defined()) {
            state_bit = *result;
        }else{
            return;
        }
        if (state_bit !=  on){
            cout << "attempt in SwitchLight"  << endl; 
            attempt--;
        }else
            break;
    }
    printf("State bit %i\n", (bool)state_bit);

}

void TSysfsOnewireManager::RescanBus()
{
}
