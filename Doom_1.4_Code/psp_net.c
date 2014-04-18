#include <pspkernel.h>
#include <pspdebug.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_apctl.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

#include <pspsdk.h>

#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"
#include "m_swap.h"

#include "doomstat.h"

#include "i_net.h"


#define printf pspDebugScreenPrintf


#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED	5000
#endif


void cleanup_net (void);


//
// NETWORKING
//

/**********************************************************************/
/**********************************************************************/
/* TCP/IP stuff */

static int IP_DOOMPORT = (IPPORT_USERRESERVED + 0x1d);

static int IP_sendsocket = -1;
static int IP_insocket = -1;

static struct sockaddr_in IP_sendaddress[MAXNETNODES];

static void (*netget) (void);
static void (*netsend) (void);


/**********************************************************************/
//
// IP_UDPsocket
//
static int IP_UDPsocket (void)
{
  int s;
  int val = 1;

  // allocate a socket
  s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if (s < 0)
    I_Error ("can't create socket: %s", strerror(errno));

  // set to non-blocking
  if(setsockopt(s, SOL_SOCKET, SO_NONBLOCK, &val, sizeof(val)) < 0)
    I_Error ("can't set socket option: %s", strerror(errno));

  return s;
}

/**********************************************************************/
//
// IP_BindToLocalPort
//
static void IP_BindToLocalPort (int s, int port)
{
  int v;
  struct sockaddr_in address;

  memset (&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY); // let's be anal!
  address.sin_port = htons(port);

  v = bind (s, (struct sockaddr *)&address, sizeof(address));
  if (v == -1)
    I_Error ("BindToPort: bind: %s", strerror(errno));
}


/**********************************************************************/
//
// IP_PacketSend
//
static void IP_PacketSend (void)
{
  int  c;
  doomdata_t sw;

  // byte swap
  sw.checksum = htonl(netbuffer->checksum);
  sw.player = netbuffer->player;
  sw.retransmitfrom = netbuffer->retransmitfrom;
  sw.starttic = netbuffer->starttic;
  sw.numtics = netbuffer->numtics;
  for (c = 0 ; c < netbuffer->numtics; c++) {
    sw.cmds[c].forwardmove = netbuffer->cmds[c].forwardmove;
    sw.cmds[c].sidemove = netbuffer->cmds[c].sidemove;
    sw.cmds[c].angleturn = htons(netbuffer->cmds[c].angleturn);
    sw.cmds[c].consistancy = htons(netbuffer->cmds[c].consistancy);
    sw.cmds[c].chatchar = netbuffer->cmds[c].chatchar;
    sw.cmds[c].buttons = netbuffer->cmds[c].buttons;
  }

  //printf ("sending %i\n",gametic);
  sceKernelDelayThread(10);
  c = sendto (IP_sendsocket , (unsigned char *)&sw, doomcom->datalength,
              0, (struct sockaddr *)&IP_sendaddress[doomcom->remotenode],
              sizeof(IP_sendaddress[doomcom->remotenode]));
  //printf("Sending packet to %s\n", inet_ntoa(IP_sendaddress[doomcom->remotenode].sin_addr));
//  if (c == -1) {
//    if (errno != EWOULDBLOCK)
//      I_Error ("SendPacket error: %s",strerror(errno));
//  }
}


/**********************************************************************/
//
// IP_PacketGet
//
static void IP_PacketGet (void)
{
  int i, c;
  struct sockaddr_in fromaddress;
  socklen_t fromlen;
  doomdata_t sw;

  fromlen = sizeof(fromaddress);
  sceKernelDelayThread(10);
  c = recvfrom (IP_insocket, (unsigned char *)&sw, sizeof(sw), 0,
                (struct sockaddr *)&fromaddress, &fromlen);
  if (c == -1) {
    if (errno != EWOULDBLOCK)
      I_Error ("GetPacket: %s",strerror(errno));
    doomcom->remotenode = -1;  // no packet
    return;
  }

  {
    static int first=1;
    if (first)
      printf("len=%d:p=[0x%x 0x%x] \n", c, *(int*)&sw, *((int*)&sw+1));
    first = 0;
  }

  // find remote node number
  for (i = 0; i < doomcom->numnodes; i++)
    if (fromaddress.sin_addr.s_addr == IP_sendaddress[i].sin_addr.s_addr)
      break;

  if (i == doomcom->numnodes) {
    // packet is not from one of the players (new game broadcast)
    //printf("packet fromaddr unrecognized %08X\n", ntohl(fromaddress.sin_addr.s_addr));
    doomcom->remotenode = -1;  // no packet
    return;
  }

  doomcom->remotenode = i;   // good packet from a game player
  doomcom->datalength = c;

  // byte swap
  netbuffer->checksum = ntohl(sw.checksum);
  netbuffer->player = sw.player;
  netbuffer->retransmitfrom = sw.retransmitfrom;
  netbuffer->starttic = sw.starttic;
  netbuffer->numtics = sw.numtics;

  for (c = 0; c < netbuffer->numtics; c++) {
    netbuffer->cmds[c].forwardmove = sw.cmds[c].forwardmove;
    netbuffer->cmds[c].sidemove = sw.cmds[c].sidemove;
    netbuffer->cmds[c].angleturn = ntohs(sw.cmds[c].angleturn);
    netbuffer->cmds[c].consistancy = ntohs(sw.cmds[c].consistancy);
    netbuffer->cmds[c].chatchar = sw.cmds[c].chatchar;
    netbuffer->cmds[c].buttons = sw.cmds[c].buttons;
  }
  //printf("good packet returned\n");
}


