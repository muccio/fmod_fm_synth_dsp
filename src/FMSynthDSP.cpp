#include "FMSynthDSP.h"

#include <cmath>
#include <cstdio>
#include <new>

namespace FMODPlugin {

FMSynthDSP::FMSynthDSP() noexcept
    : m_carrierFreq(CARRIER_FREQ_DEFAULT)
    , m_modRatio(MOD_RATIO_DEFAULT)
    , m_modIndex(MOD_INDEX_DEFAULT)
    , m_volume(VOL_DEFAULT)
    , m_carrierPhase(0.0f)
    , m_modulatorPhase(0.0f)
{
}

void FMSynthDSP::reset() noexcept
{
    m_carrierPhase = 0.0f;
    m_modulatorPhase = 0.0f;
}

void FMSynthDSP::process(float* outBuffer, unsigned int length, int numChannels, int sampleRate) noexcept
{
    if (!outBuffer || length == 0 || numChannels <= 0 || sampleRate <= 0)
    {
        return;
    }

    // Load atomic parameters once per audio block for consistent buffer processing
    const float fc = m_carrierFreq.load(std::memory_order_relaxed);
    const float ratio = m_modRatio.load(std::memory_order_relaxed);
    const float index = m_modIndex.load(std::memory_order_relaxed);
    const float vol = m_volume.load(std::memory_order_relaxed);

    const float fm = fc * ratio;
    const float invFs = 1.0f / static_cast<float>(sampleRate);
    const float carrierPhaseInc = fc * invFs;
    const float modPhaseInc = fm * invFs;

    constexpr float TWO_PI = 6.28318530717958647692f;

    float cPhase = m_carrierPhase;
    float mPhase = m_modulatorPhase;

    for (unsigned int i = 0; i < length; ++i)
    {
        // 1. Modulator oscillator output
        const float modVal = std::sin(TWO_PI * mPhase);

        // 2. Chowning FM Carrier synthesis: s(t) = Vol * sin(2*pi*fc*t + Index * sin(2*pi*fm*t))
        const float sample = vol * std::sin(TWO_PI * cPhase + index * modVal);

        // Write sample to all configured output channels (interleaved format)
        const unsigned int frameOffset = i * static_cast<unsigned int>(numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            outBuffer[frameOffset + static_cast<unsigned int>(ch)] = sample;
        }

        // Advance phase accumulators and wrap strictly within [0.0, 1.0)
        cPhase += carrierPhaseInc;
        if (cPhase >= 1.0f)
        {
            cPhase -= std::floor(cPhase);
        }

        mPhase += modPhaseInc;
        if (mPhase >= 1.0f)
        {
            mPhase -= std::floor(mPhase);
        }
    }

    m_carrierPhase = cPhase;
    m_modulatorPhase = mPhase;
}

void FMSynthDSP::setCarrierFrequency(float freq) noexcept
{
    const float clamped = std::clamp(freq, CARRIER_FREQ_MIN, CARRIER_FREQ_MAX);
    m_carrierFreq.store(clamped, std::memory_order_relaxed);
}

float FMSynthDSP::getCarrierFrequency() const noexcept
{
    return m_carrierFreq.load(std::memory_order_relaxed);
}

void FMSynthDSP::setModulatorRatio(float ratio) noexcept
{
    const float clamped = std::clamp(ratio, MOD_RATIO_MIN, MOD_RATIO_MAX);
    m_modRatio.store(clamped, std::memory_order_relaxed);
}

float FMSynthDSP::getModulatorRatio() const noexcept
{
    return m_modRatio.load(std::memory_order_relaxed);
}

void FMSynthDSP::setModulationIndex(float index) noexcept
{
    const float clamped = std::clamp(index, MOD_INDEX_MIN, MOD_INDEX_MAX);
    m_modIndex.store(clamped, std::memory_order_relaxed);
}

float FMSynthDSP::getModulationIndex() const noexcept
{
    return m_modIndex.load(std::memory_order_relaxed);
}

void FMSynthDSP::setVolume(float vol) noexcept
{
    const float clamped = std::clamp(vol, VOL_MIN, VOL_MAX);
    m_volume.store(clamped, std::memory_order_relaxed);
}

float FMSynthDSP::getVolume() const noexcept
{
    return m_volume.load(std::memory_order_relaxed);
}

FMOD_RESULT FMSynthDSP::getInfo(char* outName, unsigned int* outVersion, int* outChannels, int* outConfigWidth, int* outConfigHeight) const noexcept
{
    if (outName)
    {
        std::strncpy(outName, "FM Synthesizer", 32);
        outName[31] = '\0';
    }
    if (outVersion)
    {
        *outVersion = 0x00010000; // Version 1.0.0
    }
    if (outChannels)
    {
        *outChannels = 2; // Default stereo output
    }
    if (outConfigWidth)
    {
        *outConfigWidth = 0;
    }
    if (outConfigHeight)
    {
        *outConfigHeight = 0;
    }
    return FMOD_OK;
}

} // namespace FMODPlugin

