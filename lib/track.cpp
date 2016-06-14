#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <algorithm>

#include "sync.h"
#include "track.h"
#include "base.h"

namespace rocket
{
	static double key_linear(const struct track_key k[2], double row)
	{
		double t = (row - k[0].row) / (k[1].row - k[0].row);
		return k[0].value + (k[1].value - k[0].value) * t;
	}

	static double key_smooth(const struct track_key k[2], double row)
	{
		double t = (row - k[0].row) / (k[1].row - k[0].row);
		t = t * t * (3 - 2 * t);
		return k[0].value + (k[1].value - k[0].value) * t;
	}

	static double key_ramp(const struct track_key k[2], double row)
	{
		double t = (row - k[0].row) / (k[1].row - k[0].row);
		t = pow(t, 2.0);
		return k[0].value + (k[1].value - k[0].value) * t;
	}

	double Track::GetVal(double row)
	{
		/* If we have no keys at all, return a constant 0 */
		if (m_keys.size == 0)
			return 0.0f;

		int irow = (int)floor(row);
		auto it = FindKey(row);
		if (it == m_keys.end())
			return false;

		/* at the edges, return the first/last value */
		if (it == m_keys.begin() || it == std::prev(m_keys.end()))
			return it->value;

		/* interpolate according to key-type */
		switch (it->type) {
		case Track::Key::Type::KEY_STEP:
			return it->value;
		case Track::Key::Type::KEY_LINEAR:
			return key_linear(t->keys + idx, row);
		case Track::Key::Type::KEY_SMOOTH:
			return key_smooth(t->keys + idx, row);
		case Track::Key::Type::KEY_RAMP:
			return key_ramp(t->keys + idx, row);
		default:
			assert(0);
			return 0.0f;
		}
	}

	std::list<Track::Key>::iterator Track::FindKey(int row)
	{
		int lo = 0, hi = m_keys.size();
		auto it = std::find_if(m_keys.begin(), m_keys.end(), [row](Key a)
		{
			return a.row == row;
		});
		return it;
	}

#ifndef SYNC_PLAYER
	void Track::SetKey(Key&& key)
	{
		auto it = FindKey(key.row);
		if (it == m_keys.end()) {
			/* no exact hit, we need to put new key in */
			m_keys.emplace_back(std::move(key));
			m_keys.sort([](const Key &a, const Key &b)
			{
				return a.row < b.row;
			});
		}
		else
		{
			(*it) = std::move(key);
		}
	}

	bool Track::DelKey(int pos)
	{
		void *tmp;
		auto it = FindKey(pos);
		if (it == m_keys.end())
			return false;
		m_keys.erase(it);
		return 0;
	}
#endif

	void Track::SaveKeys(std::ofstream & fp)
	{
		int size = m_keys.size();
		fp.write(reinterpret_cast<char*>(&size), sizeof(int));
		for (auto key : m_keys)
		{
			fp.write(reinterpret_cast<char*>(&key.row), sizeof(int));
			fp.write(reinterpret_cast<char*>(&key.value), sizeof(float));
			char type = static_cast<char>(key.type);
			fp.put(type);
		}
	}
}