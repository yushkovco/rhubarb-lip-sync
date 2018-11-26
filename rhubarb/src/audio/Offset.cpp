#include "Offset.h"

using std::unique_ptr;
using std::make_unique;

Offset::Offset(std::unique_ptr<AudioClip> inputClip, size_type offset) :
	inputClip(std::move(inputClip)),
	offset(offset)
{}

std::unique_ptr<AudioClip> Offset::clone() const {
	return std::make_unique<Offset>(*this);
}

SampleReader Offset::createUnsafeSampleReader() const {
	return[read = inputClip->createSampleReader(), offset = offset](size_type index) {
		return index < offset ? 0.0f : read(index - offset);
	};
}

AudioEffect addOffset(AudioClip::size_type offset) {
	return [offset](std::unique_ptr<AudioClip> inputClip) -> std::unique_ptr<AudioClip> {
		return std::make_unique<Offset>(std::move(inputClip), offset);
	};
}
