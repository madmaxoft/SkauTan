#ifndef AUDIOEFFECTS_H
#define AUDIOEFFECTS_H






#include "AudioDataSource.h"





class AudioFadeOut:
	public AudioDataSourceChain
{
	using Super = AudioDataSourceChain;

public:
	AudioFadeOut(std::unique_ptr<AudioDataSource> && a_Lower);
	AudioFadeOut(AudioDataSource * a_Lower);

	// AudioDataSource overrides:
	virtual void fadeOut(int a_Msec) override;
	virtual size_t read(void * a_Dest, size_t a_MaxLen) override;


private:

	/** Set to true if a FadeOut was requested.
	If true, data being read will get faded out until the end of FadeOut is reached, at which point no more data will
	be returned by the read() function. */
	std::atomic<bool> m_IsFadingOut;

	/** Total number of samples across which the FadeOut should be done.
	Only used when m_IsFadingOut is true.
	Protected against multithreaded access by m_Mtx. */
	int m_FadeOutTotalSamples;

	/** Number of samples that will be faded out, before reaching zero volume.
	Each read operation after a fadeout will decrease this numer by the number of samples read, until it reaches
	zero, at which point the readData() function will abort decoding and return no more data.
	Protected against multithreaded access by m_Mtx. */
	int m_FadeOutRemaining;


	/** Applies FadeOut, if appropriate, to the specified data, and updates the FadeOut progress.
	a_Data is the data to be returned from the read operation, it is assumed to be in complete samples. */
	void applyFadeOut(void * a_Data, size_t a_NumBytes);
};





#endif // AUDIOEFFECTS_H
