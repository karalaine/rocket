#ifndef SYNC_TRACK_H
#define SYNC_TRACK_H

#include <string>
#include <cstdlib>
#include <set>
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
			bool operator<(const Key& r) const { 
				return row < r.row; 
			}
		};
		Track() = default;
		Track(const std::string& name);
		Track(const Track& other) = delete;
		Track& operator=(const Track& other) = delete;
		Track(Track&& other);
		Track& operator=(Track&& other);

		double GetVal(int) const;
		void SetKey(Key&& key);
		bool DelKey(int);
		void ClearKeys();
		void SaveKeys(std::ofstream& stream);
		const std::string& GetName() const;
	private:
		std::set<Track::Key>::iterator FindKey(int row) const;

		inline bool IsKeyFrame(int row)
		{
			return FindKey(row) != m_keys.end();
		}
		std::string m_name;
		std::set<Key> m_keys;
	};
}

#endif /* SYNC_TRACK_H */
