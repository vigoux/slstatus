/* See LICENSE file for copyright and license details. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../util.h"

#if defined(__OpenBSD__)
	#include <sys/audioio.h>

	const char *
	vol_perc(const char *card)
	{
		static int cls = -1;
		mixer_devinfo_t mdi;
		mixer_ctrl_t mc;
		int afd = -1, m = -1, v = -1;

		if ((afd = open(card, O_RDONLY)) < 0) {
			warn("open '%s':", card);
			return NULL;
		}

		for (mdi.index = 0; cls == -1; mdi.index++) {
			if (ioctl(afd, AUDIO_MIXER_DEVINFO, &mdi) < 0) {
				warn("ioctl 'AUDIO_MIXER_DEVINFO':");
				close(afd);
				return NULL;
			}
			if (mdi.type == AUDIO_MIXER_CLASS &&
			    !strncmp(mdi.label.name,
				     AudioCoutputs,
				     MAX_AUDIO_DEV_LEN))
				cls = mdi.index;
			}
		for (mdi.index = 0; v == -1 || m == -1; mdi.index++) {
			if (ioctl(afd, AUDIO_MIXER_DEVINFO, &mdi) < 0) {
				warn("ioctl 'AUDIO_MIXER_DEVINFO':");
				close(afd);
				return NULL;
			}
			if (mdi.mixer_class == cls &&
			    ((mdi.type == AUDIO_MIXER_VALUE &&
			      !strncmp(mdi.label.name,
				       AudioNmaster,
				       MAX_AUDIO_DEV_LEN)) ||
			     (mdi.type == AUDIO_MIXER_ENUM &&
			      !strncmp(mdi.label.name,
				      AudioNmute,
				      MAX_AUDIO_DEV_LEN)))) {
				mc.dev = mdi.index, mc.type = mdi.type;
				if (ioctl(afd, AUDIO_MIXER_READ, &mc) < 0) {
					warn("ioctl 'AUDIO_MIXER_READ':");
					close(afd);
					return NULL;
				}
				if (mc.type == AUDIO_MIXER_VALUE)
					v = mc.un.value.num_channels == 1 ?
					    mc.un.value.level[AUDIO_MIXER_LEVEL_MONO] :
					    (mc.un.value.level[AUDIO_MIXER_LEVEL_LEFT] >
					     mc.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] ?
					     mc.un.value.level[AUDIO_MIXER_LEVEL_LEFT] :
					     mc.un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
				else if (mc.type == AUDIO_MIXER_ENUM)
					m = mc.un.ord;
			}
		}

		close(afd);

		return bprintf("%d", m ? 0 : v * 100 / 255);
	}
#elif defined(__linux__)
	#include <alsa/asoundlib.h>
	#include <alsa/mixer.h>

	const char *
	vol_perc(const char * card)
	{
		long minv, maxv, outvol;
		snd_mixer_t* handle;
		snd_mixer_elem_t* elem;
		snd_mixer_selem_id_t* sid;

		static const char* mix_name = "Master";
		static int mix_index = 0;

		snd_mixer_selem_id_alloca(&sid);

		/* sets simple-mixer index and name */
		snd_mixer_selem_id_set_index(sid, mix_index);
		snd_mixer_selem_id_set_name(sid, mix_name);

		if (snd_mixer_open(&handle, 0) < 0)
			return NULL;
		if (snd_mixer_attach(handle, card) < 0) {
			snd_mixer_close(handle);
			return NULL;
		}
		if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
			snd_mixer_close(handle);
			return NULL;
		}
		if (snd_mixer_load(handle) < 0) {
			snd_mixer_close(handle);
			return NULL;
		}
		elem = snd_mixer_find_selem(handle, sid);
		if (!elem) {
			snd_mixer_close(handle);
			return NULL;
		}

		snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);

		if(snd_mixer_selem_get_playback_volume(elem, 0, &outvol) < 0) {
			snd_mixer_close(handle);
			return NULL;
		}

		/* make the value bound to 100 */
		outvol -= minv;
		maxv -= minv;
		minv = 0;
		outvol = 100 * outvol / maxv; /* make the value bound from 0 to 100 */

		snd_mixer_close(handle);
		return bprintf("%ld", outvol);
	}
#else
	#include <sys/soundcard.h>

	const char *
	vol_perc(const char *card)
	{
		size_t i;
		int v, afd, devmask;
		char *vnames[] = SOUND_DEVICE_NAMES;

		if ((afd = open(card, O_RDONLY | O_NONBLOCK)) < 0) {
			warn("open '%s':", card);
			return NULL;
		}

		if (ioctl(afd, (int)SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			warn("ioctl 'SOUND_MIXER_READ_DEVMASK':");
			close(afd);
			return NULL;
		}
		for (i = 0; i < LEN(vnames); i++) {
			if (devmask & (1 << i) && !strcmp("vol", vnames[i])) {
				if (ioctl(afd, MIXER_READ(i), &v) < 0) {
					warn("ioctl 'MIXER_READ(%ld)':", i);
					close(afd);
					return NULL;
				}
			}
		}

		close(afd);

		return bprintf("%d", v & 0xff);
	}
#endif
