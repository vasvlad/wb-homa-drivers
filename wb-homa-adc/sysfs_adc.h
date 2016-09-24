#pragma once
#include <string>
#include <exception>
#include <stdexcept>

#include <memory>
#include <vector>
#define OHM_METER "ohm_meter"
#define DELAY 10
#define MXS_LRADC_DEFAULT_SCALE_FACTOR 0.451660156 // default scale for file "in_voltageNUMBER_scale"
#define ADC_VALUE_MAX 4095
#define ADC_DEFAULT_MAX_VOLTAGE 3100 // voltage in mV
using namespace std;

struct TMUXChannel // config for mux channel
{
    std::string Id;
    float Multiplier = 1;
    std::string Type = "";
    int Current = 40;// current in uA
    int Resistance1 = 1000;// resistance in Ohm
    int Resistance2 = 1000;// resistance in Ohm
    int MuxChannelNumber = 0;// ADC channel number
    int ReadingsNumber = 10;// number of reading value during one selection
    int DecimalPlaces = 3;// number of figures after point 
    int DischargeChannel = -1;// discharge channel ADC should switch to, before switching to current mux channel
    float CurrentCalibrationFactor = 1.0; //Calibration factor for current source  (usually 1.0 +/- 5%)
};
struct TChannel
{
    int AveragingWindow = 10;
    int PollInterval;
    string MatchIIO = "";
    int ChannelNumber = 1;//IIO channel
    int MinSwitchIntervalMs = 0;
    string Type = "";
    float MaxVoltage = ADC_DEFAULT_MAX_VOLTAGE;
    float Scale = 0;
    vector<int> Gpios;
    vector<TMUXChannel> Mux;
};


class TSysfsAdcChannel;

class TAdcException: public runtime_error
{
public:
  TAdcException(const string  msg) : runtime_error("Adc error " + msg) {};
};

class TSysfsAdc
{
public:
    TSysfsAdc(const std::string& sysfs_dir = "/sys", bool debug = false, const TChannel& channel_config = TChannel ());
    std::unique_ptr<TSysfsAdcChannel> GetChannel(int i);
    int ReadValue();
    inline int GetNumberOfChannels() { return NumberOfChannels; };
    double IIOScale; // Result = raw reading * IIOScale
    bool CheckVoltage(int value); // check if voltage on LRADC pin is bigger than ADC_MAX_VOLTAGE
    int GetLradcChannel() { return ChannelConfig.ChannelNumber; }; // return LRADC channel number

    virtual void SelectMuxChannel(int index) = 0;


protected:
    int AveragingWindow;
    bool Debug;
    bool Initialized;
    std::string SysfsDir;
    std::string SysfsIIODir;
    ifstream AdcValStream;
    friend class TSysfsAdcChannel;
    TChannel ChannelConfig;
    int NumberOfChannels;
    float MaxVoltage;
private:
	void SelectScale();
};

class TSysfsAdcMux : public TSysfsAdc // class, that handles mux channels
{
    public : 
        TSysfsAdcMux(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
	protected:
        void SelectMuxChannel(int index);
    private:
        void SetMuxABC(int n);
        void InitMux();
        void InitGPIO(int gpio);
        void SetGPIOValue(int gpio, int value);
        std::string GPIOPath(int gpio, const std::string& suffix) const;
        void MaybeWaitBeforeSwitching();
        int MinSwitchIntervalMs;
        int CurrentMuxInput;
        struct timespec PrevSwitchTS;
        int GpioMuxA;
        int GpioMuxB;
        int GpioMuxC;
};

class TSysfsAdcPhys: public TSysfsAdc
{
    public :
        TSysfsAdcPhys(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
    protected : 
		void SelectMuxChannel(int index);
};
 
struct TSysfsAdcChannelPrivate 
{
    TSysfsAdc* Owner;
    int Index;
    std::string Name;
    vector<int> Buffer;
    double Sum = 0;
    bool Ready = false;
    int Pos = 0;
    int ReadingsNumber;
    int ChannelAveragingWindow;
    int DischargeChannel;

}; 
class TSysfsAdcChannel 
{ 
    public: 
        int GetAverageValue();
        virtual float GetValue(); 
        const std::string& GetName() const;
        virtual std::string GetType();
        TSysfsAdcChannel(TSysfsAdc* owner, int index, const std::string& name, int readings_number, int decimal_places, int discharge_channel);
        TSysfsAdcChannel(TSysfsAdc* owner, int index, const std::string& name, int readings_number,int decimal_places,
                         int discharge_channel, float multiplier);
        int DecimalPlaces;
    protected:
        std::unique_ptr<TSysfsAdcChannelPrivate> d;
        friend class TSysfsAdc;
    private:
        float Multiplier;
        float Scale;
};

class TSysfsAdcChannelRes : public TSysfsAdcChannel// class, that measures resistance
{
    public : 
         TSysfsAdcChannelRes(TSysfsAdc* owner, int index, const std::string& name, int readings_number, int decimal_places, 
							 int discharge_channel, int current, int resistance1, int resistance2, bool source_always_on,
							 float current_calibration_factor);
         float GetValue();
         std::string GetType();
         void SetUpCurrentSource();
         void SwitchOffCurrentSource();
    private:
        int Current;
        int Resistance1;
        int Resistance2;
        std::string Type;
        int CurrentSourceChannel;
        bool SourceAlwaysOn;
        float CurrentCalibrationFactor;
};
