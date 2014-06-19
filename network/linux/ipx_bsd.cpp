/* $Id: ipx_bsd.c,v 1.7 2003/10/12 09:17:47 btb Exp $ */
/*
 *
 * IPX driver using BSD style sockets
 * Mostly taken from dosemu
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#ifdef HAVE_NETIPX_IPX_H
#include <netipx/ipx.h>
#else
# include <linux/ipx.h>
# ifndef IPX_TYPE
#  define IPX_TYPE 1
# endif
#endif

#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include "pstypes.h"
#include "ipx_drv.h"
#include "ipx_bsd.h"

#ifndef DOSEMU
#include "mono.h"
#define enter_priv_on ()
#define leave_priv_setting ()
#endif

// ----------------------------------------------------------------------------

typedef union tIpxAddrCast {
	ubyte	b [1];
	uint	i;
} tIpxAddrCast;

// ----------------------------------------------------------------------------

struct ipx_driver ipx_bsd = {
	ipx_bsd_GetMyAddress,
	ipx_bsd_OpenSocket,
	ipx_bsd_CloseSocket,
	ipx_bsd_SendPacket,
	ipx_bsd_ReceivePacket,
	IPXGeneralPacketReady,
	NULL,	// InitNetgameAuxData
	NULL,	// HandleNetgameAuxData
	NULL,	// HandleLeaveGame
	NULL	// SendGamePacket
};

//------------------------------------------------------------------------------

int Fail (const char *fmt, ...);

#define FAIL	return Fail

//------------------------------------------------------------------------------

static int ipx_bsd_GetMyAddress (void)
{
	int sock;
	struct sockaddr_ipx ipxs;
	struct sockaddr_ipx ipxs2;
	uint len;
	int i;

	sock=socket (AF_IPX,SOCK_DGRAM,PF_IPX);
	if (sock == -1) {
		FAIL ("IPX: could not open socket in GetMyAddress\n");
		return (-1);
	}

	/* bind this socket to network 0 */
	ipxs.sipx_family=AF_IPX;
#ifdef IPX_MANUAL_ADDRESS
	memcpy (&ipxs.sipx_network, ipx_MyAddress, 4);
#else
	ipxs.sipx_network=0;
#endif
	ipxs.sipx_port=0;

	if (bind (sock,reinterpret_cast<struct sockaddr*> (&ipxs), sizeof (ipxs)) == -1) {
		FAIL ("IPX: could bind to network 0 in GetMyAddress\n");
		close ( sock );
		return (-1);
	}

	len = sizeof (ipxs2);
	if (getsockname (sock, reinterpret_cast<struct sockaddr*> (&ipxs2), &len) < 0) {
		FAIL ("IPX: could not get socket name in GetMyAddress\n");
		close ( sock );
		return (-1);
	}

	memcpy (ipx_MyAddress, &ipxs2.sipx_network, 4);
	for (i = 0; i < 6; i++) {
		ipx_MyAddress[4+i] = ipxs2.sipx_node[i];
	}
	close ( sock );
	return (0);
}

// ----------------------------------------------------------------------------

