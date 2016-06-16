#include "SyncDevice.h"
#include "track.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <algorithm>

namespace rocket
{

#ifndef SYNC_PLAYER
#pragma comment(lib, "Ws2_32.lib")
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
	//#define SYNC_PLAYER 1

	SyncDevice::SyncDevice(const std::string& basename, int trackCount)
		: m_base(basename),
		m_tracks(),
		m_row(-1),
		m_socket(INVALID_SOCKET),
		m_IOCb()
	{
		m_tracks.reserve(trackCount);
		m_IOCb.open = [](const std::string& file, const char * mode) { return (void*)fopen(file.c_str(), mode); };
		m_IOCb.read = (size_t(*)(void *, size_t, size_t, void *))fread;
		m_IOCb.close = (int(*)(void *))fclose;
	}

	SyncDevice::~SyncDevice()
	{
#ifndef SYNC_PLAYER
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);
#endif

#if defined(USE_AMITCP) && !defined(SYNC_PLAYER)
		if (socket_base) {
			CloseLibrary(socket_base);
			socket_base = NULL;
		}
#endif
	}

#else

	void SyncDevice::SetIOCallbacks(SyncIOCb&& callbacks)
	{
		m_IOCb = std::move(callbacks);
	}

#endif

	const std::string path_encode(const std::string& path)
	{
		std::string temp;
		temp.resize(255);
		int pos = 0;
		for (char ch : path) {
			if (isalnum(ch) || ch == '.' || ch == '_') {
				temp[pos++] = ch;
			}
			else {
				temp[pos++] = '-';
				temp[pos++] = "0123456789ABCDEF"[(ch >> 4) & 0xF];
				temp[pos++] = "0123456789ABCDEF"[ch & 0xF];
			}
		}
		temp.resize(pos);
		return temp;
	}

	bool SyncDevice::ReadTrackData(Track& track, const std::string& name)
	{
		int i;
		const std::string& path = m_base + "_" + path_encode(name) + ".track";
		void *fp = m_IOCb.open(path, "rb");
		if (!fp)	
			return false;
		int num_keys = -1;
		m_IOCb.read(&num_keys, sizeof(int), 1, fp);

		for (i = 0; i < num_keys; ++i) {
			Track::Key key;
			m_IOCb.read(&key.row, sizeof(int), 1, fp);
			m_IOCb.read(&key.value, sizeof(float), 1, fp);
			m_IOCb.read(&key.type, sizeof(char), 1, fp);
			track.SetKey(std::move(key));
		}
		m_IOCb.close(fp);
		return true;
	}

	bool SyncDevice::SaveTracks()
	{
		for (auto& track : m_tracks) {
			std::ofstream fp(track.GetName(), std::ios_base::out | std::ios_base::binary);
			if (!fp.is_open())
				return false;
			track.SaveKeys(fp);
			fp.close();
		}
		return true;
	}

#ifndef SYNC_PLAYER

	bool SyncDevice::FetchTrackData(const std::string& name)
	{
		unsigned char cmd = GET_TRACK;
		assert(name.length() <= UINT32_MAX);
		uint32_t name_len = htonl(static_cast<uint32_t>(name.length()));

		/* send request data */
		if (xsend(m_socket, reinterpret_cast<char *>(&cmd), 1, 0) ||
			xsend(m_socket, reinterpret_cast<char *>(&name_len), sizeof(name_len), 0) ||
			xsend(m_socket, name.c_str(), name.length(), 0))
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
			return false;

		track = ntohl(track);
		v.i = ntohl(v.i);

		key.row = ntohl(row);
		key.value = v.f;

		assert(type < (int)Track::Key::Type::KEY_TYPE_COUNT);
		assert(track < m_tracks.size());
		key.type = (Track::Key::Type)type;
		auto& trackObject = m_tracks.at(track);
		trackObject.SetKey(std::move(key));
		return true;
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
		auto& trackObject = m_tracks.at(track);
		return trackObject.DelKey(row);
	}

	bool SyncDevice::Connect(const std::string& host, unsigned short port)
	{
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);

		m_socket = ServerConnect(host, port);
		if (m_socket == INVALID_SOCKET)
			return false;

		for (auto& track : m_tracks) {
			track.ClearKeys();
			if (FetchTrackData(track.GetName())) {
				closesocket(m_socket);
				m_socket = INVALID_SOCKET;
				return false;
			}
		}
		return true;
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
		auto it = std::find_if(m_tracks.begin(), m_tracks.end(),[&](Track &track)
			{ return track.GetName() == name; });
		if (it != m_tracks.end())
			return *it;
		m_tracks.emplace_back(Track(name));
		auto& item = m_tracks.back();
#ifndef SYNC_PLAYER
		if (m_socket != INVALID_SOCKET)
		{
			FetchTrackData(name);
		}
		else
#endif
		{
			ReadTrackData(item, name);
		}
		return item;
	}
}