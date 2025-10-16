
#include "SoapyRX888.hpp"
#include <SoapySDR/Time.hpp>
#include <SoapySDR/Types.hpp>
#include <algorithm>

SoapyRX888::SoapyRX888(const SoapySDR::Kwargs &args):
    deviceId(-1),
    dev(nullptr),
    rfGain(0),
    vgaGain(29),
    vgaAtt(0),
    randCtrl(false),
    ditherCtrl(true),
    pgaCtrl(true),
    rxFormat(RX888_RX_FORMAT_INT16),
    sampleRate(64000000),
    numBuffers(DEFAULT_NUM_BUFFERS)
    
{
    if (args.count("label") != 0) SoapySDR_logf(SOAPY_SDR_INFO, "Opening %s...", args.at("label").c_str());

    //if a serial is not present, then findRX888 had zero devices enumerated
    if (args.count("serial") == 0) throw std::runtime_error("No RX888 devices found!");

    const auto serial = args.at("serial");
    deviceId = rx888_get_index_by_serial(serial.c_str());
    if (deviceId < 0) throw std::runtime_error("rx888_get_index_by_serial("+serial+") - " + std::to_string(deviceId));
    
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RX888 opening device %d", deviceId);
    if (rx888_open(&dev, deviceId) != 0) {
        throw std::runtime_error("Unable to open RX888 device");
    }

    // TODO: Testing to see if we need a little time before starting to send commands to device.
    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    rx888_set_dither(dev, ditherCtrl);
    rx888_set_rand(dev, randCtrl);
    rx888_set_pga(dev, pgaCtrl);
    rx888_set_vga_gain(dev, vgaGain);
    rx888_set_vga_attenuation(dev, vgaAtt);
}

SoapyRX888::~SoapyRX888(void)
{
    //cleanup device handles
    rx888_close(dev);
}



/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyRX888::getDriverKey(void) const
{
    return "RX888";
}

std::string SoapyRX888::getHardwareKey(void) const
{
    return "RX888";
}

SoapySDR::Kwargs SoapyRX888::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/cozycactus/SoapyRX888";
    args["index"] = std::to_string(deviceId);

    return args;
}    

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyRX888::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapyRX888::getFullDuplex(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    return false;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyRX888::listAntennas(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    std::vector<std::string> antennas;
    antennas.push_back("RX");
    return antennas;
}

void SoapyRX888::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    (void)channel;
    (void)name;
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("setAntena failed: RX888 only supports RX");
    }
}

std::string SoapyRX888::getAntenna(const int direction, const size_t channel) const
{
    (void)channel;
    (void)direction;
    return "RX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyRX888::hasDCOffsetMode(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    return false;
}

bool SoapyRX888::hasFrequencyCorrection(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    return true;
}

void SoapyRX888::setFrequencyCorrection(const int direction, const size_t channel, const double value) {
    (void)direction;
    (void)channel;
    setFrequency(direction, channel, "CORR", value);
}

double SoapyRX888::getFrequencyCorrection(const int direction, const size_t channel) const {
    (void)direction;
    (void)channel;
    return getFrequency(direction, channel, "CORR");
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyRX888::listGains(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    std::vector<std::string> gains;
    // DeviceName='RX888'
    gains.push_back("RF");

    // DeviceNames: RX888, RX888MK2
    gains.push_back("VGAGain");
    gains.push_back("VGAAtt");

    return gains;
}

bool SoapyRX888::hasGainMode(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    return false;
}

void SoapyRX888::setGain(const int direction, const size_t channel, const std::string &name, const double value) 
{
    (void)direction;
    (void)channel;
    if (name == "RF")
    {
        rfGain = value;
        rx888_set_hf_attenuation(dev, rfGain);
    } else if (name == "VGAGain")
    {
        vgaGain = (int) value;
        rx888_set_vga_gain(dev, vgaGain);
    } else if (name == "VGAAtt")
    {
        vgaAtt = (int) value;
        rx888_set_vga_attenuation(dev, vgaAtt);
    }

}

SoapySDR::Range SoapyRX888::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    (void)direction;
    (void)channel;
    if (name == "RF")
    {
        return SoapySDR::Range(-20.0, 0, 10.0);
    } else if (name == "VGAGain")
    {
        return SoapySDR::Range(0, 255, 1);
    } else if (name == "VGAAtt")
    {
        return SoapySDR::Range(0, 63, 1);
    }
    
    return SoapySDR::Range(0, 0);
} 

void SoapyRX888::setFrequency(const int direction,
                                 const size_t channel,
                                 const double frequency,
                                 const SoapySDR::Kwargs &args)
{
    // default to RF
    setFrequency(direction, channel, "RF", frequency, args);
}

void SoapyRX888::setFrequency(const int direction,
                                 const size_t channel,
                                 const std::string &name,
                                 const double frequency,
                                 const SoapySDR::Kwargs &args)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);


   if (direction == SOAPY_SDR_RX)
   {
      if (name == "RF")
      {
         SoapySDR::RangeList frequencyRange = getFrequencyRange(direction, channel, name);
         if (!(frequency >= frequencyRange.front().minimum() && frequency <= frequencyRange.back().maximum()))
         {
            SoapySDR_logf(SOAPY_SDR_WARNING, "RF center frequency out of range - frequency=%lg", frequency);
            return;
         }
         if (freqHz != (uint32_t)frequency)
         {
            freqHz = (uint32_t)frequency;
            // TODO: Enable setting of HF Frequency
         }
      }
      // can't set ppm for RSPduo slaves
      else if ((name == "CORR") && (ppm != frequency))
      {
         ppm = frequency;
         // TODO: Enable setting of PPM 
      }
   }
}

