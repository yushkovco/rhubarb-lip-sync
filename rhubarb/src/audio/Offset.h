#pragma once

#include "AudioClip.h"

// Adds silence to the start of an audio clip
class Offset : public AudioClip {
public:
	Offset(std::unique_ptr<AudioClip> inputClip, size_type offset);
	std::unique_ptr<AudioClip> clone() const override;
	int getSampleRate() const override;
	size_type size() const override;
private:
	SampleReader createUnsafeSampleReader() const override;

	std::shared_ptr<AudioClip> inputClip;
	size_type offset;
};

inline int Offset::getSampleRate() const {
	return inputClip->getSampleRate();
}

inline AudioClip::size_type Offset::size() const {
	return offset + inputClip->size();
}

AudioEffect addOffset(AudioClip::size_type offset);
