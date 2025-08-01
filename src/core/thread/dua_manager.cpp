/*
 *  Copyright (c) 2020, The OpenThread Authors.
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
 *   This file implements managing DUA.
 */

#include "dua_manager.hpp"

#if OPENTHREAD_CONFIG_DUA_ENABLE || (OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE)

#include "instance/instance.hpp"

namespace ot {

RegisterLogModule("DuaManager");

DuaManager::DuaManager(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mRegistrationTask(aInstance)
    , mIsDuaPending(false)
#if OPENTHREAD_CONFIG_DUA_ENABLE
    , mDuaState(kNotExist)
    , mDadCounter(0)
    , mLastRegistrationTime(0)
#endif
#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
    , mChildIndexDuaRegistering(Mle::kMaxChildren)
#endif
{
    mDelay.mValue = 0;

#if OPENTHREAD_CONFIG_DUA_ENABLE
    mDomainUnicastAddress.InitAsThreadOriginGlobalScope();
    mFixedDuaInterfaceIdentifier.Clear();
#endif

#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
    mChildDuaMask.Clear();
    mChildDuaRegisteredMask.Clear();
#endif
}

void DuaManager::HandleDomainPrefixUpdate(BackboneRouter::DomainPrefixEvent aEvent)
{
    if ((aEvent == BackboneRouter::kDomainPrefixRemoved) || (aEvent == BackboneRouter::kDomainPrefixRefreshed))
    {
        if (mIsDuaPending)
        {
            IgnoreError(Get<Tmf::Agent>().AbortTransaction(&DuaManager::HandleDuaResponse, this));
        }

#if OPENTHREAD_CONFIG_DUA_ENABLE
        RemoveDomainUnicastAddress();
#endif

#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
        if (!mChildDuaMask.IsEmpty())
        {
            mChildDuaMask.Clear();
            mChildDuaRegisteredMask.Clear();
        }
#endif
    }

#if OPENTHREAD_CONFIG_DUA_ENABLE
    switch (aEvent)
    {
    case BackboneRouter::kDomainPrefixUnchanged:
        // In case removed for some reason e.g. the kDuaInvalid response from PBBR forcefully
        VerifyOrExit(!Get<ThreadNetif>().HasUnicastAddress(GetDomainUnicastAddress()));

        OT_FALL_THROUGH;

    case BackboneRouter::kDomainPrefixRefreshed:
    case BackboneRouter::kDomainPrefixAdded:
    {
        const Ip6::Prefix *prefix = Get<BackboneRouter::Leader>().GetDomainPrefix();
        OT_ASSERT(prefix != nullptr);
        mDomainUnicastAddress.mPrefixLength = prefix->GetLength();
        mDomainUnicastAddress.GetAddress().Clear();
        mDomainUnicastAddress.GetAddress().SetPrefix(*prefix);
    }
    break;
    default:
        ExitNow();
    }

    // Apply cached DUA Interface Identifier manually specified.
    if (IsFixedDuaInterfaceIdentifierSet())
    {
        mDomainUnicastAddress.GetAddress().SetIid(mFixedDuaInterfaceIdentifier);
    }
    else
    {
        SuccessOrExit(GenerateDomainUnicastAddressIid());
    }

    AddDomainUnicastAddress();

exit:
    return;
#endif
}

#if OPENTHREAD_CONFIG_DUA_ENABLE
Error DuaManager::GenerateDomainUnicastAddressIid(void)
{
    Error   error;
    uint8_t dadCounter = mDadCounter;

    if ((error = Get<Utils::Slaac>().GenerateIid(mDomainUnicastAddress, dadCounter)) == kErrorNone)
    {
        if (dadCounter != mDadCounter)
        {
            mDadCounter = dadCounter;
            Store();
        }

        LogInfo("Generated DUA: %s", mDomainUnicastAddress.GetAddress().ToString().AsCString());
    }
    else
    {
        LogWarnOnError(error, "generate DUA");
    }

    return error;
}

Error DuaManager::SetFixedDuaInterfaceIdentifier(const Ip6::InterfaceIdentifier &aIid)
{
    Error error = kErrorNone;

    VerifyOrExit(!aIid.IsReserved(), error = kErrorInvalidArgs);
    VerifyOrExit(mFixedDuaInterfaceIdentifier.IsUnspecified() || mFixedDuaInterfaceIdentifier != aIid);

    mFixedDuaInterfaceIdentifier = aIid;
    LogInfo("Set DUA IID: %s", mFixedDuaInterfaceIdentifier.ToString().AsCString());

    if (Get<ThreadNetif>().HasUnicastAddress(GetDomainUnicastAddress()))
    {
        RemoveDomainUnicastAddress();
        mDomainUnicastAddress.GetAddress().SetIid(mFixedDuaInterfaceIdentifier);
        AddDomainUnicastAddress();
    }

exit:
    return error;
}

void DuaManager::ClearFixedDuaInterfaceIdentifier(void)
{
    // Nothing to clear.
    VerifyOrExit(IsFixedDuaInterfaceIdentifierSet());

    if (GetDomainUnicastAddress().GetIid() == mFixedDuaInterfaceIdentifier &&
        Get<ThreadNetif>().HasUnicastAddress(GetDomainUnicastAddress()))
    {
        RemoveDomainUnicastAddress();

        if (GenerateDomainUnicastAddressIid() == kErrorNone)
        {
            AddDomainUnicastAddress();
        }
    }

    LogInfo("Cleared DUA IID: %s", mFixedDuaInterfaceIdentifier.ToString().AsCString());
    mFixedDuaInterfaceIdentifier.Clear();

exit:
    return;
}

void DuaManager::Restore(void)
{
    Settings::DadInfo dadInfo;

    SuccessOrExit(Get<Settings>().Read(dadInfo));
    mDadCounter = dadInfo.GetDadCounter();

exit:
    return;
}

void DuaManager::Store(void)
{
    Settings::DadInfo dadInfo;

    dadInfo.SetDadCounter(mDadCounter);
    Get<Settings>().Save(dadInfo);
}

void DuaManager::AddDomainUnicastAddress(void)
{
    mDuaState             = kToRegister;
    mLastRegistrationTime = TimerMilli::GetNow();
    Get<ThreadNetif>().AddUnicastAddress(mDomainUnicastAddress);
}

void DuaManager::RemoveDomainUnicastAddress(void)
{
    if (mDuaState == kRegistering && mIsDuaPending)
    {
        IgnoreError(Get<Tmf::Agent>().AbortTransaction(&DuaManager::HandleDuaResponse, this));
    }

    mDuaState                        = kNotExist;
    mDomainUnicastAddress.mPreferred = false;
    Get<ThreadNetif>().RemoveUnicastAddress(mDomainUnicastAddress);
}

void DuaManager::UpdateRegistrationDelay(uint8_t aDelay)
{
    if (mDelay.mFields.mRegistrationDelay == 0 || mDelay.mFields.mRegistrationDelay > aDelay)
    {
        mDelay.mFields.mRegistrationDelay = aDelay;

        LogDebg("update regdelay %d", mDelay.mFields.mRegistrationDelay);
        UpdateTimeTickerRegistration();
    }
}

void DuaManager::NotifyDuplicateDomainUnicastAddress(void)
{
    RemoveDomainUnicastAddress();
    mDadCounter++;

    if (GenerateDomainUnicastAddressIid() == kErrorNone)
    {
        AddDomainUnicastAddress();
    }
}
#endif // OPENTHREAD_CONFIG_DUA_ENABLE

void DuaManager::UpdateReregistrationDelay(void)
{
    uint16_t               delay = 0;
    BackboneRouter::Config config;

    VerifyOrExit(Get<BackboneRouter::Leader>().GetConfig(config) == kErrorNone);

    delay = config.mReregistrationDelay > 1 ? Random::NonCrypto::GetUint16InRange(1, config.mReregistrationDelay) : 1;

    if (mDelay.mFields.mReregistrationDelay == 0 || mDelay.mFields.mReregistrationDelay > delay)
    {
        mDelay.mFields.mReregistrationDelay = delay;
        UpdateTimeTickerRegistration();
        LogDebg("update reregdelay %d", mDelay.mFields.mReregistrationDelay);
    }

exit:
    return;
}

void DuaManager::UpdateCheckDelay(uint8_t aDelay)
{
    if (mDelay.mFields.mCheckDelay == 0 || mDelay.mFields.mCheckDelay > aDelay)
    {
        mDelay.mFields.mCheckDelay = aDelay;

        LogDebg("update checkdelay %d", mDelay.mFields.mCheckDelay);
        UpdateTimeTickerRegistration();
    }
}

void DuaManager::HandleNotifierEvents(Events aEvents)
{
    Mle::Mle &mle = Get<Mle::Mle>();

#if OPENTHREAD_CONFIG_DUA_ENABLE
    if (aEvents.Contains(kEventThreadNetdataChanged))
    {
        Lowpan::Context context;
        // Remove a stale DUA address if any.
        if (Get<ThreadNetif>().HasUnicastAddress(Get<DuaManager>().GetDomainUnicastAddress()) &&
            (Get<NetworkData::Leader>().GetContext(Get<DuaManager>().GetDomainUnicastAddress(), context) != kErrorNone))
        {
            RemoveDomainUnicastAddress();
        }
    }
#endif

    VerifyOrExit(mle.IsAttached(), mDelay.mValue = 0);

    if (aEvents.Contains(kEventThreadRoleChanged))
    {
        if (mle.HasRestored())
        {
            UpdateReregistrationDelay();
        }
#if OPENTHREAD_CONFIG_DUA_ENABLE && OPENTHREAD_FTD
        else if (mle.IsRouter())
        {
            // Wait for link establishment with neighboring routers.
            UpdateRegistrationDelay(kNewRouterRegistrationDelay);
        }
        else if (mle.WillBecomeRouterSoon())
        {
            // Will check again in case the device decides to stay REED when jitter timeout expires.
            UpdateRegistrationDelay(mle.GetRouterRoleTransitionTimeout() + kNewRouterRegistrationDelay + 1);
        }
#endif
    }

#if OPENTHREAD_CONFIG_DUA_ENABLE
    if (aEvents.ContainsAny(kEventIp6AddressAdded))
    {
        UpdateRegistrationDelay(kNewDuaRegistrationDelay);
    }
#endif

exit:
    return;
}

void DuaManager::HandleBackboneRouterPrimaryUpdate(BackboneRouter::Leader::State aState,
                                                   const BackboneRouter::Config &aConfig)
{
    OT_UNUSED_VARIABLE(aConfig);

    if (aState == BackboneRouter::Leader::kStateAdded || aState == BackboneRouter::Leader::kStateToTriggerRereg)
    {
#if OPENTHREAD_CONFIG_DUA_ENABLE
        if (Get<Mle::Mle>().IsFullThreadDevice() || Get<Mle::Mle>().GetParent().IsThreadVersion1p1())
#endif
        {
            UpdateReregistrationDelay();
        }
    }
}

void DuaManager::HandleTimeTick(void)
{
    bool attempt = false;

#if OPENTHREAD_CONFIG_DUA_ENABLE
    LogDebg("regdelay %d, reregdelay %d, checkdelay %d", mDelay.mFields.mRegistrationDelay,
            mDelay.mFields.mReregistrationDelay, mDelay.mFields.mCheckDelay);

    if ((mDuaState != kNotExist) &&
        (TimerMilli::GetNow() > (mLastRegistrationTime + TimeMilli::SecToMsec(kDuaDadPeriod))))
    {
        mDomainUnicastAddress.mPreferred = true;
    }

    if ((mDelay.mFields.mRegistrationDelay > 0) && (--mDelay.mFields.mRegistrationDelay == 0))
    {
        attempt = true;
    }
#else
    LogDebg("reregdelay %d, checkdelay %d", mDelay.mFields.mReregistrationDelay, mDelay.mFields.mCheckDelay);
#endif

    if ((mDelay.mFields.mCheckDelay > 0) && (--mDelay.mFields.mCheckDelay == 0))
    {
        attempt = true;
    }

    if ((mDelay.mFields.mReregistrationDelay > 0) && (--mDelay.mFields.mReregistrationDelay == 0))
    {
#if OPENTHREAD_CONFIG_DUA_ENABLE
        if (mDuaState != kNotExist)
        {
            mDuaState = kToRegister;
        }
#endif

#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
        mChildDuaRegisteredMask.Clear();
#endif
        attempt = true;
    }

    if (attempt)
    {
        mRegistrationTask.Post();
    }

    UpdateTimeTickerRegistration();
}

void DuaManager::UpdateTimeTickerRegistration(void)
{
    if (mDelay.mValue == 0)
    {
        Get<TimeTicker>().UnregisterReceiver(TimeTicker::kDuaManager);
    }
    else
    {
        Get<TimeTicker>().RegisterReceiver(TimeTicker::kDuaManager);
    }
}

void DuaManager::PerformNextRegistration(void)
{
    Error            error   = kErrorNone;
    Mle::Mle        &mle     = Get<Mle::Mle>();
    Coap::Message   *message = nullptr;
    Tmf::MessageInfo messageInfo(GetInstance());
    Ip6::Address     dua;

    VerifyOrExit(mle.IsAttached());
    VerifyOrExit(Get<BackboneRouter::Leader>().HasPrimary());

    // Only allow one outgoing DUA.req
    VerifyOrExit(!mIsDuaPending);

    // Only send DUA.req when necessary
#if OPENTHREAD_CONFIG_DUA_ENABLE
#if OPENTHREAD_FTD
    if (!mle.IsRouterOrLeader() && mle.WillBecomeRouterSoon())
    {
        UpdateRegistrationDelay(mle.GetRouterRoleTransitionTimeout() + kNewRouterRegistrationDelay + 1);
        ExitNow();
    }
#endif
    VerifyOrExit(mle.IsFullThreadDevice() || mle.GetParent().IsThreadVersion1p1());
#endif // OPENTHREAD_CONFIG_DUA_ENABLE

    {
        bool needReg = false;

#if OPENTHREAD_CONFIG_DUA_ENABLE
        needReg = (mDuaState == kToRegister && mDelay.mFields.mRegistrationDelay == 0);
#endif

#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
        needReg = needReg || (!mChildDuaMask.IsEmpty() && mChildDuaMask != mChildDuaRegisteredMask);
#endif
        VerifyOrExit(needReg);
    }

    // Prepare DUA.req
    message = Get<Tmf::Agent>().NewPriorityConfirmablePostMessage(kUriDuaRegistrationRequest);
    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

#if OPENTHREAD_CONFIG_DUA_ENABLE
    if (mDuaState == kToRegister && mDelay.mFields.mRegistrationDelay == 0)
    {
        dua = GetDomainUnicastAddress();
        SuccessOrExit(error = Tlv::Append<ThreadTargetTlv>(*message, dua));
        SuccessOrExit(error = Tlv::Append<ThreadMeshLocalEidTlv>(*message, mle.GetMeshLocalEid().GetIid()));
        mDuaState             = kRegistering;
        mLastRegistrationTime = TimerMilli::GetNow();
    }
    else
#endif // OPENTHREAD_CONFIG_DUA_ENABLE
    {
#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
        uint32_t lastTransactionTime;
        Child   *child = nullptr;

        OT_ASSERT(mChildIndexDuaRegistering == Mle::kMaxChildren);

        for (Child &iter : Get<ChildTable>().Iterate(Child::kInStateValid))
        {
            uint16_t childIndex = Get<ChildTable>().GetChildIndex(iter);

            if (mChildDuaMask.Has(childIndex) && !mChildDuaRegisteredMask.Has(childIndex))
            {
                mChildIndexDuaRegistering = childIndex;
                break;
            }
        }

        child = Get<ChildTable>().GetChildAtIndex(mChildIndexDuaRegistering);
        SuccessOrAssert(child->GetDomainUnicastAddress(dua));

        SuccessOrExit(error = Tlv::Append<ThreadTargetTlv>(*message, dua));
        SuccessOrExit(error = Tlv::Append<ThreadMeshLocalEidTlv>(*message, child->GetMeshLocalIid()));

        lastTransactionTime = Time::MsecToSec(TimerMilli::GetNow() - child->GetLastHeard());
        SuccessOrExit(error = Tlv::Append<ThreadLastTransactionTimeTlv>(*message, lastTransactionTime));
#endif // OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
    }

    if (!mle.IsFullThreadDevice() && mle.GetParent().IsThreadVersion1p1())
    {
        uint8_t pbbrServiceId;

        SuccessOrExit(error = Get<BackboneRouter::Leader>().GetServiceId(pbbrServiceId));
        mle.GetServiceAloc(pbbrServiceId, messageInfo.GetPeerAddr());
    }
    else
    {
        messageInfo.GetPeerAddr().SetToRoutingLocator(mle.GetMeshLocalPrefix(),
                                                      Get<BackboneRouter::Leader>().GetServer16());
    }

    messageInfo.SetSockAddrToRloc();

    SuccessOrExit(error = Get<Tmf::Agent>().SendMessage(*message, messageInfo, &DuaManager::HandleDuaResponse, this));

    mIsDuaPending   = true;
    mRegisteringDua = dua;
    mDelay.mValue   = 0;

    // Generally Thread 1.2 Router would send DUA.req on behalf for DUA registered by its MTD child.
    // When Thread 1.2 MTD attaches to Thread 1.1 parent, 1.2 MTD should send DUA.req to PBBR itself.
    // In this case, Thread 1.2 sleepy end device relies on fast data poll to fetch the response timely.
    if (!Get<Mle::Mle>().IsRxOnWhenIdle())
    {
        Get<DataPollSender>().SendFastPolls();
    }

    LogInfo("Sent %s for DUA %s", UriToString<kUriDuaRegistrationRequest>(), dua.ToString().AsCString());

exit:
    if (error == kErrorNoBufs)
    {
        UpdateCheckDelay(kNoBufDelay);
    }

    LogWarnOnError(error, "perform next registration");
    FreeMessageOnError(message, error);
}

void DuaManager::HandleDuaResponse(void                *aContext,
                                   otMessage           *aMessage,
                                   const otMessageInfo *aMessageInfo,
                                   otError              aResult)
{
    static_cast<DuaManager *>(aContext)->HandleDuaResponse(AsCoapMessagePtr(aMessage), AsCoreTypePtr(aMessageInfo),
                                                           aResult);
}

void DuaManager::HandleDuaResponse(Coap::Message *aMessage, const Ip6::MessageInfo *aMessageInfo, Error aResult)
{
    OT_UNUSED_VARIABLE(aMessageInfo);
    Error error;

    mIsDuaPending = false;
#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
    mChildIndexDuaRegistering = Mle::kMaxChildren;
#endif

    if (aResult == kErrorResponseTimeout)
    {
        UpdateCheckDelay(KResponseTimeoutDelay);
        ExitNow(error = aResult);
    }

    VerifyOrExit(aResult == kErrorNone, error = kErrorParse);
    OT_ASSERT(aMessage != nullptr);

    VerifyOrExit(aMessage->GetCode() == Coap::kCodeChanged || aMessage->GetCode() >= Coap::kCodeBadRequest,
                 error = kErrorParse);

    error = ProcessDuaResponse(*aMessage);

exit:
    if (error != kErrorResponseTimeout)
    {
        mRegistrationTask.Post();
    }

    LogInfo("Received %s response: %s", UriToString<kUriDuaRegistrationRequest>(), ErrorToString(error));
}

template <>
void DuaManager::HandleTmf<kUriDuaRegistrationNotify>(Coap::Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    OT_UNUSED_VARIABLE(aMessageInfo);

    Error error;

    VerifyOrExit(aMessage.IsPostRequest(), error = kErrorParse);

    if (aMessage.IsConfirmable() && Get<Tmf::Agent>().SendEmptyAck(aMessage, aMessageInfo) == kErrorNone)
    {
        LogInfo("Sent %s ack", UriToString<kUriDuaRegistrationNotify>());
    }

    error = ProcessDuaResponse(aMessage);

exit:
    OT_UNUSED_VARIABLE(error);
    LogInfo("Received %s: %s", UriToString<kUriDuaRegistrationNotify>(), ErrorToString(error));
}

Error DuaManager::ProcessDuaResponse(Coap::Message &aMessage)
{
    Error        error = kErrorNone;
    Ip6::Address target;
    uint8_t      status;

    if (aMessage.GetCode() >= Coap::kCodeBadRequest)
    {
        status = ThreadStatusTlv::kDuaGeneralFailure;
        target = mRegisteringDua;
    }
    else
    {
        SuccessOrExit(error = Tlv::Find<ThreadStatusTlv>(aMessage, status));
        SuccessOrExit(error = Tlv::Find<ThreadTargetTlv>(aMessage, target));
    }

    VerifyOrExit(Get<BackboneRouter::Leader>().IsDomainUnicast(target), error = kErrorDrop);

#if OPENTHREAD_CONFIG_DUA_ENABLE
    if (Get<ThreadNetif>().HasUnicastAddress(target))
    {
        switch (static_cast<ThreadStatusTlv::DuaStatus>(status))
        {
        case ThreadStatusTlv::kDuaSuccess:
            mLastRegistrationTime = TimerMilli::GetNow();
            mDuaState             = kRegistered;
            break;
        case ThreadStatusTlv::kDuaReRegister:
            if (Get<ThreadNetif>().HasUnicastAddress(GetDomainUnicastAddress()))
            {
                RemoveDomainUnicastAddress();
                AddDomainUnicastAddress();
            }
            break;
        case ThreadStatusTlv::kDuaInvalid:
            // Domain Prefix might be invalid.
            RemoveDomainUnicastAddress();
            break;
        case ThreadStatusTlv::kDuaDuplicate:
            NotifyDuplicateDomainUnicastAddress();
            break;
        case ThreadStatusTlv::kDuaNoResources:
        case ThreadStatusTlv::kDuaNotPrimary:
        case ThreadStatusTlv::kDuaGeneralFailure:
            UpdateReregistrationDelay();
            break;
        }
    }
    else
#endif
#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
    {
        Child   *child = nullptr;
        uint16_t childIndex;

        for (Child &iter : Get<ChildTable>().Iterate(Child::kInStateValid))
        {
            if (iter.HasIp6Address(target))
            {
                child = &iter;
                break;
            }
        }

        VerifyOrExit(child != nullptr, error = kErrorNotFound);

        childIndex = Get<ChildTable>().GetChildIndex(*child);

        switch (status)
        {
        case ThreadStatusTlv::kDuaSuccess:
            // Mark as Registered
            if (mChildDuaMask.Has(childIndex))
            {
                mChildDuaRegisteredMask.Add(childIndex);
            }
            break;
        case ThreadStatusTlv::kDuaReRegister:
            // Parent stops registering for the Child's DUA until next Child Update Request
            mChildDuaMask.Remove(childIndex);
            mChildDuaRegisteredMask.Remove(childIndex);
            break;
        case ThreadStatusTlv::kDuaInvalid:
        case ThreadStatusTlv::kDuaDuplicate:
            IgnoreError(child->RemoveIp6Address(target));
            mChildDuaMask.Remove(childIndex);
            mChildDuaRegisteredMask.Remove(childIndex);
            break;
        case ThreadStatusTlv::kDuaNoResources:
        case ThreadStatusTlv::kDuaNotPrimary:
        case ThreadStatusTlv::kDuaGeneralFailure:
            UpdateReregistrationDelay();
            break;
        }

        if (status != ThreadStatusTlv::kDuaSuccess)
        {
            SendAddressNotification(target, static_cast<ThreadStatusTlv::DuaStatus>(status), *child);
        }
    }
#endif // OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE

exit:
    UpdateTimeTickerRegistration();
    return error;
}

#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE
void DuaManager::SendAddressNotification(Ip6::Address              &aAddress,
                                         ThreadStatusTlv::DuaStatus aStatus,
                                         const Child               &aChild)
{
    Coap::Message   *message = nullptr;
    Tmf::MessageInfo messageInfo(GetInstance());
    Error            error;

    message = Get<Tmf::Agent>().NewPriorityConfirmablePostMessage(kUriDuaRegistrationNotify);
    VerifyOrExit(message != nullptr, error = kErrorNoBufs);

    SuccessOrExit(error = Tlv::Append<ThreadStatusTlv>(*message, aStatus));
    SuccessOrExit(error = Tlv::Append<ThreadTargetTlv>(*message, aAddress));

    messageInfo.SetSockAddrToRlocPeerAddrTo(aChild.GetRloc16());

    SuccessOrExit(error = Get<Tmf::Agent>().SendMessage(*message, messageInfo));

    LogInfo("Sent %s for child %04x DUA %s", UriToString<kUriDuaRegistrationNotify>(), aChild.GetRloc16(),
            aAddress.ToString().AsCString());

exit:

    if (error != kErrorNone)
    {
        FreeMessage(message);

        // TODO: (DUA) (P4) may enhance to  guarantee the delivery of DUA.ntf
        LogWarn("Sent %s for child %04x DUA %s Error %s", UriToString<kUriDuaRegistrationNotify>(), aChild.GetRloc16(),
                aAddress.ToString().AsCString(), ErrorToString(error));
    }
}

void DuaManager::HandleChildDuaAddressEvent(const Child &aChild, ChildDuaAddressEvent aEvent)
{
    uint16_t childIndex = Get<ChildTable>().GetChildIndex(aChild);

    if ((aEvent == kAddressRemoved || aEvent == kAddressChanged) && mChildDuaMask.Has(childIndex))
    {
        // Abort on going proxy DUA.req for this child
        if (mChildIndexDuaRegistering == childIndex)
        {
            IgnoreError(Get<Tmf::Agent>().AbortTransaction(&DuaManager::HandleDuaResponse, this));
        }

        mChildDuaMask.Remove(childIndex);
        mChildDuaRegisteredMask.Remove(childIndex);
    }

    if (aEvent == kAddressAdded || aEvent == kAddressChanged ||
        (aEvent == kAddressUnchanged && !mChildDuaMask.Has(childIndex)))
    {
        if (mChildDuaMask == mChildDuaRegisteredMask)
        {
            UpdateCheckDelay(Random::NonCrypto::GetUint8InRange(1, BackboneRouter::kParentAggregateDelay));
        }

        mChildDuaMask.Add(childIndex);
        mChildDuaRegisteredMask.Remove(childIndex);
    }
}
#endif // OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE

} // namespace ot

#endif // OPENTHREAD_CONFIG_DUA_ENABLE || (OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE)
