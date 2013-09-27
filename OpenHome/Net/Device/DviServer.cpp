#include <OpenHome/Net/Private/DviServer.h>
#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Env.h>
#include <OpenHome/Net/Private/DviStack.h>
#include <OpenHome/Private/NetworkAdapterList.h>
#include <OpenHome/Net/Core/OhNet.h>

using namespace OpenHome;
using namespace OpenHome::Net;

// DviServer

DviServer::~DviServer()
{
    iLock.Wait();
    iDvStack.Env().NetworkAdapterList().RemoveCurrentChangeListener(iCurrentAdapterChangeListenerId);
    iDvStack.Env().NetworkAdapterList().RemoveSubnetListChangeListener(iSubnetListChangeListenerId);
    for (TUint i=0; i<iServers.size(); i++) {
        delete iServers[i];
    }
    iServers.clear();
    iLock.Signal();
}

TUint DviServer::Port(TIpAddress aInterface)
{
    AutoMutex a(iLock);
    for (TUint i=0; i<iServers.size(); i++) {
        DviServer::Server* server = iServers[i];
        if (server->Interface() == aInterface) {
            return server->Port();
        }
    }
    return 0;
}

DviServer::DviServer(DvStack& aDvStack)
    : iDvStack(aDvStack)
    , iLock("DSUM")
    , iSubnetListChangeListenerId(NetworkAdapterList::kListenerIdNull)
{
}

void DviServer::Initialise()
{
    Functor functor = MakeFunctor(*this, &DviServer::SubnetListChanged);
    NetworkAdapterList& nifList = iDvStack.Env().NetworkAdapterList();
    iCurrentAdapterChangeListenerId = nifList.AddCurrentChangeListener(functor);
    iSubnetListChangeListenerId = nifList.AddSubnetListChangeListener(functor);
    AutoMutex a(iLock);
    std::vector<NetworkAdapter*>* subnetList = nifList.CreateSubnetList();
    for (TUint i=0; i<subnetList->size(); i++) {
        AddServer(*(*subnetList)[i]);
    }
    NetworkAdapterList::DestroySubnetList(subnetList);
}

void DviServer::AddServer(NetworkAdapter& aNif)
{
    SocketTcpServer* tcpServer = CreateServer(aNif);
    DviServer::Server* server = new DviServer::Server(tcpServer, aNif);
    iServers.push_back(server);
}

void DviServer::SubnetListChanged()
{
    /* DviProtocolUpnp relies on servers being available on all appropriate interfaces.
       We assume this happens through DviServer being created before any devices
       so registering for subnet change notification earlier.  Assuming NetworkAdapterList
       always runs its listeners in the order they registered, we'll have updated before
       any device listeners are run. */

    AutoMutex a(iLock);
    NetworkAdapterList& adapterList = iDvStack.Env().NetworkAdapterList();
    AutoNetworkAdapterRef ref(iDvStack.Env(), "DviServer::SubnetListChanged");
    NetworkAdapter* current = ref.Adapter();
    if (current != NULL) {
        TInt i;
        // remove servers whose interface is no longer available
        for (i = (TInt)iServers.size() - 1; i >= 0; i--) {
            DviServer::Server* server = iServers[i];
            if (server->Interface() != current->Address()) {
                delete server;
                iServers.erase(iServers.begin() + i);
            }
        }
        // add server if 'current' is a new subnet
        if (iServers.size() == 0) {
            AddServer(*current);
        }
    }
    else {
        std::vector<NetworkAdapter*>* subnetList = adapterList.CreateSubnetList();
        const std::vector<NetworkAdapter*>& nifList = adapterList.List();
        TInt i;
        // remove servers whose interface is no longer available
        for (i = (TInt)iServers.size() - 1; i >= 0; i--) {
            DviServer::Server* server = iServers[i];
            if (FindInterface(server->Interface(), nifList) == -1) {
                delete server;
                iServers.erase(iServers.begin() + i);
            }
        }
        // add servers for new subnets
        for (i = 0; i < (TInt)subnetList->size(); i++) {
            NetworkAdapter* subnet = (*subnetList)[i];
            if (FindServer(subnet->Subnet()) == -1) {
                AddServer(*subnet);
            }
        }
        NetworkAdapterList::DestroySubnetList(subnetList);
    }
}

TInt DviServer::FindInterface(TIpAddress aInterface, const std::vector<NetworkAdapter*>& aNifList)
{
    for (TUint i=0; i<aNifList.size(); i++) {
        if (aNifList[i]->Address() == aInterface) {
            return i;
        }
    }
    return -1;
}

TInt DviServer::FindServer(TIpAddress aSubnet)
{
    for (TUint i=0; i<iServers.size(); i++) {
        if (iServers[i]->Subnet() == aSubnet) {
            return i;
        }
    }
    return -1;
}


//  DviServer::Server

DviServer::Server::Server(SocketTcpServer* aTcpServer, NetworkAdapter& aNif)
    : iNif(aNif)
{
    iServer = aTcpServer;
    iNif.AddRef("DviServer::Server");
}

DviServer::Server::~Server()
{
    delete iServer;
    iNif.RemoveRef("DviServer::Server");
}
