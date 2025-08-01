/*
 *  Copyright (c) 2021, The OpenThread Authors.
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
 *   This file implements the History Tracker public APIs.
 */

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_HISTORY_TRACKER_ENABLE

#include "instance/instance.hpp"

using namespace ot;

void otHistoryTrackerInitIterator(otHistoryTrackerIterator *aIterator) { AsCoreType(aIterator).Init(); }

const otHistoryTrackerNetworkInfo *otHistoryTrackerIterateNetInfoHistory(otInstance               *aInstance,
                                                                         otHistoryTrackerIterator *aIterator,
                                                                         uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateNetInfoHistory(AsCoreType(aIterator), *aEntryAge);
}

const otHistoryTrackerUnicastAddressInfo *otHistoryTrackerIterateUnicastAddressHistory(
    otInstance               *aInstance,
    otHistoryTrackerIterator *aIterator,
    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateUnicastAddressHistory(AsCoreType(aIterator),
                                                                                           *aEntryAge);
}

const otHistoryTrackerMulticastAddressInfo *otHistoryTrackerIterateMulticastAddressHistory(
    otInstance               *aInstance,
    otHistoryTrackerIterator *aIterator,
    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateMulticastAddressHistory(AsCoreType(aIterator),
                                                                                             *aEntryAge);
}

const otHistoryTrackerMessageInfo *otHistoryTrackerIterateRxHistory(otInstance               *aInstance,
                                                                    otHistoryTrackerIterator *aIterator,
                                                                    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateRxHistory(AsCoreType(aIterator), *aEntryAge);
}

const otHistoryTrackerMessageInfo *otHistoryTrackerIterateTxHistory(otInstance               *aInstance,
                                                                    otHistoryTrackerIterator *aIterator,
                                                                    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateTxHistory(AsCoreType(aIterator), *aEntryAge);
}

const otHistoryTrackerNeighborInfo *otHistoryTrackerIterateNeighborHistory(otInstance               *aInstance,
                                                                           otHistoryTrackerIterator *aIterator,
                                                                           uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateNeighborHistory(AsCoreType(aIterator), *aEntryAge);
}

const otHistoryTrackerRouterInfo *otHistoryTrackerIterateRouterHistory(otInstance               *aInstance,
                                                                       otHistoryTrackerIterator *aIterator,
                                                                       uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateRouterHistory(AsCoreType(aIterator), *aEntryAge);
}

const otHistoryTrackerOnMeshPrefixInfo *otHistoryTrackerIterateOnMeshPrefixHistory(otInstance               *aInstance,
                                                                                   otHistoryTrackerIterator *aIterator,
                                                                                   uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateOnMeshPrefixHistory(AsCoreType(aIterator),
                                                                                         *aEntryAge);
}

const otHistoryTrackerExternalRouteInfo *otHistoryTrackerIterateExternalRouteHistory(
    otInstance               *aInstance,
    otHistoryTrackerIterator *aIterator,
    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateExternalRouteHistory(AsCoreType(aIterator),
                                                                                          *aEntryAge);
}

const otHistoryTrackerDnsSrpAddrInfo *otHistoryTrackerIterateDnsSrpAddrHistory(otInstance               *aInstance,
                                                                               otHistoryTrackerIterator *aIterator,
                                                                               uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateDnsSrpAddrHistory(AsCoreType(aIterator),
                                                                                       *aEntryAge);
}

#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE && OPENTHREAD_CONFIG_BORDER_AGENT_EPHEMERAL_KEY_ENABLE
const otHistoryTrackerBorderAgentEpskcEvent *otHistoryTrackerIterateBorderAgentEpskcEventHistory(
    otInstance               *aInstance,
    otHistoryTrackerIterator *aIterator,
    uint32_t                 *aEntryAge)
{
    AssertPointerIsNotNull(aEntryAge);

    return AsCoreType(aInstance).Get<HistoryTracker::Local>().IterateEpskcEventHistory(AsCoreType(aIterator),
                                                                                       *aEntryAge);
}
#endif

void otHistoryTrackerEntryAgeToString(uint32_t aEntryAge, char *aBuffer, uint16_t aSize)
{
    HistoryTracker::Local::EntryAgeToString(aEntryAge, aBuffer, aSize);
}

#endif // OPENTHREAD_CONFIG_HISTORY_TRACKER_ENABLE
