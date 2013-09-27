#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/OsWrapper.h>
#include <exception>
#include <OpenHome/Net/Private/Globals.h> // FIXME - use of globals should be discouraged
#include <OpenHome/Private/Env.h>

using namespace OpenHome;

static const Brn kThreadNameUnknown("____");

// Semaphore

Semaphore::Semaphore(const TChar* aName, TUint aCount)
{
    iHandle = OpenHome::Os::SemaphoreCreate(OpenHome::gEnv->OsCtx(), aName, aCount);
    if (iHandle == kHandleNull) {
        throw std::bad_alloc();
    }
}

Semaphore::~Semaphore()
{
    OpenHome::Os::SemaphoreDestroy(iHandle);
}

void Semaphore::Wait()
{
    OpenHome::Os::SemaphoreWait(iHandle);
}

void Semaphore::Wait(TUint aTimeoutMs)
{
    if (aTimeoutMs == 0) {
        return (Wait());
    }
    ASSERT(iHandle != kHandleNull);
    if (!OpenHome::Os::SemaphoreTimedWait(iHandle, aTimeoutMs)) {
        THROW(Timeout);
    }
}

TBool Semaphore::Clear()
{
    ASSERT(iHandle != kHandleNull);
    return OpenHome::Os::SemaphoreClear(iHandle);
}

void Semaphore::Signal()
{
    OpenHome::Os::SemaphoreSignal(iHandle);
}


// Mutex

Mutex::Mutex(const TChar* aName)
{
    iHandle = OpenHome::Os::MutexCreate(OpenHome::gEnv->OsCtx(), aName);
    if (iHandle == kHandleNull) {
        throw std::bad_alloc();
    }
    (void)strncpy(iName, aName, 4);
    iName[4] = 0;
}

Mutex::~Mutex()
{
    OpenHome::Os::MutexDestroy(iHandle);
}

void Mutex::Wait()
{
    TInt err = OpenHome::Os::MutexLock(iHandle);
    if (err != 0) {
        const char* msg;
        if (err == -1) {
            msg = "Recursive lock attempted on mutex";
        }
        else {
            msg = "Lock attempted on uninitialised mutex";
        }    
        Brhz thBuf;
        Bws<5> thName(Thread::CurrentThreadName());
        thName.PtrZ();
        Log::Print("ERROR: %s %s from thread %s\n", msg, iName, thName.Ptr());
        ASSERT(err == 0);
    }
}

void Mutex::Signal()
{
    OpenHome::Os::MutexUnlock(iHandle);
}


// Thread

const TUint OpenHome::Thread::kDefaultStackBytes = 32 * 1024;

Thread::Thread(const TChar* aName, TUint aPriority, TUint aStackBytes)
    : iHandle(kHandleNull)
    , iSema("TSEM", 0)
    , iTerminated(aName, 0)
    , iKill(false)
    , iStackBytes(aStackBytes)
    , iPriority(aPriority)
    , iKillMutex("KMTX")
{
    ASSERT(aName != NULL);
    iName.SetBytes(iName.MaxBytes());
    iName.Fill(0);
    TUint bytes = (TUint)strlen(aName);
    if (bytes > kNameBytes) {
        bytes = kNameBytes;
    }
    iName.Replace((TByte*)aName, bytes);
}

Thread::~Thread()
{
    LOG(kThread, "> Thread::~Thread() called for thread: %p\n", this);
    Kill();
    Join();
    OpenHome::Os::ThreadDestroy(iHandle);
    LOG(kThread, "< Thread::~Thread() called for thread: %p\n", this);
}

void Thread::Start()
{
    ASSERT(iHandle == kHandleNull);
    iHandle = OpenHome::Os::ThreadCreate(OpenHome::gEnv->OsCtx(), (TChar*)iName.Ptr(), iPriority, iStackBytes, &Thread::EntryPoint, this);
}

