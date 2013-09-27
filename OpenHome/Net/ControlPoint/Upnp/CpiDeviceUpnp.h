/**
 * List(s) of UPnP devices suitable for use in control points
 *
 * Cp prefixes show that classes are intended for use in control points
 * Route inside a name implies that a class requires one instance per network interface
 */

#ifndef HEADER_CPIDEVICEUPNP
#define HEADER_CPIDEVICEUPNP

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Private/Discovery.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/Net/Private/CpiDevice.h>
#include <OpenHome/Net/Private/CpiService.h>
#include <OpenHome/Net/Private/DeviceXml.h>
#include <OpenHome/Net/Private/XmlFetcher.h>
#include <OpenHome/Private/Env.h>

namespace OpenHome {
namespace Net {

class CpiDeviceListUpnp;
class CpStack;
/**
 * UPnP-specific device
 *
 * Can be created in response to either a msearch request or a multicast alive
 * notification.  Uses a timer to remove itself from ots owning list if no
 * subsequent alive message is received within a specified maxage.
 */
class CpiDeviceUpnp : private ICpiProtocol, private ICpiDeviceObserver
{
public:
    CpiDeviceUpnp(CpStack& aCpStack, const Brx& aUdn, const Brx& aLocation, TUint aMaxAgeSecs, IDeviceRemover& aDeviceList, CpiDeviceListUpnp& aList);
    const Brx& Udn() const;
    const Brx& Location() const;
    CpiDevice& Device();

    /**
     * Called to reset the maxage timeout each time an alive message is received.
     * A device should be considered alive while at least one of its services hasn't
     * expired so the underlying timer is only updated if the new max age is later
     * than the current one.
     */
    void UpdateMaxAge(TUint aSeconds);

    void FetchXml();
    void InterruptXmlFetch();
private: // ICpiProtocol
    TBool GetAttribute(const char* aKey, Brh& aValue) const;
    void InvokeAction(Invocation& aInvocation);
    TUint Subscribe(CpiSubscription& aSubscription, const Uri& aSubscriber);
    TUint Renew(CpiSubscription& aSubscription);
    void Unsubscribe(CpiSubscription& aSubscription, const Brx& aSid);
    void NotifyRemovedBeforeReady();
private: // ICpiDeviceObserver
    void Release();
private:
    ~CpiDeviceUpnp();
    void TimerExpired();
    void GetServiceUri(Uri& aUri, const TChar* aType, const ServiceType& aServiceType);
    void XmlFetchCompleted(IAsync& aAsync);
    static TBool UdnMatches(const Brx& aFound, const Brx& aTarget);
private:
    class Invocable : public IInvocable, private INonCopyable
    {
    public:
        Invocable(CpiDeviceUpnp& aDevice);
        virtual void InvokeAction(Invocation& aInvocation);
    private:
        CpiDeviceUpnp& iDevice;
    };
private:
    CpiDevice* iDevice;
    Mutex iLock;
    Brhz iLocation;
    XmlFetch* iXmlFetch;
    Brh iXml;
    DeviceXmlDocument* iDeviceXmlDocument;
    DeviceXml* iDeviceXml;
    Timer* iTimer;
    TUint iExpiryTime;
    IDeviceRemover& iDeviceList;
    CpiDeviceListUpnp* iList;
    Invocable* iInvocable;
    Semaphore iSemReady;
    TBool iRemoved;
    friend class Invocable;
};

/**
 * UPnP-specific list of devices for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnp : public CpiDeviceList, public ISsdpNotifyHandler, private IResumeObserver
{
public:
    void XmlFetchCompleted(CpiDeviceUpnp& aDevice, TBool aError);
protected:
    CpiDeviceListUpnp(CpStack& aCpStack, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnp();

    void StopListeners();

    /**
     * Checks to see if a device is already in the list and updates its maxage
     * timeout if it is.
     * Returns true if an existing device was found.  Returning false implies
     * that this is a new device which should be added to the list.
     * false will always be returned while a list is being refreshed.
     */
    TBool Update(const Brx& aUdn, const Brx& aLocation, TUint aMaxAge);
    // CpiDeviceList
    void Start();
    void Refresh();
    TBool IsDeviceReady(CpiDevice& aDevice);
    TBool IsLocationReachable(const Brx& aLocation) const;
    // ISsdpNotifyHandler
    void SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge);
    void SsdpNotifyUuidAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge);
    void SsdpNotifyDeviceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion,
                                   const Brx& aLocation, TUint aMaxAge);
    void SsdpNotifyServiceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion,
                                    const Brx& aLocation, TUint aMaxAge);
    void SsdpNotifyRootByeBye(const Brx& aUuid);
    void SsdpNotifyUuidByeBye(const Brx& aUuid);
    void SsdpNotifyDeviceTypeByeBye(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion);
    void SsdpNotifyServiceTypeByeBye(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion);
