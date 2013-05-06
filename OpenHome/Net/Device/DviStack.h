/**
 * UPnP library internal APIs
 *
 * NOT intended for use by clients of the library
 */

#ifndef HEADER_DVI_STACK
#define HEADER_DVI_STACK

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Env.h>
#include <OpenHome/Net/Private/DviServerUpnp.h>
#include <OpenHome/Net/Private/DviDevice.h>
#include <OpenHome/Net/Private/DviSubscription.h>
#include <OpenHome/Net/Private/DviServerWebSocket.h>
#include <OpenHome/Net/Private/DviService.h>
#include <OpenHome/Net/Private/Bonjour.h>
#include <OpenHome/Net/Private/DviPropertyUpdateCollection.h>
#include <OpenHome/Net/Private/DviSsdpNotifier.h>
#include <OpenHome/Private/Standard.h>

#include <vector>

namespace OpenHome {
namespace Net {

class DvStack : private IStack, private INonCopyable
{
public:
    DvStack(Environment& aEnv);
    Environment& Env() { return iEnv; }
    TUint BootId();
    TUint NextBootId();
    void UpdateBootId();
    DviServerUpnp& ServerUpnp();
    DviDeviceMap& DeviceMap();
    DviSubscriptionManager& SubscriptionManager();
    IMdnsProvider* MdnsProvider();
    DviPropertyUpdateCollection& PropertyUpdateCollection();
    DviSsdpNotifierManager& SsdpNotifierManager();
private:
    ~DvStack();
private:
    OpenHome::Environment& iEnv;
    TUint iBootId;
    TUint iNextBootId;
    DviServerUpnp* iDviServerUpnp;
    DviDeviceMap* iDviDeviceMap;
    DviSubscriptionManager* iSubscriptionManager;
    DviServerWebSocket* iDviServerWebSocket;
    IMdnsProvider* iMdns;
    DviPropertyUpdateCollection* iPropertyUpdateCollection;
    DviSsdpNotifierManager* iSsdpNotifierManager;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_DVI_STACK
