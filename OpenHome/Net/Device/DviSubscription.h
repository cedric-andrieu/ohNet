#ifndef HEADER_DVISUBSCRIPTION
#define HEADER_DVISUBSCRIPTION

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Exception.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Net/Private/Service.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Net/Core/OhNet.h>

#include <vector>
#include <map>
#include <list>

EXCEPTION(DvSubscriptionError);

namespace OpenHome {
namespace Net {

class IDviSubscriptionUserData
{
public:
    virtual ~IDviSubscriptionUserData() {}
    virtual const void* Data() const = 0;
    virtual void Release() = 0;
};
    
class IPropertyWriterFactory
{
public:
    virtual ~IPropertyWriterFactory() {}
    virtual IPropertyWriter* CreateWriter(const IDviSubscriptionUserData* aUserData, 
                                          const Brx& aSid, TUint aSequenceNumber) = 0;
    virtual void NotifySubscriptionCreated(const Brx& aSid) = 0;
    virtual void NotifySubscriptionDeleted(const Brx& aSid) = 0;
    virtual void NotifySubscriptionExpired(const Brx& aSid) = 0;
};

class DviDevice;
class DviService;
class DvStack;

class DviSubscription : private IStackObject
{
public:
    DviSubscription(DvStack& aDvStack, DviDevice& aDevice, IPropertyWriterFactory& aWriterFactory,
                    IDviSubscriptionUserData* aUserData, Brh& aSid, TUint& aDurationSecs);
    void Start(DviService& aService);
    void Stop(); // should only be called by DviService
    void AddRef();
    TBool TryAddRef();
    void RemoveRef();
    void Remove();
    void Renew(TUint& aSeconds);
    void WriteChanges();
    const Brx& Sid() const;
private: // from IStackObject
    void ListObjectDetails() const;
private:
    virtual ~DviSubscription();
    IPropertyWriter* CreateWriter();
    void Expired();
    void DoRenew(TUint& aSeconds);
private:
    DvStack& iDvStack;
    mutable Mutex iLock;
    TUint iRefCount;
    DviDevice& iDevice;
    IPropertyWriterFactory& iWriterFactory;
    IDviSubscriptionUserData* iUserData;
    Brh iSid;
    DviService* iService;
    std::vector<TUint> iPropertySequenceNumbers;
    TUint iSequenceNumber;
    Timer* iTimer;
};

class PropertyWriter : public IPropertyWriter
{
public:
    static void WriteVariable(IWriter& aWriter, const Brx& aName, const Brx& aValue);
protected:
    PropertyWriter();
    void SetWriter(IWriter& aWriter);
private: // IPropertyWriter
    void PropertyWriteString(const Brx& aName, const Brx& aValue);
    void PropertyWriteInt(const Brx& aName, TInt aValue);
    void PropertyWriteUint(const Brx& aName, TUint aValue);
    void PropertyWriteBool(const Brx& aName, TBool aValue);
    void PropertyWriteBinary(const Brx& aName, const Brx& aValue);
private:
    void WriteVariable(const Brx& aName, const Brx& aValue);
private:
    IWriter* iWriter;
};

class Publisher : public Thread
{
public:
    Publisher(const TChar* aName, Fifo<Publisher*>& aFree);
    ~Publisher();
    void Publish(DviSubscription* aSubscription);
private:
    void Error(const TChar* aErr);
    void Run();
private:
    Fifo<Publisher*>& iFree;
    DviSubscription* iSubscription;
};

class DviSubscriptionManager : public Thread
{
public:
    DviSubscriptionManager(DvStack& aDvStack);
    ~DviSubscriptionManager();
    void AddSubscription(DviSubscription& aSubscription);
    void RemoveSubscription(DviSubscription& aSubscription);
    DviSubscription* Find(const Brx& aSid);
    void QueueUpdate(DviSubscription& aSubscription);
private:
    void Run();
private:
    DvStack& iDvStack;
    Mutex iLock;
    std::list<DviSubscription*> iList;
    Fifo<Publisher*> iFree;
    Publisher** iPublishers;
    typedef std::map<Brn,DviSubscription*,BufferCmp> Map;
    Map iMap;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_DVISUBSCRIPTION
