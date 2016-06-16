#ifndef SYNC_DEVICE_H
#define SYNC_DEVICE_H

#include "base.h"
#include "track.h"
#include <map>
#include <vector>

#ifndef SYNC_PLAYER

/* configure socket-stack */
#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define USE_GETADDRINFO
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <windows.h>
 #include <limits.h>
#elif defined(USE_AMITCP)
 #include <sys/socket.h>
 #include <proto/exec.h>
 #include <proto/socket.h>
 #include <netdb.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
 #define select(n,r,w,e,t) WaitSelect(n,r,w,e,t,0)
 #define closesocket(x) CloseSocket(x)
#else
 #include <sys/socket.h>
 #include <sys/time.h>
 #include <netinet/in.h>
 #include <netdb.h>
 #include <unistd.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
 #define closesocket(x) close(x)
#endif

#endif /* !defined(SYNC_PLAYER) */
namespace rocket
{
	struct SynCb {
		void(*pause)(void *, bool);
		void(*set_row)(void *, int);
		bool(*is_playing)(void *);
	};
	struct SyncIOCb {
		void *(*open)(const std::string& filename, const char *mode);
		size_t(*read)(void *ptr, size_t size, size_t nitems, void *stream);
		int(*close)(void *stream);
	};

	class SyncDevice {
	public:
		SyncDevice(const std::string&, int trackCount);
		~SyncDevice();
		bool Connect(const std::string&, unsigned short);
		bool Update(int, SynCb&, void *);
		void SetIOCallbacks(SyncIOCb&& callbacks);
		Track& GetTrack(const std::string&);
	private:
		bool FetchTrackData(const std::string&);
		bool ReadTrackData(Track&, const std::string&);
		SOCKET ServerConnect(const std::string&, unsigned short);
		bool SetKeyCmd();
		bool DelKeyCmd();
		bool SaveTracks();
		std::string m_base;
		std::vector<Track> m_tracks;
		size_t num_tracks;

#ifndef SYNC_PLAYER
		int m_row;
		SOCKET m_socket;
#endif
		SyncIOCb m_IOCb;
	};
}
#endif /* SYNC_DEVICE_H */
