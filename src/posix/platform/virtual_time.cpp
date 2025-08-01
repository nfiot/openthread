/*
 *  Copyright (c) 2018, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief
 *   This file implements the posix simulation.
 */

#include "openthread-posix-config.h"
#include "platform-posix.h"

#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/un.h>
#include <unistd.h>

#include "mainloop.hpp"
#include "utils.hpp"

#if OPENTHREAD_POSIX_VIRTUAL_TIME

using namespace ot::Posix;

static const int kMaxNetworkSize = 33;      ///< Well-known ID used by a simulated radio supporting promiscuous mode.
static const int kRcpBasePort    = 9000;    ///< This base port for RCP simulation.
static const int kBasePort       = 18000;   ///< This base port for posix app simulation.
static const int kUsPerSecond    = 1000000; ///< Number of microseconds per second.

static uint64_t sNow        = 0;  ///< Time of simulation.
static int      sSockFd     = -1; ///< Socket used to communicating with simulator.
static uint16_t sPortOffset = 0;  ///< Port offset for simulation.

static bool sUseUnixSocket = false;

static void InitWithEnvs(void)
{
    char *env = getenv("OT_VT_USE_UNIX_SOCKET");
    if (env != nullptr && !strcmp(env, "1"))
    {
        sUseUnixSocket = true;
    }

    env = getenv("PORT_OFFSET");

    if (env != nullptr)
    {
        char *endptr;

        sPortOffset = static_cast<uint16_t>(strtol(env, &endptr, 0));

        if (*endptr != '\0')
        {
            const uint8_t kMsgSize = 40;
            char          msg[kMsgSize];

            snprintf(msg, sizeof(msg), "Invalid PORT_OFFSET: %s", env);
            DieNowWithMessage(msg, OT_EXIT_INVALID_ARGUMENTS);
        }

        sPortOffset *= (kMaxNetworkSize + 1);
    }
}

void virtualTimeInit(uint16_t aNodeId)
{
    InitWithEnvs();

    sSockFd = sUseUnixSocket ? SocketWithCloseExec(AF_UNIX, SOCK_SEQPACKET, 0, kSocketBlock)
                             : SocketWithCloseExec(AF_INET, SOCK_DGRAM, IPPROTO_UDP, kSocketBlock);

    if (sSockFd == -1)
    {
        DieNowWithMessage("socket", OT_EXIT_ERROR_ERRNO);
    }

    if (sUseUnixSocket)
    {
        uint16_t           port = kBasePort + sPortOffset + aNodeId;
        struct sockaddr_un addr;

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "vt.%u.sock", port);

        if (unlink(addr.sun_path) == -1 && errno != ENOENT)
        {
            perror("unlink");
            DieNow(OT_EXIT_ERROR_ERRNO);
        }
        if (bind(sSockFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
        {
            perror("bind");
            DieNow(OT_EXIT_ERROR_ERRNO);
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "vt.%u.sock", kRcpBasePort + sPortOffset);

        if (connect(sSockFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
        {
            perror("connect");
            DieNow(OT_EXIT_ERROR_ERRNO);
        }
    }
    else
    {
        struct sockaddr_in sockaddr;

        memset(&sockaddr, 0, sizeof(sockaddr));
        sockaddr.sin_family = AF_INET;

        sockaddr.sin_port        = htons(kBasePort + sPortOffset + aNodeId);
        sockaddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sSockFd, reinterpret_cast<struct sockaddr *>(&sockaddr), sizeof(sockaddr)) == -1)
        {
            DieNowWithMessage("bind", OT_EXIT_ERROR_ERRNO);
        }
    }
}

void virtualTimeDeinit(void)
{
    if (sSockFd != -1)
    {
        close(sSockFd);
        sSockFd = -1;
    }
}

static void virtualTimeSendEvent(struct VirtualTimeEvent *aEvent, size_t aLength)
{
    ssize_t rval;

    if (sUseUnixSocket)
    {
        rval = send(sSockFd, aEvent, aLength, 0);
    }
    else
    {
        struct sockaddr_in sockaddr;

        memset(&sockaddr, 0, sizeof(sockaddr));
        sockaddr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr);
        sockaddr.sin_port = htons(kRcpBasePort + sPortOffset);

        rval = sendto(sSockFd, aEvent, aLength, 0, reinterpret_cast<struct sockaddr *>(&sockaddr), sizeof(sockaddr));
    }

    if (rval < 0)
    {
        DieNowWithMessage("sendto", OT_EXIT_ERROR_ERRNO);
    }
}

void virtualTimeReceiveEvent(struct VirtualTimeEvent *aEvent)
{
    ssize_t rval = sUseUnixSocket ? recv(sSockFd, aEvent, sizeof(*aEvent), 0)
                                  : recvfrom(sSockFd, aEvent, sizeof(*aEvent), 0, nullptr, nullptr);

    if (rval < 0 || static_cast<size_t>(rval) < offsetof(struct VirtualTimeEvent, mData))
    {
        DieNowWithMessage("recvfrom", (rval < 0) ? OT_EXIT_ERROR_ERRNO : OT_EXIT_FAILURE);
    }

    sNow += aEvent->mDelay;
}

void virtualTimeSendSleepEvent(const struct timeval *aTimeout)
{
    struct VirtualTimeEvent event;

    event.mDelay = static_cast<uint64_t>(aTimeout->tv_sec) * kUsPerSecond + static_cast<uint64_t>(aTimeout->tv_usec);
    event.mEvent = OT_SIM_EVENT_ALARM_FIRED;
    event.mDataLength = 0;

    virtualTimeSendEvent(&event, offsetof(struct VirtualTimeEvent, mData));
}

void virtualTimeSendRadioSpinelWriteEvent(const uint8_t *aData, uint16_t aLength)
{
    struct VirtualTimeEvent event;

    event.mDelay      = 0;
    event.mEvent      = OT_SIM_EVENT_RADIO_SPINEL_WRITE;
    event.mDataLength = aLength;

    memcpy(event.mData, aData, aLength);

    virtualTimeSendEvent(&event, offsetof(struct VirtualTimeEvent, mData) + event.mDataLength);
}

void virtualTimeUpdateFdSet(Mainloop::Context *aContext) { Mainloop::AddToReadFdSet(sSockFd, *aContext); }

void virtualTimeProcess(otInstance *aInstance, const Mainloop::Context *aContext)
{
    struct VirtualTimeEvent event;

    memset(&event, 0, sizeof(event));

    if (Mainloop::IsFdReadable(sSockFd, *aContext))
    {
        virtualTimeReceiveEvent(&event);
    }

    virtualTimeSpinelProcess(aInstance, &event);
    virtualTimeRadioProcess(aInstance, &event);
}

uint64_t otPlatTimeGet(void) { return sNow; }

#endif // OPENTHREAD_POSIX_VIRTUAL_TIME
