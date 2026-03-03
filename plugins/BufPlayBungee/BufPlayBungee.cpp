// BufPlayBungee.cpp
#include "BufPlayBungee.hpp"
#include <cmath>

static InterfaceTable* ft;

namespace BufPlayBungee {
#define MAX_STRETCH 10
    BufPlayBungee::BufPlayBungee()
        : m_stretcher(nullptr)
        , request({})
        , m_fbufnum(-1.f)
        , m_buf(nullptr)
        , m_bufNum(-1)
        , m_inputFramePos(0)
        , m_inputGrain(nullptr)
        , m_overflow(0)
        , m_overflowOffset(0)

    {
        auto* unit = this;
       
        const Bungee::SampleRates sampleRates = {
            static_cast<double>(sampleRate()), // input sample rate
            static_cast<double>(sampleRate())  // output sample rate
        };

        m_stretcher = std::make_unique<Bungee::Stretcher<Bungee::Basic>>(sampleRates, 1);

        const int maxInputFrameCount = BUFLENGTH * MAX_STRETCH* MAX_STRETCH;
        
        request.pitch = std::pow(2., 1. / 12.);

        request.speed = 1.0;

        // Set initial starting position at 0.5 seconds offset from the start of the input buffer.
        request.position = 0.0;

       
        m_stretcher->preroll(request);
       
        m_inputGrain = (float*)RTAlloc(mWorld,2* maxInputFrameCount * sizeof(float));
        memset(m_inputGrain, 0, maxInputFrameCount * sizeof(float));


        // Set the calculation function
        mCalcFunc = make_calc_function<BufPlayBungee, &BufPlayBungee::next>();
        next(1); // Initialize
    }

    BufPlayBungee::~BufPlayBungee()
    {
        if (m_inputGrain)
            RTFree(mWorld, m_inputGrain);
    }

    void BufPlayBungee::next(int nSamples)
    {
        auto* unit = this;
        // --- Get buffer and validate ---
        SIMPLE_GET_BUF_SHARED

            if (m_fbufnum < 0 || !m_buf || m_buf->frames <= 0 || m_buf->channels != 1) {
                // Output silence if buffer is invalid or not stereo (for this simplified example)
                ClearUnitOutputs(this, nSamples);
                return;
            }

        // Update buffer info if buffer number changed
        if (m_bufNum != static_cast<int>(m_fbufnum)) {
            m_bufNum = static_cast<int>(m_fbufnum);
            // Reset stream state? Bungee might need a reset when switching buffers.
            // This is an advanced detail.
        }


        const float* speed = in(1);
        const float* pitch = in(2);
        const float position = sc_frac(in(3)[0]) * m_buf->frames;

        if (in(4)[0] > 0.5f)
        {
            request.position = position;
        }

        request.speed = sc_min(10.f, sc_max(m_buf->samplerate * speed[0] / sampleRate(), 0.1f));
        request.pitch = pitch[0];
        
        int samplesObtained = 0;
        if (m_overflow > 0)
        {
            auto actualSamples = sc_min(m_overflow, nSamples);
            memcpy(out(0), m_inputGrain + m_overflowOffset, actualSamples * sizeof(float));
            m_overflowOffset += actualSamples;
            m_overflow -= actualSamples;
            if(m_overflow>0)
            {
                return;
            }
            m_overflowOffset = 0;
            samplesObtained = actualSamples;
        }
        while (samplesObtained < nSamples)
        {
            auto inputChunk = m_stretcher->specifyGrain(request);

            const int inGrainLen = inputChunk.end - inputChunk.begin;
            if (inGrainLen > m_buf->frames)
                return;

            while (inputChunk.begin > m_buf->frames && inputChunk.end > m_buf->frames)
            {
                inputChunk.begin -= m_buf->frames;
                inputChunk.end -= m_buf->frames;

            }

            if (inputChunk.begin < 0 && inputChunk.end <= m_buf->frames)
            {

                auto pos = m_buf->frames + inputChunk.begin;
                auto amt = -inputChunk.begin;
                memcpy(m_inputGrain, &m_buf->data[pos], sizeof(float) * amt);
                memcpy(m_inputGrain + amt, &m_buf->data[0], sizeof(float) * inputChunk.end);
            }
            else if (inputChunk.end > m_buf->frames)
            {        
                auto pos = inputChunk.begin;
                auto amt = m_buf->frames-inputChunk.begin;
                memcpy(m_inputGrain, &m_buf->data[pos], sizeof(float) * amt);
                memcpy(m_inputGrain + amt, &m_buf->data[0], sizeof(float) * (inputChunk.end-m_buf->frames));
            }
            else
            {
                memcpy(m_inputGrain, &m_buf->data[inputChunk.begin], sizeof(float) * inGrainLen);
            }

            m_stretcher->analyseGrain(m_inputGrain, 0);

            Bungee::OutputChunk outputChunk;
            m_stretcher->synthesiseGrain(outputChunk);

            auto framesNeeded = nSamples - samplesObtained;
            if (outputChunk.frameCount == framesNeeded)
            {
                memcpy(out(0)+samplesObtained, outputChunk.data, sizeof(float) * framesNeeded);
                samplesObtained += framesNeeded;
            }
            else if (outputChunk.frameCount < framesNeeded)
            {
                memcpy(out(0)+samplesObtained, outputChunk.data, sizeof(float) * outputChunk.frameCount);
                samplesObtained += outputChunk.frameCount;
            }
            else //>framesNeeded
            {
                memcpy(out(0)+samplesObtained, outputChunk.data, sizeof(float) * framesNeeded);
                samplesObtained += framesNeeded;
                auto overflow = outputChunk.frameCount - framesNeeded;
                memcpy(m_inputGrain, outputChunk.data + framesNeeded, sizeof(float) * overflow);
                m_overflow += overflow;
                m_overflowOffset = 0;
                m_stretcher->next(request);
                break;
            }
            m_stretcher->next(request);

        }       
    }

} // namespace BufPlayBungee

PluginLoad(BufPlayBungeeUGens) {
    ft = inTable;
    registerUnit<BufPlayBungee::BufPlayBungee>(ft, "BufPlayBungee");
}