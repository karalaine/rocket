#ifndef SYNC_TRACK_H
#define SYNC_TRACK_H

#include <string>
#include <cstdlib>
#include <list>
#include <fstream>

#include "base.h"

namespace rocket
{
	class Track {
	public:
		struct Key {
			enum class Type {
				KEY_STEP,   /* stay constant */
				KEY_LINEAR, /* lerp to the next value */
				KEY_SMOOTH, /* smooth curve to the next value */
				KEY_RAMP,
				KEY_TYPE_COUNT
			};
			int row;
			float value;
			Type type;

		};

		double GetVal(double);
		void SetKey(Key&& key);
		bool DelKey(int);
		void ClearKeys();
		void SaveKeys(std::ofstream& stream);
	private:
		std::list<Track::Key>::iterator Track::FindKey(int row);

		inline bool IsKeyFrame(int row)
		{
			return FindKey(row) != m_keys.end();
		}
		std::string name;
		std::list<Key> m_keys;
	};
}

#endif /* SYNC_TRACK_H */