static int ipx_bsd_OpenSocket (ipx_socket_t *sk, int port)
{
	int sock;           /* sock here means Linux socket handle */
	int opt;
	struct sockaddr_ipx ipxs;
	uint len;
	struct sockaddr_ipx ipxs2;

	/* DANG_FIXTHIS - kludge to support broken linux IPX stack */
	/* need to convert dynamic socket open into a real socket number */
	/*
	if (port == 0) {
		FAIL ("IPX: using socket %x\n", nextDynamicSocket);
		port = nextDynamicSocket++;
	}
	*/
	/* do a socket call, then bind to this port */
	sock = socket (AF_IPX, SOCK_DGRAM, PF_IPX);
	if (sock == -1) {
		FAIL ("IPX: could not open IPX socket.\n");
		return -1;
	}

#ifdef DOSEMU
	opt = 1;
	/* turn on socket debugging */
	if (d.network) {
		enter_priv_on ();
		if (setsockopt (sock, SOL_SOCKET, SODBG, &opt, sizeof (opt)) == -1) {
			leave_priv_setting ();
			FAIL ("IPX: could not set socket option for debugging.\n");
			return -1;
		}
		leave_priv_setting ();
	}
#endif
	opt = 1;
	/* Permit broadcast output */
	enter_priv_on ();
	if (setsockopt (sock, SOL_SOCKET, SO_BROADCAST,
				   &opt, sizeof (opt)) == -1) {
		leave_priv_setting ();
		FAIL ("IPX: could not set socket option for broadcast.\n");
		return -1;
	}
#ifdef DOSEMU
	/* allow setting the nType field in the IPX header */
	opt = 1;
#if 0 /* this seems to be wrong: IPX_TYPE can only be set on level SOL_IPX */
	if (setsockopt (sock, SOL_SOCKET, IPX_TYPE, &opt, sizeof (opt)) == -1) {
#else
	/* the socket _is_ an IPX socket, hence it first passes ipx_setsockopt ()
	 * in file linux/net/ipx/af_ipx.c. This one handles SOL_IPX itself and
	 * passes SOL_SOCKET-levels down to sock_setsockopt ().
	 * Hence I guess the below is correct (can somebody please verify this?)
	 * -- Hans, June 14 1997
	 */
	if (setsockopt (sock, SOL_IPX, IPX_TYPE, &opt, sizeof (opt)) == -1) {
#endif
		leave_priv_setting ();
		FAIL ("IPX: could not set socket option for nType.\n");
		return -1;
	}
#endif
	ipxs.sipx_family = AF_IPX;

	tIpxAddrCast ipxAddrCast;

	memcpy (ipxAddrCast.b, ipx_MyAddress, sizeof (ipx_MyAddress));
 	ipxs.sipx_network = ipxAddrCast.i;
	/*  ipxs.sipx_network = htonl (MyNetwork); */
	bzero (ipxs.sipx_node, 6);	/* Please fill in my node name */
	ipxs.sipx_port = htons (port);

	/* now bind to this port */
	if (bind (sock, reinterpret_cast<struct sockaddr*> (&ipxs), sizeof (ipxs)) == -1) {
		FAIL ("IPX: could not bind socket to address\n");
		close ( sock );
		leave_priv_setting ();
		return -1;
	}

	if ( port==0 ) {
		len = sizeof (ipxs2);
		if (getsockname (sock, reinterpret_cast<struct sockaddr*> (&ipxs2), &len) < 0) {
			FAIL ("IPX: could not get socket name in IPXOpenSocket\n");
			close ( sock );
			leave_priv_setting ();
			return -1;
		} else {
			port = htons (ipxs2.sipx_port);
			FAIL ("IPX: opened dynamic socket %04x\n", port);
		}
	}
	leave_priv_setting ();
	sk->fd = sock;
	sk->socket = port;
	return 0;
}

// ----------------------------------------------------------------------------

static void ipx_bsd_CloseSocket (ipx_socket_t *mysock) {
	/* now close the file descriptor for the socket, and D2_FREE it */
	close (mysock->fd);
	Fail ("IPX: closing file descriptor on socket %x\n", mysock->socket);
}

// ----------------------------------------------------------------------------

static int ipx_bsd_SendPacket (ipx_socket_t *mysock, IPXPacket_t *IPXHeader, u_char *data, int dataLen) {
	struct sockaddr_ipx ipxs;

	ipxs.sipx_family = AF_IPX;
	/* get destination address from IPX packet header */
	memcpy (&ipxs.sipx_network, IPXHeader->Destination.Network, 4);
	/* if destination address is 0, then send to my net */
	if (ipxs.sipx_network == 0) {
		ipxs.sipx_network = *((uint*) &ipx_MyAddress[0]);
		tIpxAddrCast ipxAddrCast;

		memcpy (ipxAddrCast.b, ipx_MyAddress, sizeof (ipx_MyAddress));
		ipxs.sipx_network = ipxAddrCast.i;
		/*  ipxs.sipx_network = htonl (MyNetwork); */
	}
	memcpy (&ipxs.sipx_node, IPXHeader->Destination.Node, 6);
	memcpy (&ipxs.sipx_port, IPXHeader->Destination.Socket, 2);
	ipxs.sipx_type = IPXHeader->PacketType;
	/*	ipxs.sipx_port=htons (0x452); */
	return sendto (mysock->fd, data, dataLen, 0, reinterpret_cast<struct sockaddr*> (&ipxs), sizeof (ipxs));
}

// ----------------------------------------------------------------------------

static int ipx_bsd_ReceivePacket (ipx_socket_t *s, char *buffer, int bufsize, IPXRecvData_t *rd) {
	uint sz, size;
	struct sockaddr_ipx ipxs;

	sz = sizeof (ipxs);
	if ((size = recvfrom (s->fd, buffer, bufsize, 0, reinterpret_cast<struct sockaddr*> (&ipxs), &sz)) <= 0)
		return size;
	memcpy (rd->src_network, &ipxs.sipx_network, 4);
	memcpy (rd->src_node, ipxs.sipx_node, 6);
	rd->src_socket = ipxs.sipx_port;
	rd->dst_socket = s->socket;
	rd->pktType = ipxs.sipx_type;

	return size;
}

// ----------------------------------------------------------------------------

