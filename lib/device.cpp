#include "device.h"
#include "track.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fstream>

namespace rocket
{
	static const char *path_encode(const char *path)
	{
		static char temp[FILENAME_MAX];
		int i, pos = 0;
		int path_len = (int)strlen(path);
		for (i = 0; i < path_len; ++i) {
			int ch = path[i];
			if (isalnum(ch) || ch == '.' || ch == '_') {
				if (pos >= sizeof(temp) - 1)
					break;

				temp[pos++] = (char)ch;
			}
			else {
				if (pos >= sizeof(temp) - 3)
					break;

				temp[pos++] = '-';
				temp[pos++] = "0123456789ABCDEF"[(ch >> 4) & 0xF];
				temp[pos++] = "0123456789ABCDEF"[ch & 0xF];
			}
		}

		temp[pos] = '\0';
		return temp;
	}

#ifndef SYNC_PLAYER

#define CLIENT_GREET "hello, synctracker!"
#define SERVER_GREET "hello, demo!"

	enum {
		SET_KEY = 0,
		DELETE_KEY = 1,
		GET_TRACK = 2,
		SET_ROW = 3,
		PAUSE = 4,
		SAVE_TRACKS = 5
	};

	static inline int socket_poll(SOCKET socket)
	{
		struct timeval to = { 0, 0 };
		fd_set fds;

		FD_ZERO(&fds);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif
		FD_SET(socket, &fds);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

		return select((int)socket + 1, &fds, NULL, NULL, &to) > 0;
	}

	static inline int xsend(SOCKET s, const void *buf, size_t len, int flags)
	{
#ifdef WIN32
		assert(len <= INT_MAX);
		return send(s, (const char *)buf, (int)len, flags) != (int)len;
#else
		return send(s, (const char *)buf, len, flags) != (int)len;
#endif
	}

	static inline int xrecv(SOCKET s, void *buf, size_t len, int flags)
	{
#ifdef WIN32
		assert(len <= INT_MAX);
		return recv(s, (char *)buf, (int)len, flags) != (int)len;
#else
		return recv(s, (char *)buf, len, flags) != (int)len;
#endif
	}

#ifdef USE_AMITCP
	static struct Library *socket_base = NULL;
#endif

