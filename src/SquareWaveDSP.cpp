#include "SquareWaveDSP.h"

#include <cmath>
#include <cstdio>
#include <new>

namespace FMODPlugin {

SquareWaveDSP::SquareWaveDSP() noexcept
    : m_frequency(FREQ_DEFAULT)
    , m_volume(VOL_DEFAULT)
    , m_phase(0.0f)
{
}

void SquareWaveDSP::reset() noexcept
{
    m_phase = 0.0f;
}

void SquareWaveDSP::process(float* outBuffer, unsigned int length, int numChannels, int sampleRate) noexcept
{
    if (!outBuffer || length == 0 || numChannels <= 0 || sampleRate <= 0)
    {
        return;
    }

    // Load atomic parameters once per audio block for consistent buffer processing
    const float freq = m_frequency.load(std::memory_order_relaxed);
    const float vol  = m_volume.load(std::memory_order_relaxed);

    const float phaseIncrement = freq / static_cast<float>(sampleRate);
    float phase = m_phase;

    for (unsigned int i = 0; i < length; ++i)
    {
        // Pure bipolar square wave: +vol for first half of cycle [0.0, 0.5), -vol for second half [0.5, 1.0)
        const float sample = (phase < 0.5f) ? vol : -vol;

        // Write sample to all configured output channels (interleaved format)
        const unsigned int frameOffset = i * static_cast<unsigned int>(numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            outBuffer[frameOffset + static_cast<unsigned int>(ch)] = sample;
        }

        // Advance phase accumulator and wrap strictly within [0.0, 1.0)
        phase += phaseIncrement;
        if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
        }
    }

    m_phase = phase;
}

void SquareWaveDSP::setFrequency(float freq) noexcept
{
    const float clamped = std::clamp(freq, FREQ_MIN, FREQ_MAX);
    m_frequency.store(clamped, std::memory_order_relaxed);
}

float SquareWaveDSP::getFrequency() const noexcept
{
    return m_frequency.load(std::memory_order_relaxed);
}

void SquareWaveDSP::setVolume(float vol) noexcept
{
    const float clamped = std::clamp(vol, VOL_MIN, VOL_MAX);
    m_volume.store(clamped, std::memory_order_relaxed);
}

float SquareWaveDSP::getVolume() const noexcept
{
    return m_volume.load(std::memory_order_relaxed);
}

FMOD_RESULT SquareWaveDSP::getInfo(char* outName, unsigned int* outVersion, int* outChannels, int* outConfigWidth, int* outConfigHeight) const noexcept
{
    if (outName)
    {
        std::strncpy(outName, "Square Wave Generator", 32);
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

static FMOD_DSP_PARAMETER_DESC gParamFreq;
static FMOD_DSP_PARAMETER_DESC gParamVol;

static FMOD_DSP_PARAMETER_DESC* gParamArray[FMODPlugin::NUM_PARAMETERS] = {
    &gParamFreq,
    &gParamVol
};

static bool gParamsInitialized = false;

static void InitPluginParameters()
{
    if (gParamsInitialized)
    {
        return;
    }

    // Parameter 0: Frequency (Logarithmic piecewise mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT_WITH_MAPPING(
        gParamFreq,
        "Frequency",
        "Hz",
        "Fundamental frequency of the square wave in Hertz (logarithmic mapping)",
        FMODPlugin::FREQ_DEFAULT,
        gFreqValues,
        gFreqPositions
    );

    // Parameter 1: Volume (Linear mapping)
    FMOD_DSP_INIT_PARAMDESC_FLOAT(
        gParamVol,
        "Volume",
        "",
        "Output signal linear volume (0.0 to 1.0)",
        FMODPlugin::VOL_MIN,
        FMODPlugin::VOL_MAX,
        FMODPlugin::VOL_DEFAULT
    );

    gParamsInitialized = true;
}

static FMOD_DSP_DESCRIPTION gSquareWaveDSPDesc = {
    FMOD_PLUGIN_SDK_VERSION,                // pluginsdkversion
    "Square Wave Generator",                // name
    0x00010000,                             // version (1.0.0)
    1,                                      // numinputbuffers (1 allows insertion into tracks/buses in FMOD Studio)
    1,                                      // numoutputbuffers (1 output buffer)
    SquareWaveDSP_Create,                   // create
    SquareWaveDSP_Release,                  // release
    SquareWaveDSP_Reset,                    // reset
    nullptr,                                // read (unused in modern FMOD in favor of process)
    SquareWaveDSP_Process,                  // process
    nullptr,                                // setposition
    FMODPlugin::NUM_PARAMETERS,             // numparameters (2)
    gParamArray,                            // paramdesc
    SquareWaveDSP_SetParameterFloat,        // setparameterfloat
    nullptr,                                // setparameterint
    nullptr,                                // setparameterbool
    nullptr,                                // setparameterdata
    SquareWaveDSP_GetParameterFloat,        // getparameterfloat
    nullptr,                                // getparameterint
    nullptr,                                // getparameterbool
    nullptr,                                // getparameterdata
    SquareWaveDSP_ShouldIProcess,           // shouldiprocess
    nullptr,                                // userdata
    nullptr,                                // sys_register
    nullptr,                                // sys_deregister
    nullptr                                 // sys_mix
};

/*
 * C-linkage FMOD DSP Callbacks implementation
 */
extern "C" {

FMOD_RESULT F_CALL SquareWaveDSP_Create(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = new (std::nothrow) FMODPlugin::SquareWaveDSP();
    if (!instance)
    {
        return FMOD_ERR_MEMORY;
    }

    dsp_state->plugindata = instance;
    return FMOD_OK;
}

FMOD_RESULT F_CALL SquareWaveDSP_Release(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
    if (instance)
    {
        delete instance;
        dsp_state->plugindata = nullptr;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL SquareWaveDSP_Reset(FMOD_DSP_STATE* dsp_state)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
    if (instance)
    {
        instance->reset();
    }

    return FMOD_OK;
}

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

FMOD_RESULT F_CALL SquareWaveDSP_ShouldIProcess(
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

FMOD_RESULT F_CALL SquareWaveDSP_Process(
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

        auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
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

FMOD_RESULT F_CALL SquareWaveDSP_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value)
{
    if (!dsp_state)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
    if (!instance)
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    switch (index)
    {
        case FMODPlugin::PARAM_FREQUENCY:
            instance->setFrequency(value);
            return FMOD_OK;

        case FMODPlugin::PARAM_VOLUME:
            instance->setVolume(value);
            return FMOD_OK;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }
}

FMOD_RESULT F_CALL SquareWaveDSP_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr)
{
    if (!dsp_state || !value)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
    if (!instance)
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    switch (index)
    {
        case FMODPlugin::PARAM_FREQUENCY:
        {
            const float f = instance->getFrequency();
            *value = f;
            if (valuestr)
            {
                std::snprintf(valuestr, FMOD_DSP_GETPARAM_VALUESTR_LENGTH, "%.1f Hz", f);
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

FMOD_RESULT F_CALL SquareWaveDSP_GetInfo(
    FMOD_DSP_STATE* dsp_state,
    char* name,
    unsigned int* version,
    int* channels,
    int* configwidth,
    int* configheight)
{
    if (dsp_state && dsp_state->plugindata)
    {
        auto* instance = static_cast<FMODPlugin::SquareWaveDSP*>(dsp_state->plugindata);
        return instance->getInfo(name, version, channels, configwidth, configheight);
    }

    if (name)
    {
        std::strncpy(name, "Square Wave Generator", 32);
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
    return &gSquareWaveDSPDesc;
}

} // extern "C"
