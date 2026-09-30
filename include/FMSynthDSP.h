#pragma once

#include "fmod_common.h"
#include "fmod_dsp.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace FMODPlugin {

/**
 * @brief Identifiers for the parameters exposed by FMSynthDSP.
 */
enum ParameterIndex : int
{
    PARAM_CARRIER_FREQ = 0, ///< Carrier frequency in Hertz (Float: 20 Hz - 20,000 Hz)
    PARAM_MOD_RATIO    = 1, ///< Modulator-to-Carrier frequency ratio (Float: 0.1 - 16.0)
    PARAM_MOD_INDEX    = 2, ///< Modulation index / depth (Float: 0.0 - 20.0)
    PARAM_VOLUME       = 3, ///< Output amplitude volume (Float: 0.0 - 1.0)
    NUM_PARAMETERS     = 4
};

// Parameter Limits and Defaults
constexpr float CARRIER_FREQ_MIN     = 20.0f;
constexpr float CARRIER_FREQ_MAX     = 20000.0f;
constexpr float CARRIER_FREQ_DEFAULT = 440.0f;

constexpr float MOD_RATIO_MIN        = 0.1f;
constexpr float MOD_RATIO_MAX        = 16.0f;
constexpr float MOD_RATIO_DEFAULT    = 1.0f;

constexpr float MOD_INDEX_MIN        = 0.0f;
constexpr float MOD_INDEX_MAX        = 20.0f;
constexpr float MOD_INDEX_DEFAULT    = 2.0f;

constexpr float VOL_MIN              = 0.0f;
constexpr float VOL_MAX              = 1.0f;
constexpr float VOL_DEFAULT          = 0.2f;

/**
 * @brief Core DSP engine class for 2-operator Frequency Modulation (FM) synthesis.
 * 
 * Implements classic Chowning FM synthesis:
 *   s(t) = Volume * sin(2*pi*fc*t + Index * sin(2*pi*fm*t))
 * where fm = fc * Ratio.
 * 
 * Features lock-free atomic parameter exchange between host thread (FMOD Studio / Game Engine)
 * and the real-time audio thread, strictly adhering to real-time audio constraints:
 * - No dynamic allocations (malloc/new) in the process path.
 * - No locks, mutexes, condition variables, or blocking system calls.
 * - Deterministic, non-throwing audio computation.
 */
class FMSynthDSP
{
public:
    FMSynthDSP() noexcept;
    ~FMSynthDSP() noexcept = default;

    // Non-copyable, non-movable to ensure safe lifetime management in C plugin wrappers
    FMSynthDSP(const FMSynthDSP&) = delete;
    FMSynthDSP& operator=(const FMSynthDSP&) = delete;
    FMSynthDSP(FMSynthDSP&&) = delete;
    FMSynthDSP& operator=(FMSynthDSP&&) = delete;

    /**
     * @brief Resets both carrier and modulator phase accumulators to zero.
     */
    void reset() noexcept;

    /**
     * @brief Audio processing block.
     * 
     * Computes 2-operator FM synthesized audio across all output channels.
     * 
     * @param outBuffer Interleaved float audio buffer to write synthesized samples to.
     * @param length Number of sample frames requested in this block.
     * @param numChannels Number of output channels (1 = mono, 2 = stereo, etc.).
     * @param sampleRate Current audio system sample rate (e.g., 44100, 48000).
     */
    void process(float* outBuffer, unsigned int length, int numChannels, int sampleRate) noexcept;

    /**
     * @brief Sets the carrier frequency atomically.
     * @param freq Target frequency in Hz (automatically clamped to [20.0, 20000.0]).
     */
    void setCarrierFrequency(float freq) noexcept;

    /**
     * @brief Retrieves the current carrier frequency atomically.
     */
    float getCarrierFrequency() const noexcept;

    /**
     * @brief Sets the modulator ratio atomically.
     * @param ratio Target ratio (automatically clamped to [0.1, 16.0]).
     */
    void setModulatorRatio(float ratio) noexcept;

    /**
     * @brief Retrieves the current modulator ratio atomically.
     */
    float getModulatorRatio() const noexcept;

    /**
     * @brief Sets the modulation index atomically.
     * @param index Target modulation index (automatically clamped to [0.0, 20.0]).
     */
    void setModulationIndex(float index) noexcept;

    /**
     * @brief Retrieves the current modulation index atomically.
     */
    float getModulationIndex() const noexcept;

    /**
     * @brief Sets the output volume atomically.
     * @param vol Target volume (automatically clamped to [0.0, 1.0]).
     */
    void setVolume(float vol) noexcept;

    /**
     * @brief Retrieves the current volume atomically.
     */
    float getVolume() const noexcept;

    /**
     * @brief Fills plugin information fields.
     */
    FMOD_RESULT getInfo(char* outName, unsigned int* outVersion, int* outChannels, int* outConfigWidth, int* outConfigHeight) const noexcept;

private:
    // Atomic parameters accessed concurrently by host thread (set/get) and audio thread (process)
    std::atomic<float> m_carrierFreq{CARRIER_FREQ_DEFAULT};
    std::atomic<float> m_modRatio{MOD_RATIO_DEFAULT};
    std::atomic<float> m_modIndex{MOD_INDEX_DEFAULT};
    std::atomic<float> m_volume{VOL_DEFAULT};

    // Normalized oscillator phase accumulators in range [0.0, 1.0).
    // Accessed strictly on the real-time audio thread.
    float m_carrierPhase{0.0f};
    float m_modulatorPhase{0.0f};
};

} // namespace FMODPlugin

/*
 * C-linkage FMOD DSP Callbacks and Native Entry Point
 */
extern "C" {

    FMOD_RESULT F_CALL FMSynthDSP_Create(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL FMSynthDSP_Release(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL FMSynthDSP_Reset(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL FMSynthDSP_Process(FMOD_DSP_STATE* dsp_state, unsigned int length,
                                         const FMOD_DSP_BUFFER_ARRAY* inbufferarray,
                                         FMOD_DSP_BUFFER_ARRAY* outbufferarray,
                                         FMOD_BOOL inputsidle,
                                         FMOD_DSP_PROCESS_OPERATION op);
    FMOD_RESULT F_CALL FMSynthDSP_ShouldIProcess(FMOD_DSP_STATE* dsp_state,
                                                FMOD_BOOL inputsidle,
                                                unsigned int length,
                                                FMOD_CHANNELMASK inmask,
                                                int inchannels,
                                                FMOD_SPEAKERMODE speakermode);
    FMOD_RESULT F_CALL FMSynthDSP_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value);
    FMOD_RESULT F_CALL FMSynthDSP_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr);
    FMOD_RESULT F_CALL FMSynthDSP_GetInfo(FMOD_DSP_STATE* dsp_state, char* name, unsigned int* version, int* channels, int* configwidth, int* configheight);

    /**
     * @brief Mandatory native entry point queried by FMOD Core and FMOD Studio
     *        when loading this dynamic library.
     */
    F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetDSPDescription();

} // extern "C"