	SOCKET SyncDevice::ServerConnect(const std::string& host, unsigned short nport)
	{
		SOCKET sock = INVALID_SOCKET;
#ifdef USE_GETADDRINFO
		struct addrinfo *addr, *curr;
		char port[6];
#else
		struct hostent *he;
		char **ap;
#endif

#ifdef WIN32
		static int need_init = 1;
		if (need_init) {
			WSADATA wsa;
			if (WSAStartup(MAKEWORD(2, 0), &wsa))
				return INVALID_SOCKET;
			need_init = 0;
		}
#elif defined(USE_AMITCP)
		if (!socket_base) {
			socket_base = OpenLibrary("bsdsocket.library", 4);
			if (!socket_base)
				return INVALID_SOCKET;
		}
#endif

#ifdef USE_GETADDRINFO

		snprintf(port, sizeof(port), "%u", nport);
		if (getaddrinfo(host.c_str(), port, 0, &addr) != 0)
			return INVALID_SOCKET;

		for (curr = addr; curr; curr = curr->ai_next) {
			int family = curr->ai_family;
			struct sockaddr *sa = curr->ai_addr;
			int sa_len = (int)curr->ai_addrlen;

#else

		he = gethostbyname(host);
		if (!he)
			return INVALID_SOCKET;

		for (ap = he->h_addr_list; *ap; ++ap) {
			int family = he->h_addrtype;
			struct sockaddr_in sin;
			struct sockaddr *sa = (struct sockaddr *)&sin;
			int sa_len = sizeof(*sa);

			sin.sin_family = he->h_addrtype;
			sin.sin_port = htons(nport);
			memcpy(&sin.sin_addr, *ap, he->h_length);
			memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

#endif

			sock = socket(family, SOCK_STREAM, 0);
			if (sock == INVALID_SOCKET)
				continue;

			if (connect(sock, sa, sa_len) >= 0) {
				char greet[128];

				if (xsend(sock, CLIENT_GREET, strlen(CLIENT_GREET), 0) ||
					xrecv(sock, greet, strlen(SERVER_GREET), 0)) {
					closesocket(sock);
					sock = INVALID_SOCKET;
					continue;
				}

				if (!strncmp(SERVER_GREET, greet, strlen(SERVER_GREET)))
					break;
			}

			closesocket(sock);
			sock = INVALID_SOCKET;
		}

#ifdef USE_GETADDRINFO
		freeaddrinfo(addr);
#endif

		return sock;
	}

#else

	void sync_set_io_cb(struct sync_device *d, struct sync_io_cb *cb)
	{
		d->io_cb.open = cb->open;
		d->io_cb.read = cb->read;
		d->io_cb.close = cb->close;
	}

#endif

#ifdef NEED_STRDUP
	static inline char *rocket_strdup(const char *str)
	{
		char *ret = malloc(strlen(str) + 1);
		if (ret)
			strcpy(ret, str);
		return ret;
	}
#define strdup rocket_strdup
#endif

	struct sync_device *sync_create_device(const char *base)
	{
		struct sync_device *d = static_cast<sync_device *>(malloc(sizeof(*d)));
		if (!d)
			return NULL;

		d->base = strdup(path_encode(base));
		if (!d->base) {
			free(d);
			return NULL;
		}

		d->tracks = NULL;
		d->num_tracks = 0;

#ifndef SYNC_PLAYER
		d->row = -1;
		m_socket = INVALID_SOCKET;
#endif

		d->io_cb.open = (void *(*)(const char *, const char *))fopen;
		d->io_cb.read = (size_t(*)(void *, size_t, size_t, void *))fread;
		d->io_cb.close = (int(*)(void *))fclose;

		return d;
	}

	void sync_destroy_device(sync_device *d)
	{
		int i;

#ifndef SYNC_PLAYER
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);
#endif

		for (i = 0; i < (int)d->num_tracks; ++i) {
			free(d->tracks[i]->name);
			free(d->tracks[i]->keys);
			free(d->tracks[i]);
		}
		free(d->tracks);
		free(d->base);
		free(d);

#if defined(USE_AMITCP) && !defined(SYNC_PLAYER)
		if (socket_base) {
			CloseLibrary(socket_base);
			socket_base = NULL;
		}
#endif
	}

	bool SyncDevice::ReadTrackData(const std::string& name)
	{
		int i;
		const std::string& path = m_base + "_" + path_encode(name) + ".track";
		void *fp = m_IOCb.open(path, "rb");
		if (!fp)
			return -1;

		m_IOCb.read(&t->num_keys, sizeof(int), 1, fp);
		t->keys = static_cast<track_key*>(malloc(sizeof(track_key) * t->num_keys));
		if (!t->keys)
			return -1;

		for (i = 0; i < (int)t->num_keys; ++i) {
			struct track_key *key = t->keys + i;
			char type;
			d->io_cb.read(&key->row, sizeof(int), 1, fp);
			d->io_cb.read(&key->value, sizeof(float), 1, fp);
			d->io_cb.read(&type, sizeof(char), 1, fp);
			key->type = (enum key_type)type;
		}

		d->io_cb.close(fp);
		return 0;
	}

	bool SyncDevice::SaveTracks(const std::string& path)
	{
		std::ofstream fp(path, std::ios_base::out | std::ios_base::binary);
		if (!fp.is_open())
			return false;
		for (auto it : m_tracks) {
			auto& track = it.second;
			track.SaveKeys(fp);
		}
		fp.close();
		return true;
	}

#ifndef SYNC_PLAYER

	bool SyncDevice::FetchTrackData(const std::string& name)
	{
		unsigned char cmd = GET_TRACK;
		assert(name.length() <= UINT32_MAX);
		uint32_t name_len = htonl(name.length());

		/* send request data */
		if (xsend(m_socket, (char *)&cmd, 1, 0) ||
			xsend(m_socket, (char *)&name_len, sizeof(name_len), 0) ||
			xsend(m_socket, name.c_str(), (int)name_len, 0))
		{
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			return false;
		}

		return true;
	}

