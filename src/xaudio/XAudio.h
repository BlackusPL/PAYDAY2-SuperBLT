#pragma once

#ifdef ENABLE_XAUDIO

struct ALCdevice;
struct ALCcontext;

namespace sblt
{

	class XAudio
	{
	  public:
		static void Register(void* state);

		// Please don't use, for internal use only
		static XAudio* GetXAudioInstance();

	  private:
		XAudio();
		~XAudio();

		ALCdevice* dev;
		ALCcontext* ctx;
	};

}; // namespace sblt

#endif
