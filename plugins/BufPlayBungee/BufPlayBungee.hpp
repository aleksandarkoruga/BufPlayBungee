// BufPlayBungee.hpp
#pragma once

#include "SC_PlugIn.hpp"
#include <memory>
#include <vector>
#include <bungee/Stream.h> // Include Bungee streaming API

namespace BufPlayBungee {

    class BufPlayBungee : public SCUnit {
    public:
        BufPlayBungee();
        ~BufPlayBungee();

    private:
        void next(int nSamples);

        // Bungee objects
        std::unique_ptr<Bungee::Stretcher<Bungee::Basic>> m_stretcher;
        Bungee::Request request;

        // Buffer management
        float m_fbufnum;
        SndBuf* m_buf;
        int m_bufNum;
        int m_inputFramePos; // Our current read position in the source buffer (in frames)
        float* m_inputGrain;
        int m_overflow;
        int m_overflowOffset;

    };

} // namespace BufPlayBungee