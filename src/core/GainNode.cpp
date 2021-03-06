// License: BSD 2 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#include "LabSound/core/GainNode.h"
#include "LabSound/core/AudioArray.h"
#include "LabSound/core/AudioBus.h"
#include "LabSound/core/AudioNodeInput.h"
#include "LabSound/core/AudioNodeOutput.h"

#include "LabSound/extended/AudioContextLock.h"

#include "internal/Assertions.h"

namespace lab
{

GainNode::GainNode(AudioContext& ac)
    : AudioNode(ac)
    , m_lastGain(1.f)
    , m_sampleAccurateGainValues(AudioNode::ProcessingSizeInFrames)  // FIXME: can probably share temp buffer in context
{
    addInput(std::unique_ptr<AudioNodeInput>(new AudioNodeInput(this)));
    addOutput(std::unique_ptr<AudioNodeOutput>(new AudioNodeOutput(this, 1)));

    m_gain = std::make_shared<AudioParam>("gain", "GAIN", 1.0, 0.0, 10000.0);
    m_params.push_back(m_gain);

    initialize();
}

GainNode::~GainNode()
{
    uninitialize();
}

void GainNode::process(ContextRenderLock &r, int bufferSize)
{
    // FIXME: for some cases there is a nice optimization to avoid processing here, and let the gain change
    // happen in the summing junction input of the AudioNode we're connected to.
    // Then we can avoid all of the following:

    AudioBus * outputBus = output(0)->bus(r);
    ASSERT(outputBus);

    if (!isInitialized() || !input(0)->isConnected())
        outputBus->zero();
    else
    {
        AudioBus * inputBus = input(0)->bus(r);

        if (gain()->hasSampleAccurateValues())
        {
            // Apply sample-accurate gain scaling for precise envelopes, grain windows, etc.
            ASSERT(bufferSize <= m_sampleAccurateGainValues.size());
            if (bufferSize <= m_sampleAccurateGainValues.size())
            {
                float* gainValues_base = m_sampleAccurateGainValues.data();
                float* gainValues = gainValues_base + _scheduler._renderOffset;
                gain()->calculateSampleAccurateValues(r, gainValues, _scheduler._renderLength);
                if (_scheduler._renderOffset > 0)
                    memset(gainValues_base, 0, sizeof(float) * _scheduler._renderOffset);
                int bzero_start = _scheduler._renderOffset + _scheduler._renderLength;
                if (bzero_start < bufferSize)
                    memset(gainValues_base + bzero_start, 0, sizeof(float) * bufferSize - bzero_start);
                outputBus->copyWithSampleAccurateGainValuesFrom(*inputBus, m_sampleAccurateGainValues.data(), bufferSize);
            }
        }
        else
        {
            // Apply the gain with de-zippering into the output bus.
            outputBus->copyWithGainFrom(*inputBus, &m_lastGain, gain()->value());
        }
    }
}

void GainNode::reset(ContextRenderLock & r)
{
    // Snap directly to desired gain.
    m_lastGain = gain()->value();
}

// FIXME: this can go away when we do mixing with gain directly in summing junction of AudioNodeInput
//
// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in which case we must safely
// uninitialize and then re-initialize with the new channel count.
void GainNode::checkNumberOfChannelsForInput(ContextRenderLock & r, AudioNodeInput * input)
{
    if (!input)
        return;

    ASSERT(r.context());

    if (input != this->input(0).get())
        return;

    int numberOfChannels = input->numberOfChannels(r);

    if (isInitialized() && numberOfChannels != output(0)->numberOfChannels())
    {
        // We're already initialized but the channel count has changed.
        uninitialize();
    }

    if (!isInitialized())
    {
        // This will propagate the channel count to any nodes connected further downstream in the graph.
        output(0)->setNumberOfChannels(r, numberOfChannels);
        initialize();
    }

    AudioNode::checkNumberOfChannelsForInput(r, input);
}

}  // namespace lab
