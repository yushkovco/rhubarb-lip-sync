#include <array>
#include "audio/audioFileReading.h"
#include "audio/processing.h"
#include "audio/DcOffset.h"
#include "audio/Offset.h"
#include "audio/SampleRateConverter.h"

extern "C" {
#include <pocketsphinx.h>
#include <sphinxbase/err.h>
#include <ps_alignment.h>
#include "sphinxbase/cmd_ln.h"
#include "cmdln_macro.h"
}

typedef float Mfcc;
constexpr int mfcSize = 13;
typedef std::array<Mfcc, mfcSize> Mfc;

// Make sure PocketSphinx is configured as expected
static_assert(std::is_same<mfcc_t, float>::value,
	"Expected mfcc_t to be float. Check FIXED_POINT preprocessor variable.");
static_assert(DEFAULT_SAMPLING_RATE == 16000,
	"Unexpected DEFAULT_SAMPLING_RATE value.");
static_assert(DEFAULT_FRAME_RATE == 100,
	"Unexpected DEFAULT_FRAME_RATE value.");
static_assert(DEFAULT_NUM_CEPSTRA == mfcSize,
	"Unexpected DEFAULT_NUM_CEPSTRA value.");
static_assert(sizeof(Mfc) == mfcSize * sizeof(float),
	"Unexpected std::array implementation.");

int main(int argc, char* argv[]) {
	try {
		if (argc != 2) {
			throw std::runtime_error("File name must be specified as single argument.");
		}

		// Shift the audio so that MFCCs are aligned with frames
		const int offset =
			(std::lround(DEFAULT_WINDOW_LENGTH * DEFAULT_SAMPLING_RATE) - DEFAULT_FRAME_SHIFT) / 2;

		const auto audioFile = createAudioFileClip(argv[1])
			| resample(DEFAULT_SAMPLING_RATE)
			| removeDcOffset()
			| addOffset(offset);

		// Disable PocketSphinx' logging
		err_set_logfp(nullptr);

		// Initialize feature extractor
		const arg_t parameterDefinitions[] = {
			waveform_to_cepstral_command_line_macro(),
			CMDLN_EMPTY_OPTION
		};
		cmd_ln_t* config = cmd_ln_init(
			nullptr, parameterDefinitions, true,
			// Disable VAD -- we're doing that ourselves
			"-remove_silence", "no",
			nullptr);
		if (!config) throw std::runtime_error("Error initializing configuration.");
		fe_t* fe = fe_init_auto_r(config);

		// Process audio stream
		fe_start_stream(fe);
		fe_start_utt(fe);
		std::vector<Mfc> mfcs(100 * audioFile->size() / audioFile->getSampleRate());
		std::vector<mfcc_t*> mfcPointers(mfcs.size());
		for (int i = 0; i < mfcs.size(); ++i) {
			mfcPointers[i] = reinterpret_cast<mfcc_t*>(&mfcs[i]);
		}
		NullProgressSink progressSink;
		int writtenMfcCount = 0;
		process16bitAudioClip(*audioFile, [&](const std::vector<int16_t>& samples) {
			auto samplePointer = samples.data();
			size_t sampleCount = samples.size();
			int mfcCount = mfcs.size() - writtenMfcCount; // Variable has double purpose -- see fe_process_frames
			if (mfcCount == 0) return;
			if (fe_process_frames(fe, &samplePointer, &sampleCount, &mfcPointers[writtenMfcCount], &mfcCount, nullptr)) {
				throw std::runtime_error("Error processing frames.");
			}
			writtenMfcCount += mfcCount;
		}, progressSink);
		if (writtenMfcCount < mfcs.size()) {
			int _;
			if (fe_end_utt(fe, mfcPointers[writtenMfcCount], &_)) throw std::runtime_error("Error ending utterance.");
		}

		fe_free(fe);
		cmd_ln_free_r(config);

		for (const Mfc& mfc : mfcs) {
			for (int i = 0; i < mfcSize; ++i) {
				std::cout << mfc[i];
				if (i <mfcSize - 1) {
					std::cout << '\t';
				}
			}
			std::cout << std::endl;
		}
	} catch (const std::exception& exception) {
		std::cerr << "Fatal error: " << exception.what() << std::endl;
	}
}