/*
 * Static parameter definitions & FMOD DSP Description setup
 */
static float gFreqValues[]    = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
static float gFreqPositions[] = { 0.000f, 0.133f, 0.233f, 0.333f, 0.466f, 0.566f, 0.667f, 0.799f, 0.900f, 1.000f };

static FMOD_DSP_PARAMETER_DESC gParamCarrierFreq;
static FMOD_DSP_PARAMETER_DESC gParamModRatio;
static FMOD_DSP_PARAMETER_DESC gParamModIndex;
static FMOD_DSP_PARAMETER_DESC gParamVolume;

static FMOD_DSP_PARAMETER_DESC* gParamArray[FMODPlugin::NUM_PARAMETERS] = {
    &gParamCarrierFreq,
    &gParamModRatio,
    &gParamModIndex,
    &gParamVolume
};

static bool gParamsInitialized = false;

static void InitPluginParameters()
{
    if (gParamsInitialized)
    {
        return;
    }

    // Parameter 0: Carrier Frequency (Logarithmic piecewise mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT_WITH_MAPPING(
        gParamCarrierFreq,
        "Carrier Freq",
        "Hz",
        "Fundamental frequency of the carrier operator in Hertz",
        FMODPlugin::CARRIER_FREQ_DEFAULT,
        gFreqValues,
        gFreqPositions
    );

    // Parameter 1: Modulator Ratio (Linear mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT(
        gParamModRatio,
        "Mod Ratio",
        "x",
        "Frequency ratio between modulator and carrier operator (fm = fc * ratio)",
        FMODPlugin::MOD_RATIO_MIN,
        FMODPlugin::MOD_RATIO_MAX,
        FMODPlugin::MOD_RATIO_DEFAULT
    );

    // Parameter 2: Modulation Index (Linear mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT(
        gParamModIndex,
        "Mod Index",
        "",
        "Modulation index controlling harmonic richness and brightness",
        FMODPlugin::MOD_INDEX_MIN,
        FMODPlugin::MOD_INDEX_MAX,
        FMODPlugin::MOD_INDEX_DEFAULT
    );

    // Parameter 3: Volume (Linear mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT(
        gParamVolume,
        "Volume",
        "",
        "Output signal linear amplitude volume (0.0 to 1.0)",
        FMODPlugin::VOL_MIN,
        FMODPlugin::VOL_MAX,
        FMODPlugin::VOL_DEFAULT
    );

    gParamsInitialized = true;
}

static FMOD_DSP_DESCRIPTION gFMSynthDSPDesc = {
    FMOD_PLUGIN_SDK_VERSION,                // pluginsdkversion
    "FM Synthesizer",                       // name
    0x00010000,                             // version (1.0.0)
    1,                                      // numinputbuffers (1 allows insertion into tracks/buses in FMOD Studio)
    1,                                      // numoutputbuffers (1 output buffer)
    FMSynthDSP_Create,                      // create
    FMSynthDSP_Release,                     // release
    FMSynthDSP_Reset,                       // reset
    nullptr,                                // read (unused in modern FMOD in favor of process)
    FMSynthDSP_Process,                     // process
    nullptr,                                // setposition
    FMODPlugin::NUM_PARAMETERS,             // numparameters (4)
    gParamArray,                            // paramdesc
    FMSynthDSP_SetParameterFloat,           // setparameterfloat
    nullptr,                                // setparameterint
    nullptr,                                // setparameterbool
    nullptr,                                // setparameterdata
    FMSynthDSP_GetParameterFloat,           // getparameterfloat
    nullptr,                                // getparameterint
    nullptr,                                // getparameterbool
    nullptr,                                // getparameterdata
    FMSynthDSP_ShouldIProcess,              // shouldiprocess
    nullptr,                                // userdata
    nullptr,                                // sys_register
    nullptr,                                // sys_deregister
    nullptr                                 // sys_mix
};

/*
 * Speaker Mode mapping helper
 */
