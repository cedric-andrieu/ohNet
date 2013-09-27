#include <OpenHome/Net/Private/DviSsdpNotifier.h>
#include <OpenHome/Net/Private/DviProtocolUpnp.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Env.h>
#include <OpenHome/Net/Private/DviStack.h>
#include <OpenHome/Net/Private/Ssdp.h>
#include <OpenHome/Private/Printer.h>

#include <climits>

using namespace OpenHome;
using namespace OpenHome::Net;

#undef NOTIFIER_LOG_ENABLE

// SsdpNotifierScheduler

SsdpNotifierScheduler::~SsdpNotifierScheduler()
{
    delete iTimer;
}

SsdpNotifierScheduler::SsdpNotifierScheduler(DvStack& aDvStack, ISsdpNotifyListener& aListener)
    : iType(NULL)
    , iDvStack(aDvStack)
    , iListener(aListener)
{
    Functor functor = MakeFunctor(*this, &SsdpNotifierScheduler::SendNextMsg);
    iTimer = new Timer(iDvStack.Env(), functor);
}

void SsdpNotifierScheduler::Start(TUint aDuration, TUint aMsgCount)
{
    iStop = false;
    iEndTimeMs = Os::TimeInMs(iDvStack.Env().OsCtx()) + aDuration;
    ScheduleNextTimer(aMsgCount);
}

void SsdpNotifierScheduler::Stop()
{
    iStop = true;
}

void SsdpNotifierScheduler::NotifyComplete()
{
}

void SsdpNotifierScheduler::SendNextMsg()
{
    TUint remaining = 0;
    TBool stop = true;
    try {
        stop = (iStop || (remaining = NextMsg()) == 0);
    }
    catch (WriterError&) {}
    catch (NetworkError&) {}
    if (stop) {
#ifdef NOTIFIER_LOG_ENABLE
        Log::Print("++ Notifier completed - %s (%p)\n", iType, this);
#endif
        NotifyComplete();
        iListener.NotifySchedulerComplete(this);
        return;
    }
    else {
        ScheduleNextTimer(remaining);
    }
}

void SsdpNotifierScheduler::ScheduleNextTimer(TUint aRemainingMsgs) const
{
    TUint interval;
    Environment& env = iDvStack.Env();
    const TUint timeNow = Os::TimeInMs(env.OsCtx());
    TInt remaining;
    if (timeNow > iEndTimeMs && timeNow-iEndTimeMs > UINT_MAX/2) {
        // clock may wrap during this series of announcements but it hasn't wrapped yet
        remaining = (UINT_MAX - timeNow) + iEndTimeMs;
    }
    else {
        remaining = iEndTimeMs - timeNow;
    }
    TInt maxUpdateTimeMs = (TInt)iDvStack.Env().InitParams().DvMaxUpdateTimeSecs() * 1000;
    ASSERT(remaining <= maxUpdateTimeMs);
    TInt maxInterval = remaining / (TInt)aRemainingMsgs;
    if (maxInterval < kMinTimerIntervalMs) {
        // we're running behind.  Schedule another timer to run immediately
        interval = 0;
    }
    else {
        interval = env.Random((TUint)maxInterval);
    }
    iTimer->FireIn(interval);
}

void SsdpNotifierScheduler::LogNotifierStart(const TChar* aType)
{
    iType = aType;
#ifdef NOTIFIER_LOG_ENABLE
    Log::Print("++ %s (%p)\n", iType, this);
#endif
}


// MsearchResponse

#define NEXT_MSG_ROOT         (0)
#define NEXT_MSG_UUID         (1)
#define NEXT_MSG_DEVICE_TYPE  (2)
#define NEXT_MSG_SERVICE_TYPE (3)

MsearchResponse::MsearchResponse(DvStack& aDvStack, ISsdpNotifyListener& aListener)
    : SsdpNotifierScheduler(aDvStack, aListener)
    , iAnnouncementData(NULL)
{
    iNotifier = new SsdpMsearchResponder(aDvStack);
}

