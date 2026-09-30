#pragma once

#include "fmod_common.h"
#include "fmod_dsp.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace FMODPlugin {

/**
 * @brief Identifiers for the parameters exposed by SquareWaveDSP.
 */
enum ParameterIndex : int
{
    PARAM_FREQUENCY = 0, ///< Fundamental frequency in Hertz (Float: 20 Hz - 20,000 Hz)
    PARAM_VOLUME    = 1, ///< Output amplitude volume (Float: 0.0 - 1.0)
    NUM_PARAMETERS  = 2
};

// Parameter Limits and Defaults
constexpr float FREQ_MIN     = 20.0f;
constexpr float FREQ_MAX     = 20000.0f;
constexpr float FREQ_DEFAULT = 440.0f;

constexpr float VOL_MIN      = 0.0f;
constexpr float VOL_MAX      = 1.0f;
constexpr float VOL_DEFAULT  = 0.2f;

/**
 * @brief Core DSP engine class for generating real-time square waves.
 * 
 * Features lock-free atomic parameter exchange between host thread (FMOD Studio / Game Engine)
 * and the real-time audio thread, strictly adhering to real-time audio programming constraints:
 * - No dynamic allocations (malloc/new) in the process path.
 * - No locks, mutexes, condition variables, or blocking system calls.
 * - Deterministic, non-throwing audio computation.
 */
class SquareWaveDSP
{
public:
    SquareWaveDSP() noexcept;
    ~SquareWaveDSP() noexcept = default;

    // Non-copyable, non-movable to ensure safe lifetime management in C plugin wrappers
    SquareWaveDSP(const SquareWaveDSP&) = delete;
    SquareWaveDSP& operator=(const SquareWaveDSP&) = delete;
    SquareWaveDSP(SquareWaveDSP&&) = delete;
    SquareWaveDSP& operator=(SquareWaveDSP&&) = delete;

    /**
     * @brief Resets the internal oscillator phase accumulator to zero.
     */
    void reset() noexcept;

    /**
     * @brief Audio processing block.
     * 
     * Computes square wave samples across all output channels for the given frame count.
     * 
     * @param outBuffer Interleaved float audio buffer to write synthesized samples to.
     * @param length Number of sample frames requested in this block.
     * @param numChannels Number of output channels (1 = mono, 2 = stereo, etc.).
     * @param sampleRate Current audio system sample rate (e.g., 44100, 48000).
     */
    void process(float* outBuffer, unsigned int length, int numChannels, int sampleRate) noexcept;

    /**
     * @brief Sets the oscillator frequency atomically.
     * @param freq Target frequency in Hz (automatically clamped to [20.0, 20000.0]).
     */
    void setFrequency(float freq) noexcept;

    /**
     * @brief Retrieves the current oscillator frequency atomically.
     */
    float getFrequency() const noexcept;

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
    std::atomic<float> m_frequency{FREQ_DEFAULT};
    std::atomic<float> m_volume{VOL_DEFAULT};

    // Normalized oscillator phase in range [0.0, 1.0).
    // Accessed strictly on the real-time audio thread, so no atomic overhead is required.
    float m_phase{0.0f};
};

} // namespace FMODPlugin

/*
 * C-linkage FMOD DSP Callbacks and Native Entry Point
 */
extern "C" {

    FMOD_RESULT F_CALL SquareWaveDSP_Create(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL SquareWaveDSP_Release(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL SquareWaveDSP_Reset(FMOD_DSP_STATE* dsp_state);
    FMOD_RESULT F_CALL SquareWaveDSP_Process(FMOD_DSP_STATE* dsp_state, unsigned int length,
                                            const FMOD_DSP_BUFFER_ARRAY* inbufferarray,
                                            FMOD_DSP_BUFFER_ARRAY* outbufferarray,
                                            FMOD_BOOL inputsidle,
                                            FMOD_DSP_PROCESS_OPERATION op);
    FMOD_RESULT F_CALL SquareWaveDSP_ShouldIProcess(FMOD_DSP_STATE* dsp_state,
                                                   FMOD_BOOL inputsidle,
                                                   unsigned int length,
                                                   FMOD_CHANNELMASK inmask,
                                                   int inchannels,
                                                   FMOD_SPEAKERMODE speakermode);
    FMOD_RESULT F_CALL SquareWaveDSP_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value);
    FMOD_RESULT F_CALL SquareWaveDSP_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr);
    FMOD_RESULT F_CALL SquareWaveDSP_GetInfo(FMOD_DSP_STATE* dsp_state, char* name, unsigned int* version, int* channels, int* configwidth, int* configheight);

    /**
     * @brief Mandatory native entry point queried by FMOD Core and FMOD Studio
     *        when loading this dynamic library.
     */
    F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetDSPDescription();

} // extern "C"
