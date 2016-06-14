#ifndef SYNC_TRACK_H
#define SYNC_TRACK_H

#include <string>
#include <cstdlib>
#include <vector>
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
		bool SetKey(Key&& key);
		bool DelKey(int);
		void ClearKeys();
		void SaveKeys(std::ofstream& stream);
	private:
		int FindKey(int);
		inline int KeyIdxFloor(int row)
		{
			int idx = FindKey(row);
			if (idx < 0)
				idx = -idx - 2;
			return idx;
		};
		inline bool IsKeyFrame(int row)
		{
			return FindKey(row) >= 0;
		}
		std::string name;
		std::vector<Key> m_keys;
	};
}

#endif /* SYNC_TRACK_H */