/**********************************************************************/
#if 0
static int IP_GetLocalAddress (void)
{
  char hostname[1024];
  struct hostent* hostentry; // host information entry
  int v;

  // get local address
  v = gethostname (hostname, sizeof(hostname));
  if (v == -1)
    I_Error ("IP_GetLocalAddress : gethostname: errno %d",errno);

  hostentry = gethostbyname (hostname);
  if (!hostentry)
    I_Error ("IP_GetLocalAddress : gethostbyname: couldn't get local host");

  return *(int *)hostentry->h_addr_list[0];
}
#endif

/**********************************************************************/
//
// IP_InitNetwork
//
static void IP_InitNetwork (int i)
{
  struct hostent* hostentry; // host information entry

  // enters with PSP network initialized by the launcher
  printf("IP_InitNetwork()\n");

  netsend = IP_PacketSend;
  netget = IP_PacketGet;
  netgame = true;

  // parse player number and host list
  doomcom->consoleplayer = myargv[i+1][0]-'1';
  doomcom->numnodes = 1; // this node for sure

  printf("IP_InitNetwork: %s", myargv[i+1]);

  i++;
  while (++i < myargc && myargv[i][0] != '-') {
  	memset((void *)&IP_sendaddress[doomcom->numnodes], 0, sizeof(IP_sendaddress[doomcom->numnodes]));
    IP_sendaddress[doomcom->numnodes].sin_family = AF_INET;
    IP_sendaddress[doomcom->numnodes].sin_port = htons(IP_DOOMPORT);
    if (myargv[i][0] == '.') {
      IP_sendaddress[doomcom->numnodes].sin_addr.s_addr = inet_addr (myargv[i]+1);
      printf(" %s", myargv[i]+1);
    } else {
      hostentry = gethostbyname (myargv[i]);
      printf(" %s", myargv[i]);
      if (!hostentry)
        I_Error ("gethostbyname: couldn't find %s", myargv[i]);
      IP_sendaddress[doomcom->numnodes].sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
    }
    doomcom->numnodes++;
  }

  printf("\n");

  doomcom->id = DOOMCOM_ID;
  doomcom->numplayers = doomcom->numnodes;

  // make sockets for reading/writing
  IP_insocket = IP_UDPsocket ();
  IP_sendsocket = IP_UDPsocket ();

  IP_BindToLocalPort (IP_insocket, IP_DOOMPORT);

}

/**********************************************************************/
static void IP_Shutdown (void)
{
  if (IP_insocket != -1) {
    close (IP_insocket);
    IP_insocket = -1;
  }
  if (IP_sendsocket != -1) {
    close (IP_sendsocket);
    IP_sendsocket = -1;
  }
}

/**********************************************************************/
/**********************************************************************/
//
// I_InitNetwork
//
void I_InitNetwork (void)
{
  int i;
  int p;

  printf("I_InitNetwork()\n");

  atexit(cleanup_net);

  doomcom = malloc (sizeof (*doomcom) );
  if (!doomcom)
    I_Error ("I_InitNetwork: Couldn't allocate memory for doomcom.");
  memset (doomcom, 0, sizeof(*doomcom) );

  // set up for network
  i = M_CheckParm ("-dup");
  if (i && i < myargc - 1) {
    doomcom->ticdup = myargv[i+1][0]-'0';
    if (doomcom->ticdup < 1)
      doomcom->ticdup = 1;
    if (doomcom->ticdup > 9)
      doomcom->ticdup = 9;
  } else
    doomcom-> ticdup = 1;

  if (M_CheckParm ("-extratic"))
    doomcom-> extratics = 1;
  else
    doomcom-> extratics = 0;

  p = M_CheckParm ("-port");
  if (p && p < myargc - 1) {
    IP_DOOMPORT = atoi (myargv[p+1]);
    printf ("using alternate port %i\n",IP_DOOMPORT);
  }

  // parse network game options,
  //  -net <consoleplayer> <host> <host> ...
  if ((i = M_CheckParm ("-net")) != 0) {
    p = M_CheckParm ("-typenet");
    if (p && p < myargc - 1)
      p = atoi (myargv[p+1]);
    else
      p = -1;

    switch (p) {
      case 0:
      // TCP/IP Infrastructure
      IP_InitNetwork (i);
      break;
      case 1:
      // TC/IP Ad-Hoc
      case 2:
      // IPX
      case 3:
      // raw serial
      default:
      // single player game
      printf("I_InitNetwork: unsupported network type %d\n", p);
      netgame = false;
      doomcom->id = DOOMCOM_ID;
      doomcom->numplayers = doomcom->numnodes = 1;
      doomcom->deathmatch = false;
      doomcom->consoleplayer = 0;
    }
  } else {
    // single player game
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
  }
}


/**********************************************************************/
void I_NetCmd (void)
{
  if (doomcom->command == CMD_SEND) {
    netsend ();
  } else if (doomcom->command == CMD_GET) {
    netget ();
  } else
    I_Error ("Bad net cmd: %i\n",doomcom->command);
}

/**********************************************************************/
void cleanup_net (void)
{
  IP_Shutdown ();
}

/**********************************************************************/