double SoapyRX888::getFrequency(const int direction, const size_t channel) const
{
    // default to RF
    return getFrequency(direction, channel, "RF");
}

double SoapyRX888::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (name == "RF")
    {
        return freqHz;
    }
    else if (name == "CORR")
    {
        return ppm;
    }

    return 0;
}

std::vector<std::string> SoapyRX888::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    names.push_back("CORR");
    return names;
}

SoapySDR::RangeList SoapyRX888::getFrequencyRange(const int direction, const size_t channel) const
{
    return getFrequencyRange(direction, channel, "RF");
}

SoapySDR::RangeList SoapyRX888::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        // TODO: Confirm this the right way ? or should we be using BW and setting SampleRate as BW * 2 ?
        //results.push_back(SoapySDR::Range(1000, rx888_get_sample_rate(dev)/2.0));
        // results.push_back(SoapySDR::Range(1000, sampleRate));
        // results.push_back(SoapySDR::Range(1000, sampleRate/2.0));

        results.push_back(SoapySDR::Range(0, sampleRate));
    }
    return results;
}

SoapySDR::ArgInfoList SoapyRX888::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyRX888::setSampleRate(const int direction, const size_t channel, const double rate)
{
    (void)direction;
    (void)channel;
    long long ns = SoapySDR::ticksToTimeNs(ticks, sampleRate);
    sampleRate = rate;
    resetBuffer = true;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
    int r = rx888_set_sample_rate(dev, sampleRate);
    if (r == -EINVAL)
    {
        throw std::runtime_error("setSampleRate failed: RX888 does not support this sample rate");
    }
    if (r != 0)
    {
        throw std::runtime_error("setSampleRate failed");
    }
    sampleRate = rx888_get_sample_rate(dev);
    ticks = SoapySDR::timeNsToTicks(ns, sampleRate);
}

double SoapyRX888::getSampleRate(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    return sampleRate;
}

std::vector<double> SoapyRX888::listSampleRates(const int direction, const size_t channel) const
{
    (void)direction;
    (void)channel;
    std::vector<double> results;

    results.push_back(250000);
    results.push_back(500000);
    results.push_back(1000000);
    results.push_back(2000000);
    results.push_back(4000000);
    results.push_back(8000000);
    results.push_back(16000000);
    results.push_back(32000000);
    results.push_back(64000000);
    results.push_back(128000000);
    results.push_back(150000000);

    return results;
}

/*******************************************************************
 * Time API
 ******************************************************************/

std::vector<std::string> SoapyRX888::listTimeSources(void) const
{
    std::vector<std::string> results;

    results.push_back("sw_ticks");

    return results;
}

std::string SoapyRX888::getTimeSource(void) const
{
    return "sw_ticks";
}

bool SoapyRX888::hasHardwareTime(const std::string &what) const
{
    return what == "" || what == "sw_ticks";
}

long long SoapyRX888::getHardwareTime(const std::string &what) const
{
    (void)what;
    return SoapySDR::ticksToTimeNs(ticks, sampleRate);
}

void SoapyRX888::setHardwareTime(const long long timeNs, const std::string &what)
{
    (void)what;
    ticks = SoapySDR::timeNsToTicks(timeNs, sampleRate);
}


/*******************************************************************
 * Device Specific Settings
 ******************************************************************/

SoapySDR::ArgInfoList SoapyRX888::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;

    SoapySDR::ArgInfo RandArg;
    RandArg.key = "rand_ctlr";
    RandArg.value = "true";
    RandArg.name = "Enable randomization";
    RandArg.description = "Enable randomization control";
    RandArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(RandArg);

    SoapySDR::ArgInfo DitherArg;
    DitherArg.key = "dither_ctlr";
    DitherArg.value = "true";
    DitherArg.name = "Enable dithering";
    DitherArg.description = "Enable dithering control";
    DitherArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(DitherArg);

    SoapySDR::ArgInfo PGAArg;
    PGAArg.key = "pga_ctlr";
    PGAArg.value = "true";
    PGAArg.name = "PGA Amplifier";
    PGAArg.description = "PGA amplifier control";
    PGAArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(PGAArg);

    return setArgs;
    
}

void SoapyRX888::writeSetting(const std::string &key, const std::string &value)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    SoapySDR_logf(SOAPY_SDR_INFO, "  -- writeSetting: %s=%s", key.c_str(), value.c_str());

    if (key == "rf") {
        rx888_set_hf_attenuation(dev, stod(value));
    } else if (key == "vga-gain") {
        rx888_set_vga_gain(dev, stoi(value));
    } else if (key == "vga-att") {
        rx888_set_vga_attenuation(dev, stoi(value));
    } else if (key == "rand") {
        rx888_set_rand(dev, value == "true");
    } else if (key == "dither") {
        rx888_set_dither(dev, value == "true");
    } else if (key == "pga") {
        rx888_set_pga(dev, value == "true");
    }

}

std::string SoapyRX888::readSetting(const std::string &key) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);
    std::string val = "";
    
    if (key == "rf") {
        val = std::to_string(rfGain);
    } else if (key == "vga-gain") {
        val = std::to_string(vgaGain);
    } else if (key == "vga-att") {
        val = std::to_string(vgaAtt);
    } else if (key == "rand_ctlr") {
        val = (randCtrl) ? "true" : "false";
    } else if (key == "dither_ctlr") {
        val = (ditherCtrl) ? "true" : "false";
    } else if (key == "pga_ctlr") {
        val = (pgaCtrl) ? "true" : "false";
    }

    SoapySDR_logf(SOAPY_SDR_INFO, "  -- readSetting: %s=%s", key.c_str(), val.c_str());

    return val;
}