static void GetSpeakerModeChannelsAndMask(FMOD_SPEAKERMODE mode, int& channels, FMOD_CHANNELMASK& mask)
{
    switch (mode)
    {
        case FMOD_SPEAKERMODE_MONO:
            channels = 1;
            mask = FMOD_CHANNELMASK_MONO;
            break;
        case FMOD_SPEAKERMODE_STEREO:
            channels = 2;
            mask = FMOD_CHANNELMASK_STEREO;
            break;
        case FMOD_SPEAKERMODE_QUAD:
            channels = 4;
            mask = FMOD_CHANNELMASK_QUAD;
            break;
        case FMOD_SPEAKERMODE_SURROUND:
            channels = 5;
            mask = FMOD_CHANNELMASK_SURROUND;
            break;
        case FMOD_SPEAKERMODE_5POINT1:
            channels = 6;
            mask = FMOD_CHANNELMASK_5POINT1;
            break;
        case FMOD_SPEAKERMODE_7POINT1:
            channels = 8;
            mask = FMOD_CHANNELMASK_7POINT1;
            break;
        default:
            channels = 2;
            mask = FMOD_CHANNELMASK_STEREO;
            break;
    }
}

/*
 * C-linkage FMOD DSP Callbacks implementation
 */
extern "C" {

FMOD_RESULT F_CALL FMSynthDSP_Create(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = new (std::nothrow) FMODPlugin::FMSynthDSP();
    if (!instance)
    {
        return FMOD_ERR_MEMORY;
    }

    dsp_state->plugindata = instance;
    return FMOD_OK;
}

FMOD_RESULT F_CALL FMSynthDSP_Release(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
    if (instance)
    {
        delete instance;
        dsp_state->plugindata = nullptr;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL FMSynthDSP_Reset(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
    if (instance)
    {
        instance->reset();
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL FMSynthDSP_ShouldIProcess(
    FMOD_DSP_STATE* dsp_state,
    FMOD_BOOL inputsidle,
    unsigned int length,
    FMOD_CHANNELMASK inmask,
    int inchannels,
    FMOD_SPEAKERMODE speakermode)
{
    (void)dsp_state;
    (void)inputsidle;
    (void)length;
    (void)inmask;
    (void)inchannels;
    (void)speakermode;

    // Continuous sound generator / synthesizer: always process even when inputs are idle
    return FMOD_OK;
}

FMOD_RESULT F_CALL FMSynthDSP_Process(
    FMOD_DSP_STATE* dsp_state,
    unsigned int length,
    const FMOD_DSP_BUFFER_ARRAY* inbufferarray,
    FMOD_DSP_BUFFER_ARRAY* outbufferarray,
    FMOD_BOOL inputsidle,
    FMOD_DSP_PROCESS_OPERATION op)
{
    (void)inputsidle;

    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        if (outbufferarray && outbufferarray->numbuffers > 0)
        {
            int channels = 2;
            FMOD_CHANNELMASK channelMask = FMOD_CHANNELMASK_STEREO;
            FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_STEREO;

            // Inherit input format if available from track/bus
            if (inbufferarray && inbufferarray->numbuffers > 0 &&
                inbufferarray->buffernumchannels && inbufferarray->buffernumchannels[0] > 0)
            {
                channels = inbufferarray->buffernumchannels[0];
                speakerMode = inbufferarray->speakermode;
                if (inbufferarray->bufferchannelmask && inbufferarray->bufferchannelmask[0] != 0)
                {
                    channelMask = inbufferarray->bufferchannelmask[0];
                }
                else
                {
                    GetSpeakerModeChannelsAndMask(speakerMode, channels, channelMask);
                }
            }
            else
            {
                // Query system speaker mode
                if (dsp_state && dsp_state->functions && dsp_state->functions->getspeakermode)
                {
                    FMOD_SPEAKERMODE mixerMode = FMOD_SPEAKERMODE_DEFAULT;
                    FMOD_SPEAKERMODE outputMode = FMOD_SPEAKERMODE_DEFAULT;
                    if (dsp_state->functions->getspeakermode(dsp_state, &mixerMode, &outputMode) == FMOD_OK)
                    {
                        if (mixerMode > FMOD_SPEAKERMODE_DEFAULT && mixerMode < FMOD_SPEAKERMODE_MAX)
                        {
                            speakerMode = mixerMode;
                        }
                    }
                }
                GetSpeakerModeChannelsAndMask(speakerMode, channels, channelMask);
            }

            outbufferarray->speakermode = speakerMode;
            if (outbufferarray->buffernumchannels)
            {
                outbufferarray->buffernumchannels[0] = channels;
            }
            if (outbufferarray->bufferchannelmask)
            {
                outbufferarray->bufferchannelmask[0] = channelMask;
            }
        }
        return FMOD_OK;
    }

    if (op == FMOD_DSP_PROCESS_PERFORM)
    {
        if (!dsp_state)
        {
            return FMOD_ERR_INVALID_PARAM;
        }

        auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
        if (!instance)
        {
            return FMOD_ERR_INVALID_HANDLE;
        }

        if (!outbufferarray || outbufferarray->numbuffers <= 0 ||
            !outbufferarray->buffers || !outbufferarray->buffers[0])
        {
            return FMOD_OK;
        }

        // Query system sample rate directly from FMOD state functions
        int sampleRate = 48000;
        if (dsp_state->functions && dsp_state->functions->getsamplerate)
        {
            dsp_state->functions->getsamplerate(dsp_state, &sampleRate);
        }
        if (sampleRate <= 0)
        {
            sampleRate = 48000;
        }

        // Determine configured channel count for output buffer
        int numChannels = 2;
        if (outbufferarray->buffernumchannels && outbufferarray->buffernumchannels[0] > 0)
        {
            numChannels = outbufferarray->buffernumchannels[0];
        }

        float* outBuffer = outbufferarray->buffers[0];
        instance->process(outBuffer, length, numChannels, sampleRate);

        return FMOD_OK;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL FMSynthDSP_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
    if (!instance)
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    switch (index)
    {
        case FMODPlugin::PARAM_CARRIER_FREQ:
            instance->setCarrierFrequency(value);
            return FMOD_OK;

        case FMODPlugin::PARAM_MOD_RATIO:
            instance->setModulatorRatio(value);
            return FMOD_OK;

        case FMODPlugin::PARAM_MOD_INDEX:
            instance->setModulationIndex(value);
            return FMOD_OK;

        case FMODPlugin::PARAM_VOLUME:
            instance->setVolume(value);
            return FMOD_OK;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }
}

FMOD_RESULT F_CALL FMSynthDSP_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr)
{
    if (!dsp_state || !value)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
    if (!instance)
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    switch (index)
    {
        case FMODPlugin::PARAM_CARRIER_FREQ:
        {
            const float f = instance->getCarrierFrequency();
            *value = f;
            if (valuestr)
            {
                std::snprintf(valuestr, FMOD_DSP_GETPARAM_VALUESTR_LENGTH, "%.1f Hz", f);
            }
            return FMOD_OK;
        }

        case FMODPlugin::PARAM_MOD_RATIO:
        {
            const float r = instance->getModulatorRatio();
            *value = r;
            if (valuestr)
            {
                std::snprintf(valuestr, FMOD_DSP_GETPARAM_VALUESTR_LENGTH, "%.2f x", r);
            }
            return FMOD_OK;
        }

        case FMODPlugin::PARAM_MOD_INDEX:
        {
            const float idx = instance->getModulationIndex();
            *value = idx;
            if (valuestr)
            {
                std::snprintf(valuestr, FMOD_DSP_GETPARAM_VALUESTR_LENGTH, "%.2f", idx);
            }
            return FMOD_OK;
        }

        case FMODPlugin::PARAM_VOLUME:
        {
            const float v = instance->getVolume();
            *value = v;
            if (valuestr)
            {
                std::snprintf(valuestr, FMOD_DSP_GETPARAM_VALUESTR_LENGTH, "%.2f", v);
            }
            return FMOD_OK;
        }

        default:
            return FMOD_ERR_INVALID_PARAM;
    }
}

FMOD_RESULT F_CALL FMSynthDSP_GetInfo(
    FMOD_DSP_STATE* dsp_state,
    char* name,
    unsigned int* version,
    int* channels,
    int* configwidth,
    int* configheight)
{
    if (dsp_state && dsp_state->plugindata)
    {
        auto* instance = static_cast<FMODPlugin::FMSynthDSP*>(dsp_state->plugindata);
        return instance->getInfo(name, version, channels, configwidth, configheight);
    }

    if (name)
    {
        std::strncpy(name, "FM Synthesizer", 32);
        name[31] = '\0';
    }
    if (version)
    {
        *version = 0x00010000;
    }
    if (channels)
    {
        *channels = 2;
    }
    if (configwidth)
    {
        *configwidth = 0;
    }
    if (configheight)
    {
        *configheight = 0;
    }
    return FMOD_OK;
}

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetDSPDescription()
{
    InitPluginParameters();
    return &gFMSynthDSPDesc;
}

} // extern "C"