private: // IResumeObserver
    void NotifyResumed();
private:
    void RefreshTimerComplete();
    void NextRefreshDue();
    void CurrentNetworkAdapterChanged();
    void SubnetListChanged();
    void HandleInterfaceChange(TBool aNewSubnet);
    void RemoveAll();
protected:
    SsdpListenerUnicast* iUnicastListener;
    Mutex iSsdpLock;
private:
    static const TUint kMaxMsearchRetryForNewAdapterSecs = 60;
    TIpAddress iInterface;
    SsdpListenerMulticast* iMulticastListener;
    TInt iNotifyHandlerId;
    TUint iInterfaceChangeListenerId;
    TUint iSubnetListChangeListenerId;
    TBool iStarted;
    Timer* iRefreshTimer;
    Timer* iNextRefreshTimer;
    TUint iPendingRefreshCount;
};

/**
 * List of all UPnP devices for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnpAll : public CpiDeviceListUpnp
{
public:
    CpiDeviceListUpnpAll(CpStack& aCpStack, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnpAll();
    void Start();
    void SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge);
};

/**
 * List of all root UPnP devices for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnpRoot : public CpiDeviceListUpnp
{
public:
    CpiDeviceListUpnpRoot(CpStack& aCpStack, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnpRoot();
    void Start();
    void SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge);
};

/**
 * List of all UPnP devices with a given uuid (udn) for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnpUuid : public CpiDeviceListUpnp
{
public:
    CpiDeviceListUpnpUuid(CpStack& aCpStack, const Brx& aUuid, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnpUuid();
    void Start();
    void SsdpNotifyUuidAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge);
private:
    Brh iUuid;
};

/**
 * List of all UPnP devices of a given device type for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnpDeviceType : public CpiDeviceListUpnp
{
public:
    CpiDeviceListUpnpDeviceType(CpStack& aCpStack, const Brx& aDomainName, const Brx& aDeviceType,
                                TUint aVersion, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnpDeviceType();
    void Start();
    void SsdpNotifyDeviceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion,
                                   const Brx& aLocation, TUint aMaxAge);
private:
    Brh iDomainName;
    Brh iDeviceType;
    TUint iVersion;
};

/**
 * List of all UPnP devices of a given service type for a single network interface.
 *
 * This class is not intended for use outside this module.
 */
class CpiDeviceListUpnpServiceType : public CpiDeviceListUpnp
{
public:
    CpiDeviceListUpnpServiceType(CpStack& aCpStack, const Brx& aDomainName, const Brx& aServiceType,
                                 TUint aVersion, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved);
    ~CpiDeviceListUpnpServiceType();
    void Start();
    void SsdpNotifyServiceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType, TUint aVersion,
                                    const Brx& aLocation, TUint aMaxAge);
private:
    Brh iDomainName;
    Brh iServiceType;
    TUint iVersion;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_CPIDEVICEUPNP