void Thread::EntryPoint(void* aArg)
{ // static
    Thread* self = (Thread*)aArg;
    try {
        self->Run();
    }
    catch(ThreadKill&) {
        LOG(kThread, "Thread::Entry() caught ThreadKill for %s(%p)\n", self->iName.Ptr(), self);
    }
    catch(Exception& e) {
        UnhandledExceptionHandler(e);
    }
    catch(std::exception& e) {
        UnhandledExceptionHandler(e);
    }
    catch(...) {
        UnhandledExceptionHandler( "Unknown Exception", "Unknown File", 0 );
    }
    LOG(kThread, "Thread::Entry(): Thread::Run returned, exiting thread %s(%p)\n", self->iName.Ptr(), self);
    self->iTerminated.Signal();
}

void Thread::Wait()
{
    iSema.Wait();
    CheckForKill();
}

void Thread::Signal()
{
    iSema.Signal();
}

TBool Thread::TryWait()
{
    CheckForKill();
    return (iSema.Clear());
}

void Thread::Sleep(TUint aMilliSecs)
{ // static
    if (aMilliSecs == 0) {
        // a 0ms sleep will block forever so round up to the smallest delay we can manage (1ms)
        aMilliSecs = 1;
    }
    Semaphore sem("", 0);
    try {
        sem.Wait(aMilliSecs);
    }
    catch(Timeout&) {}
}

const Brx& Thread::CurrentThreadName()
{ // static
    Thread* th = Current();
    if (th == NULL) {
        return kThreadNameUnknown;
    }
    return th->iName;
}

Thread* Thread::Current()
{ // static
    void* th = OpenHome::Os::ThreadTls(OpenHome::gEnv->OsCtx());
    if (th == NULL) {
        return NULL;
    }
    return (Thread*)th;
}

TBool Thread::SupportsPriorities()
{ // static
    return OpenHome::Os::ThreadSupportsPriorities(OpenHome::gEnv->OsCtx());
}

void Thread::CheckCurrentForKill()
{ // static
    Thread* thread = Thread::Current();
    if ( thread != NULL )
        thread->CheckForKill();
}

void Thread::CheckForKill() const
{
    AutoMutex _amtx(iKillMutex);
    TBool kill = iKill;
    if (kill) {
        THROW(ThreadKill);
    }
}

void Thread::Kill()
{
    LOG(kThread, "Thread::Kill() called for thread: %p\n", this);
    AutoMutex _amtx(iKillMutex);
    iKill = true;
    Signal();
}

const Brx& Thread::Name() const
{
    return iName;
}

bool Thread::operator== (const Thread& other) const {
    return (Thread*) this == (Thread*) &other;
}

bool Thread::operator!= (const Thread& other) const {
    return !(*this == other);
}

void Thread::Join()
{
    LOG(kThread, "Thread::Join() called for thread: %p\n", this);
    
    iTerminated.Wait();
    iTerminated.Signal();
}


// ThreadFunctor

ThreadFunctor::ThreadFunctor(const TChar* aName, Functor aFunctor, TUint aPriority, TUint aStackBytes)
    : Thread(aName, aPriority, aStackBytes)
    , iFunctor(aFunctor)
    , iStarted("TFSM", 0)
{
}

ThreadFunctor::~ThreadFunctor()
{
    iStarted.Wait();
}

void ThreadFunctor::Run()
{
    iStarted.Signal();
    iFunctor();
}


// AutoMutex

AutoMutex::AutoMutex(Mutex& aMutex)
    : iMutex(aMutex)
{
    iMutex.Wait();
}

AutoMutex::~AutoMutex()
{
    iMutex.Signal();
}


// AutoSemaphore

AutoSemaphore::AutoSemaphore(Semaphore& aSemaphore)
    : iSem(aSemaphore)
{
    iSem.Wait();
}

AutoSemaphore::~AutoSemaphore()
{
    iSem.Signal();
}
