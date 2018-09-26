#include <stdio.h>
#include <string.h>
#include <array>
#include "audio/audioFileReading.h"
#include "audio/processing.h"

extern "C" {
#include <pocketsphinx.h>
#include <sphinxbase/err.h>
#include <ps_alignment.h>
#include "sphinxbase/cmd_ln.h"
#include "cmdln_macro.h"
}

#include <math.h>

#define EPSILON 0.01
#define TEST_ASSERT(x) if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); exit(1); }
#define TEST_EQUAL(a,b) TEST_ASSERT((a) == (b))
#define TEST_EQUAL_FLOAT(a,b) TEST_ASSERT(fabs((a) - (b)) < EPSILON)
#define LOG_EPSILON 20
#define TEST_EQUAL_LOG(a,b) TEST_ASSERT(abs((a) - (b)) < LOG_EPSILON)

static_assert(
	std::is_same<mfcc_t, float>::value,
	"Expected mfcc_t to be float. Check FIXED_POINT preprocessor variable."
);
typedef float Mfcc;
typedef std::array<Mfcc, DEFAULT_NUM_CEPSTRA> Mfc;
static_assert(
	sizeof(Mfc) == DEFAULT_NUM_CEPSTRA * sizeof(float),
	"Unexpected std::array implementation."
);

int main() {
	const auto audioFile = createAudioFileClip("C:\\Users\\Daniel\\Desktop\\av\\misc\\smearing-fruit.wav");

	err_set_logfp(nullptr);
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
		for (const Mfcc mfcc : mfc) {
			std::cout << mfcc << '\t';
		}
		std::cout << std::endl;
	}
}