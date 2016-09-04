#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <algorithm>

#include "sync.h"
#include "track.h"
#include "base.h"

namespace rocket
{
	double key_linear(const Track::Key& key0, const Track::Key& key1, double row)
	{
		double t = (row - key0.row) / (key1.row - key0.row);
		return key0.value + (key1.value - key0.value) * t;
	}

	double key_smooth(const Track::Key& key0, const Track::Key& key1, double row)
	{
		double t = (row - key0.row) / (key1.row - key0.row);
		t = t * t * (3 - 2 * t);
		return key0.value + (key1.value - key0.value) * t;
	}

	double key_ramp(const Track::Key& key0, const Track::Key& key1, double row)
	{
		double t = (row - key0.row) / (key1.row - key0.row);
		t = pow(t, 2.0);
		return key0.value + (key1.value - key0.value) * t;
	}

	Track::Track(const std::string & name)
		: m_name(name),
		m_keys()
	{
	}

	Track::Track(Track && other)
		: m_name(std::move(other.m_name)),
		m_keys(std::move(other.m_keys))
	{ 
	}

	Track & Track::operator=(Track && other)
	{ 
		m_name = std::move(other.m_name);
		m_keys = std::move(other.m_keys);
		return *this;
	}


	double Track::GetVal(int row) const
	{
		/* If we have no keys at all, return a constant 0 */
		if (m_keys.size() == 0)
			return 0.0f;

		auto higher = std::upper_bound(m_keys.begin(), std::prev(m_keys.end()), row, [](int value, const Track::Key& key)
		{
			return value < key.row;
		});

		if(higher == m_keys.begin())
			return higher->value;

		auto lower = std::lower_bound(m_keys.begin(), m_keys.end(), row, [](const Track::Key& key, int value)
		{
			return key.row < value;
		});

		if (lower != m_keys.begin())
			lower--;

		if (lower == std::prev(m_keys.end()))
			return lower->value;

		/* interpolate according to key-type */
		switch (lower->type) {
		case Track::Key::Type::KEY_STEP:
			return lower->value;
		case Track::Key::Type::KEY_LINEAR:
			return key_linear(*lower,*(std::next(lower)), row);
		case Track::Key::Type::KEY_SMOOTH:
			return key_smooth(*lower, *(std::next(lower)), row);
		case Track::Key::Type::KEY_RAMP:
			return key_ramp(*lower, *(std::next(lower)), row);
		default:
			assert(0);
			return 0.0f;
		}
	}

#ifndef SYNC_PLAYER
	void Track::SetKey(Key&& key)
	{
		auto result = m_keys.emplace(std::move(key));
		if (result.second == false)
		{
			//Key was updated
			std::set<Key>::iterator hint = result.first;
			hint++;
			m_keys.erase(result.first);
			m_keys.emplace(key);
		}
	}

	bool Track::DelKey(int pos)
	{
		auto it = FindKey(pos);
		if (it != m_keys.end())
		{
			m_keys.erase(it);
			return true;
		}
		return false;
	}
	void Track::ClearKeys()
	{
		m_keys.clear();
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
	const std::string & Track::GetName() const
	{
		return m_name;
	}
	std::set<Track::Key>::iterator Track::FindKey(int row) const
	{
		Key tmp;
		tmp.row = row;
		return m_keys.find(tmp);
	}
}