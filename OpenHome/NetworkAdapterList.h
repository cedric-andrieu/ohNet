#ifndef HEADER_NETWORKINTERFACELIST
#define HEADER_NETWORKINTERFACELIST

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Functor.h>
#include <OpenHome/FunctorNetworkAdapter.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Core/OhNet.h>

#include <vector>
#include <map>
#include <list>

namespace OpenHome {

class INetworkAdapterChangeNotifier
{
public:
    virtual void NotifyCurrentChanged() = 0;
    virtual void NotifySubnetsChanged() = 0;
    virtual void NotifyAdapterAdded(NetworkAdapter& aAdapter) = 0;
    virtual void NotifyAdapterRemoved(NetworkAdapter& aAdapter) = 0;
    virtual void NotifyAdapterChanged(NetworkAdapter& aAdapter) = 0;
};

class NetworkAdapterChangeNotifier;

class NetworkAdapterList : private IStackObject, private INetworkAdapterChangeNotifier
{
public:
    static const TUint kListenerIdNull = 0;
public:
    NetworkAdapterList(Environment& aEnv, TIpAddress aDefaultSubnet=0);
    virtual ~NetworkAdapterList();
    void Refresh(); // only needs to be called if app can be suspended (e.g. iOS) or OS notification of adapter change isn't possible
    NetworkAdapter* CurrentAdapter(const char* aCookie) const;
    const std::vector<NetworkAdapter*>& List() const;
    std::vector<NetworkAdapter*>* CreateSubnetList() const;
    static void DestroySubnetList(std::vector<NetworkAdapter*>* aList);
    std::vector<NetworkAdapter*>* CreateNetworkAdapterList() const;
    static void DestroyNetworkAdapterList(std::vector<NetworkAdapter*>* aList);
    void SetCurrentSubnet(TIpAddress aSubnet);
    TUint AddCurrentChangeListener(Functor aFunctor, TBool aInternalClient = true); // internal clients are notified first
    void RemoveCurrentChangeListener(TUint aId);
    TUint AddSubnetListChangeListener(Functor aFunctor, TBool aInternalClient = true); // internal clients are notified first
    void RemoveSubnetListChangeListener(TUint aId);
    TUint AddSubnetAddedListener(FunctorNetworkAdapter aFunctor); // only for use by client code
    void RemoveSubnetAddedListener(TUint aId);
    TUint AddSubnetRemovedListener(FunctorNetworkAdapter aFunctor); // only for use by client code
    void RemoveSubnetRemovedListener(TUint aId);
    TUint AddNetworkAdapterChangeListener(FunctorNetworkAdapter aFunctor); // only for use by client code
    void RemoveNetworkAdapterChangeListener(TUint aId);
private:
    typedef std::map<TUint,Functor> Map;
    typedef std::map<TUint,FunctorNetworkAdapter> MapNetworkAdapter;
    std::vector<NetworkAdapter*>* CreateSubnetListLocked() const;
    TUint AddListener(Functor aFunctor, Map& aMap);
    TBool RemoveSubnetListChangeListener(TUint aId, Map& aMap);
    TUint AddSubnetListener(FunctorNetworkAdapter aFunctor, MapNetworkAdapter& aMap);
    void RemoveSubnetListener(TUint aId, MapNetworkAdapter& aMap);
    static void InterfaceListChanged(void* aPtr);
    static TInt FindSubnet(TIpAddress aSubnet, const std::vector<NetworkAdapter*>& aList);
    void UpdateCurrentAdapter();
    void HandleInterfaceListChanged();
    void RunCallbacks(Map& aMap);
    void DoRunCallbacks(Map& aMap);
    void RunSubnetCallbacks(MapNetworkAdapter& aMap, NetworkAdapter& aAdapter);
    static TBool CompareSubnets(NetworkAdapter* aI, NetworkAdapter* aJ);
    static void TraceAdapter(const TChar* aPrefix, NetworkAdapter& aAdapter);
    /**
     * Some platforms behave poorly for a short period after delivering a subnet change message.
     * e.g. Windows reports adapters as available/usable but attempts to bind to them may fail.
     * This function helps work around that by calling a client-specified function, catching and
     * NetworkError failure then re-trying shortly later.  It eventually gives up if errors continue.
     *
     * Frequency and overall duration of retries could be user-configurable but are currently hard-coded.
     */
//     static void TempFailureRetry(Functor& aCallback);
private: // from IStackObject
    void ListObjectDetails() const;
private: // from INetworkAdapterChangeNotifier
    void NotifyCurrentChanged();
    void NotifySubnetsChanged();
    void NotifyAdapterAdded(NetworkAdapter& aAdapter);
    void NotifyAdapterRemoved(NetworkAdapter& aAdapter);
    void NotifyAdapterChanged(NetworkAdapter& aAdapter);
private:
    Environment& iEnv;
    mutable Mutex iListLock;
    Mutex iListenerLock;
    std::vector<NetworkAdapter*>* iNetworkAdapters;
    std::vector<NetworkAdapter*>* iSubnets;
    mutable NetworkAdapter* iCurrent;
    TIpAddress iDefaultSubnet;
    Map iListenersCurrentInternal;
    Map iListenersCurrentExternal;
    Map iListenersSubnetInternal;
    Map iListenersSubnetExternal;
    MapNetworkAdapter iListenersAdded;
    MapNetworkAdapter iListenersRemoved;
    MapNetworkAdapter iListenersAdapterChanged;
    TUint iNextListenerId;
    NetworkAdapterChangeNotifier* iNotifierThread;
};

class NetworkAdapterChangeNotifier : public Thread
{
public:
    NetworkAdapterChangeNotifier(INetworkAdapterChangeNotifier& aAdapterList);
    ~NetworkAdapterChangeNotifier();
    void QueueCurrentChanged();
    void QueueSubnetsChanged();
    void QueueAdapterAdded(NetworkAdapter& aAdapter);
    void QueueAdapterRemoved(NetworkAdapter& aAdapter);
    void QueueAdapterChanged(NetworkAdapter& aAdapter);
private: // from Thread
    void Run();
private:
    class UpdateBase : private INonCopyable
    {
    public:
        virtual ~UpdateBase();
        virtual void Update(INetworkAdapterChangeNotifier& aAdapterList) = 0;
    protected:
        UpdateBase();
    };
    class UpdateCurrent : public UpdateBase
    {
    private: // from UpdateBase
        void Update(INetworkAdapterChangeNotifier& aAdapterList);
    };
    class UpdateSubnet : public UpdateBase
    {
    private: // from UpdateBase
        void Update(INetworkAdapterChangeNotifier& aAdapterList);
    };
    class UpdateAdapter : public UpdateBase
    {
    protected:
        UpdateAdapter(NetworkAdapter& aAdapter);
        ~UpdateAdapter();
    protected:
        NetworkAdapter& iAdapter;
    };
    class UpdateAdapterAdded : public UpdateAdapter
    {
    public:
        UpdateAdapterAdded(NetworkAdapter& aAdapter);
    private: // from UpdateBase
        void Update(INetworkAdapterChangeNotifier& aAdapterList);
    };
    class UpdateAdapterRemoved : public UpdateAdapter
    {
    public:
        UpdateAdapterRemoved(NetworkAdapter& aAdapter);
    private: // from UpdateBase
        void Update(INetworkAdapterChangeNotifier& aAdapterList);
    };
    class UpdateAdapterChanged : public UpdateAdapter
    {
    public:
        UpdateAdapterChanged(NetworkAdapter& aAdapter);
    private: // from UpdateBase
        void Update(INetworkAdapterChangeNotifier& aAdapterList);
    };
private:
    void Queue(UpdateBase* aUpdate);
private:
    INetworkAdapterChangeNotifier& iAdapterList;
    OpenHome::Mutex iLock;
    std::list<UpdateBase*> iList;
};

} // namespace OpenHome

#endif // HEADER_NETWORKINTERFACELIST