	bool SyncDevice::SetKeyCmd()
	{
		uint32_t track, row;
		union {
			float f;
			uint32_t i;
		} v;
		Track::Key key;
		unsigned char type;

		if (xrecv(m_socket, (char *)&track, sizeof(track), 0) ||
			xrecv(m_socket, (char *)&row, sizeof(row), 0) ||
			xrecv(m_socket, (char *)&v.i, sizeof(v.i), 0) ||
			xrecv(m_socket, (char *)&type, 1, 0))
			return -1;

		track = ntohl(track);
		v.i = ntohl(v.i);

		key.row = ntohl(row);
		key.value = v.f;

		assert(type < (int)Track::Key::Type::KEY_TYPE_COUNT);
		assert(track < m_tracks.size());
		key.type = (Track::Key::Type)type;
		auto trackName = m_trackNames[track];
		auto trackObject = m_tracks.at(trackName);
		return trackObject.SetKey(std::move(key));
	}

	bool SyncDevice::DelKeyCmd()
	{
		uint32_t track, row;

		if (xrecv(m_socket, (char *)&track, sizeof(track), 0) ||
			xrecv(m_socket, (char *)&row, sizeof(row), 0))
			return -1;

		track = ntohl(track);
		row = ntohl(row);
		assert(track < m_tracks.size());
		auto trackName = m_trackNames[track];
		auto trackObject = m_tracks.at(trackName);
		return trackObject.DelKey(row);
	}

	bool SyncDevice::Connect(const std::string& host, unsigned short port)
	{
		int i;
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);

		m_socket = ServerConnect(host, port);
		if (m_socket == INVALID_SOCKET)
			return -1;

		for (auto track : m_tracks) {
			track.second.ClearKeys();
			if (FetchTrackData(track.first)) {
				closesocket(m_socket);
				m_socket = INVALID_SOCKET;
				return -1;
			}
		}
		return 0;
	}

	bool SyncDevice::Update(int row, SynCb& cb, void *cb_param)
	{
		if (m_socket == INVALID_SOCKET)
			return false;

		/* look for new commands */
		while (socket_poll(m_socket)) {
			unsigned char cmd = 0, flag;
			uint32_t new_row;
			if (xrecv(m_socket, (char *)&cmd, 1, 0))
				goto sockerr;

			switch (cmd) {
			case SET_KEY:
				if (!SetKeyCmd())
					goto sockerr;
				break;
			case DELETE_KEY:
				if (!DelKeyCmd())
					goto sockerr;
				break;
			case SET_ROW:
				if (xrecv(m_socket, (char *)&new_row, sizeof(new_row), 0))
					goto sockerr;
				cb.set_row(cb_param, ntohl(new_row));
				break;
			case PAUSE:
				if (xrecv(m_socket, (char *)&flag, 1, 0))
					goto sockerr;
				cb.pause(cb_param, flag);
				break;
			case SAVE_TRACKS:
				SaveTracks();
				break;
			default:
				fprintf(stderr, "unknown cmd: %02x\n", cmd);
				goto sockerr;
			}
		}

		if (cb.is_playing(cb_param)) {
			if (m_row != row && m_socket != INVALID_SOCKET) {
				unsigned char cmd = SET_ROW;
				uint32_t nrow = htonl(row);
				if (xsend(m_socket, (char*)&cmd, 1, 0) ||
					xsend(m_socket, (char*)&nrow, sizeof(nrow), 0))
					goto sockerr;
				m_row = row;
			}
		}
		return true;

	sockerr:
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return false;
	}

#endif /* !defined(SYNC_PLAYER) */

	Track& SyncDevice::GetTrack(const std::string& name)
	{
		auto it = m_tracks.find(name);
		if (it != m_tracks.end())
			return it->second;
		auto t = m_tracks.emplace(name, Track());
#ifndef SYNC_PLAYER
		if (m_socket != INVALID_SOCKET)
			FetchTrackData(name);
		else
#endif
		ReadTrackData(name);
		return t.first->second;
	}
}