MsearchResponse::~MsearchResponse()
{
    delete iNotifier;
}

void MsearchResponse::StartAll(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    LogNotifierStart("StartAll");
    TUint nextMsgIndex = NEXT_MSG_ROOT;
    TUint msgCount = 3 + aAnnouncementData.ServiceCount();
    if (!aAnnouncementData.IsRoot()) {
        msgCount--;
        nextMsgIndex = NEXT_MSG_UUID;
    }
    Start(aAnnouncementData, msgCount, nextMsgIndex, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void MsearchResponse::StartRoot(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    LogNotifierStart("StartRoot");
    Start(aAnnouncementData, 1, NEXT_MSG_ROOT, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void MsearchResponse::StartUuid(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    LogNotifierStart("StartUuid");
    Start(aAnnouncementData, 1, NEXT_MSG_UUID, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void MsearchResponse::StartDeviceType(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    LogNotifierStart("StartDeviceType");
    Start(aAnnouncementData, 1, NEXT_MSG_DEVICE_TYPE, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void MsearchResponse::StartServiceType(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const OpenHome::Net::ServiceType& aServiceType, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    LogNotifierStart("StartServiceType");
    TUint index = 0;
    for (;;) {
        const OpenHome::Net::ServiceType& st = aAnnouncementData.Service(index).ServiceType();
        if (st.Domain() == aServiceType.Domain() && st.Name() == aServiceType.Name() && st.Version() == aServiceType.Version()) {
            break;
        }
        index++;
    }
    Start(aAnnouncementData, 1, NEXT_MSG_SERVICE_TYPE + index, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void MsearchResponse::Start(IUpnpAnnouncementData& aAnnouncementData, TUint aTotalMsgs, TUint aNextMsgIndex, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    iAnnouncementData = &aAnnouncementData;
    iNextMsgIndex = aNextMsgIndex;
    iRemainingMsgs = aTotalMsgs;
    static_cast<SsdpMsearchResponder*>(iNotifier)->SetRemote(aRemote, aConfigId, aAdapter);
    iUri.Replace(aUri);
    SsdpNotifierScheduler::Start(aMx * 1000, iRemainingMsgs);
}

TUint MsearchResponse::NextMsg()
{
    switch (iNextMsgIndex)
    {
    case 0:
        iNotifier->SsdpNotifyRoot(iAnnouncementData->Udn(), iUri);
        break;
    case 1:
        iNotifier->SsdpNotifyUuid(iAnnouncementData->Udn(), iUri);
        break;
    case 2:
        iNotifier->SsdpNotifyDeviceType(iAnnouncementData->Domain(), iAnnouncementData->Type(), iAnnouncementData->Version(), iAnnouncementData->Udn(), iUri);
        break;
    default:
        DviService& service = iAnnouncementData->Service(iNextMsgIndex - 3);
        const OpenHome::Net::ServiceType& serviceType = service.ServiceType();
        iNotifier->SsdpNotifyServiceType(serviceType.Domain(), serviceType.Name(), serviceType.Version(), iAnnouncementData->Udn(), iUri);
        break;
    }
    iNextMsgIndex++;
    return --iRemainingMsgs;
}


// DeviceAnnouncement

DeviceAnnouncement::DeviceAnnouncement(DvStack& aDvStack, ISsdpNotifyListener& aListener)
    : SsdpNotifierScheduler(aDvStack, aListener)
    , iSsdpNotifier(aDvStack)
    , iNotifierAlive(iSsdpNotifier)
    , iNotifierByeBye(iSsdpNotifier)
    , iNotifierUpdate(iSsdpNotifier)
    , iCurrentNotifier(NULL)
{
}

void DeviceAnnouncement::StartAlive(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId)
{
    LogNotifierStart("StartAlive");
    iCompleted = Functor();
    Start(iNotifierAlive, aAnnouncementData, aAdapter, aUri, aConfigId, kMsgIntervalMsAlive);
}

void DeviceAnnouncement::StartByeBye(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId, Functor& aCompleted)
{
    LogNotifierStart("StartByeBye");
    iCompleted = aCompleted;
    Start(iNotifierByeBye, aAnnouncementData, aAdapter, aUri, aConfigId, kMsgIntervalMsByeBye);
}

void DeviceAnnouncement::StartUpdate(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId, Functor& aCompleted)
{
    LogNotifierStart("StartUpdate");
    iCompleted = aCompleted;
    Start(iNotifierUpdate, aAnnouncementData, aAdapter, aUri, aConfigId, kMsgIntervalMsUpdate);
}

void DeviceAnnouncement::Start(ISsdpNotify& aNotifier, IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId, TUint aMsgInterval)
{
    iCurrentNotifier = &aNotifier;
    iAnnouncementData = &aAnnouncementData;
    iNextMsgIndex = (iAnnouncementData->IsRoot()? 0 : 1);
    iTotalMsgs = 3 + iAnnouncementData->ServiceCount();
    iUri.Replace(aUri);
    iSsdpNotifier.Start(aAdapter, aConfigId);
    SsdpNotifierScheduler::Start(aMsgInterval * iTotalMsgs, iTotalMsgs);
}

TUint DeviceAnnouncement::NextMsg()
{
    switch (iNextMsgIndex)
    {
    case 0:
        iCurrentNotifier->SsdpNotifyRoot(iAnnouncementData->Udn(), iUri);
        break;
    case 1:
        iCurrentNotifier->SsdpNotifyUuid(iAnnouncementData->Udn(), iUri);
        break;
    case 2:
        iCurrentNotifier->SsdpNotifyDeviceType(iAnnouncementData->Domain(), iAnnouncementData->Type(), iAnnouncementData->Version(), iAnnouncementData->Udn(), iUri);
        break;
    default:
        DviService& service = iAnnouncementData->Service(iNextMsgIndex - 3);
        const OpenHome::Net::ServiceType& serviceType = service.ServiceType();
        iCurrentNotifier->SsdpNotifyServiceType(serviceType.Domain(), serviceType.Name(), serviceType.Version(), iAnnouncementData->Udn(), iUri);
        break;
    }
    iNextMsgIndex++;
    return (iTotalMsgs - iNextMsgIndex);
}

void DeviceAnnouncement::NotifyComplete()
{
    if (iCompleted) {
        iCompleted();
    }
}


// DviSsdpNotifierManager

DviSsdpNotifierManager::DviSsdpNotifierManager(DvStack& aDvStack)
    : iDvStack(aDvStack)
    , iLock("DVDM")
    , iShutdownSem("DVDM", 1)
{
}

DviSsdpNotifierManager::~DviSsdpNotifierManager()
{
    iShutdownSem.Wait();
    
    // nasty way of ensuring NotifySchedulerComplete has released iLock
    iLock.Wait();
    iLock.Signal();

    ASSERT(iActiveResponders.size() == 0);
    ASSERT(iActiveAnnouncers.size() == 0);
    Delete(iFreeResponders);
    Delete(iFreeAnnouncers);
}

void DviSsdpNotifierManager::AnnouncementAlive(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId)
{
    AutoMutex a(iLock);
    Announcer* announcer = GetAnnouncer(aAnnouncementData);
    if (announcer != NULL) {
        try {
            announcer->Announcement().StartAlive(aAnnouncementData, aAdapter, aUri, aConfigId);
        }
        catch (NetworkError& ) {
            ASSERT(TryMove(announcer->Scheduler(), iActiveAnnouncers, iFreeAnnouncers));
            throw;
        }
    }
}

void DviSsdpNotifierManager::AnnouncementByeBye(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId, Functor& aCompleted)
{
    AutoMutex a(iLock);
    Announcer* announcer = GetAnnouncer(aAnnouncementData);
    if (announcer != NULL) {
        try {
            announcer->Announcement().StartByeBye(aAnnouncementData, aAdapter, aUri, aConfigId, aCompleted);
        }
        catch (NetworkError& ) {
            ASSERT(TryMove(announcer->Scheduler(), iActiveAnnouncers, iFreeAnnouncers));
            throw;
        }
    }
}

void DviSsdpNotifierManager::AnnouncementUpdate(IUpnpAnnouncementData& aAnnouncementData, TIpAddress aAdapter, const Brx& aUri, TUint aConfigId, Functor& aCompleted)
{
    AutoMutex a(iLock);
    Announcer* announcer = GetAnnouncer(aAnnouncementData);
    if (announcer != NULL) {
        try {
            announcer->Announcement().StartUpdate(aAnnouncementData, aAdapter, aUri, aConfigId, aCompleted);
        }
        catch (NetworkError& ) {
            ASSERT(TryMove(announcer->Scheduler(), iActiveAnnouncers, iFreeAnnouncers));
            throw;
        }
    }
}

void DviSsdpNotifierManager::MsearchResponseAll(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    AutoMutex a(iLock);
    Responder* responder = GetResponder(aAnnouncementData);
    responder->Response().StartAll(aAnnouncementData, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void DviSsdpNotifierManager::MsearchResponseRoot(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    AutoMutex a(iLock);
    Responder* responder = GetResponder(aAnnouncementData);
    responder->Response().StartRoot(aAnnouncementData, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void DviSsdpNotifierManager::MsearchResponseUuid(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    AutoMutex a(iLock);
    Responder* responder = GetResponder(aAnnouncementData);
    responder->Response().StartUuid(aAnnouncementData, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void DviSsdpNotifierManager::MsearchResponseDeviceType(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    AutoMutex a(iLock);
    Responder* responder = GetResponder(aAnnouncementData);
    responder->Response().StartDeviceType(aAnnouncementData, aRemote, aMx, aUri, aConfigId, aAdapter);
}

void DviSsdpNotifierManager::MsearchResponseServiceType(IUpnpAnnouncementData& aAnnouncementData, const Endpoint& aRemote, TUint aMx, const OpenHome::Net::ServiceType& aServiceType, const Brx& aUri, TUint aConfigId, TIpAddress aAdapter)
{
    AutoMutex a(iLock);
    Responder* responder = GetResponder(aAnnouncementData);
    responder->Response().StartServiceType(aAnnouncementData, aRemote, aMx, aServiceType, aUri, aConfigId, aAdapter);
}

void DviSsdpNotifierManager::Stop(const Brx& aUdn)
{
    iLock.Wait();
    Stop(iActiveResponders, aUdn);
    Stop(iActiveAnnouncers, aUdn);
    iLock.Signal();
}

void DviSsdpNotifierManager::Stop(std::list<Notifier*>& aList, const Brx& aUdn)
{
    std::list<Notifier*>::iterator it = aList.begin();
    while (it != aList.end()) {
        if ((*it)->MatchesDevice(aUdn)) {
            (*it)->Scheduler()->Stop();
        }
        it++;
    }
}

void DviSsdpNotifierManager::Delete(std::list<Notifier*>& aList)
{
    std::list<Notifier*>::iterator it = aList.begin();
    while (it != aList.end()) {
        delete *it;
        it++;
    }
}

DviSsdpNotifierManager::Responder* DviSsdpNotifierManager::GetResponder(IUpnpAnnouncementData& aAnnouncementData)
{
    DviSsdpNotifierManager::Responder* responder;
    if (iFreeResponders.size() == 0) {
        MsearchResponse* msr = new MsearchResponse(iDvStack, *this);
        responder = new Responder(msr);
        iActiveResponders.push_back(responder);
    }
    else {
        responder = static_cast<DviSsdpNotifierManager::Responder*>(iFreeResponders.front());
        iActiveResponders.splice(iActiveResponders.end(), iFreeResponders, iFreeResponders.begin());
    }
    (void)iShutdownSem.Clear();
    responder->SetActive(aAnnouncementData.Udn());
    return responder;
}

DviSsdpNotifierManager::Announcer* DviSsdpNotifierManager::GetAnnouncer(IUpnpAnnouncementData& aAnnouncementData)
{
    DviSsdpNotifierManager::Announcer* announcer;
    if (iFreeAnnouncers.size() == 0) {
        try {
            DeviceAnnouncement* da = new DeviceAnnouncement(iDvStack, *this);
            announcer = new Announcer(da);
            iActiveAnnouncers.push_back(announcer);
        }
        catch (NetworkError&) {
            return NULL;
        }
    }
    else {
        announcer = static_cast<DviSsdpNotifierManager::Announcer*>(iFreeAnnouncers.front());
        iActiveAnnouncers.splice(iActiveAnnouncers.end(), iFreeAnnouncers, iFreeAnnouncers.begin());
    }
    (void)iShutdownSem.Clear();
    announcer->SetActive(aAnnouncementData.Udn());
    return announcer;
}

void DviSsdpNotifierManager::NotifySchedulerComplete(SsdpNotifierScheduler* aScheduler)
{
    iLock.Wait();
    if (!TryMove(aScheduler, iActiveResponders, iFreeResponders)) {
        const TBool matched = TryMove(aScheduler, iActiveAnnouncers, iFreeAnnouncers);
        if (!matched) {
            ASSERTS(); // MSVC will warn about const conditional expr if we ASSERT(TryMove(...
        }
    }
    if (iActiveResponders.size() == 0 && iActiveAnnouncers.size() == 0) {
        iShutdownSem.Signal();
    }
    iLock.Signal();
}

TBool DviSsdpNotifierManager::TryMove(SsdpNotifierScheduler* aScheduler, std::list<Notifier*>& aFrom, std::list<Notifier*>& aTo)
{
    std::list<Notifier*>::iterator it = aFrom.begin();
    while (it != aFrom.end()) {
        if ((*it)->Scheduler() == aScheduler) {
            (*it)->SetInactive();
            aTo.splice(aTo.end(), aFrom, it);
            return true;
        }
        it++;
    }
    return false;
}


// DviSsdpNotifierManager::Notifier

DviSsdpNotifierManager::Notifier::~Notifier()
{
    delete iScheduler;
}

SsdpNotifierScheduler* DviSsdpNotifierManager::Notifier::Scheduler()
{
    return iScheduler;
}

TBool DviSsdpNotifierManager::Notifier::MatchesDevice(const Brx& aUdn) const
{
    return iUdn==aUdn;
}

void DviSsdpNotifierManager::Notifier::SetActive(const Brx& aUdn)
{
    iUdn.Set(aUdn);
}

void DviSsdpNotifierManager::Notifier::SetInactive()
{
    iUdn.Set(Brx::Empty());
}

DviSsdpNotifierManager::Notifier::Notifier(SsdpNotifierScheduler* aScheduler)
    : iScheduler(aScheduler)
{
}


// DviSsdpNotifierManager::Responder

DviSsdpNotifierManager::Responder::Responder(MsearchResponse* aResponder)
    : DviSsdpNotifierManager::Notifier(aResponder)
{
}

MsearchResponse& DviSsdpNotifierManager::Responder::Response()
{
    return *static_cast<MsearchResponse*>(iScheduler);
}


// DviSsdpNotifierManager::Announcer

DviSsdpNotifierManager::Announcer::Announcer(DeviceAnnouncement* aAnnouncer)
    : DviSsdpNotifierManager::Notifier(aAnnouncer)
{
}

DeviceAnnouncement& DviSsdpNotifierManager::Announcer::Announcement()
{
    return *static_cast<DeviceAnnouncement*>(iScheduler);
}
