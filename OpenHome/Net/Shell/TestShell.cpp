#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Private/Shell.h>
#include <OpenHome/Net/Private/ShellCommandRun.h>
#include <OpenHome/Net/Private/ShellCommandDebug.h>
#include <OpenHome/Net/Private/ShellCommandQuit.h>
#include <OpenHome/Net/Private/ShellCommandWatchDog.h>
#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Net/Private/CpiStack.h>

using namespace OpenHome;
using namespace OpenHome::Net;


extern void TestBuffer();
static void RunTestBuffer(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestBuffer(); }

extern void TestThread();
static void RunTestThread(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestThread(); }

extern void TestFifo();
static void RunTestFifo(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestFifo(); }

extern void TestQueue();
static void RunTestQueue(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestQueue(); }

extern void TestTextUtils();
static void RunTestTextUtils(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestTextUtils(); }

extern void TestNetwork(const std::vector<Brn>& aArgs);
static void RunTestNetwork(CpStack& /*aCpStack*/, DvStack& /*aDvStack*/, const std::vector<Brn>& aArgs) { TestNetwork(aArgs); }

extern void TestTimer(Environment& aEnv);
static void RunTestTimer(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestTimer(aCpStack.Env()); }

extern void TestSsdpMListen(Environment& aEnv, const std::vector<Brn>& aArgs);
static void RunTestSsdpMListen(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& aArgs) { TestSsdpMListen(aCpStack.Env(), aArgs); }

extern void TestSsdpUListen(Environment& aEnv, const std::vector<Brn>& aArgs);
static void RunTestSsdpUListen(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& aArgs) { TestSsdpUListen(aCpStack.Env(), aArgs); }

extern void TestDeviceList(CpStack& aCpStack, const std::vector<Brn>& aArgs);
static void RunTestDeviceList(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& aArgs) { TestDeviceList(aCpStack, aArgs); }

extern void TestInvocation(CpStack& aCpStack);
static void RunTestInvocation(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestInvocation(aCpStack); }

extern void TestSubscription(CpStack& aCpStack);
static void RunTestSubscription(CpStack& aCpStack, DvStack& /*aDvStack*/, const std::vector<Brn>& /*aArgs*/) { TestSubscription(aCpStack); }

extern void TestCpDeviceDv(CpStack& aCpStack, DvStack& aDvStack);
static void RunTestCpDeviceDv(CpStack& aCpStack, DvStack& aDvStack, const std::vector<Brn>& /*aArgs*/) { TestCpDeviceDv(aCpStack, aDvStack); }

extern void TestDviDiscovery(DvStack& aDvStack);
static void RunTestDviDiscovery(CpStack& /*aCpStack*/, DvStack& aDvStack, const std::vector<Brn>& /*aArgs*/) { TestDviDiscovery(aDvStack); }

extern void TestDviDeviceList(CpStack& aCpStack, DvStack& aDvStack);
static void RunTestDviDeviceList(CpStack& aCpStack, DvStack& aDvStack, const std::vector<Brn>& /*aArgs*/) { TestDviDeviceList(aCpStack, aDvStack); }

extern void TestDvInvocation(CpStack& aCpStack, DvStack& aDvStack);
static void RunTestDvInvocation(CpStack& aCpStack, DvStack& aDvStack, const std::vector<Brn>& /*aArgs*/) { TestDvInvocation(aCpStack, aDvStack); }

extern void TestDvSubscription(CpStack& aCpStack, DvStack& aDvStack);
static void RunTestDvSubscription(CpStack& aCpStack, DvStack& aDvStack, const std::vector<Brn>& /*aArgs*/) { TestDvSubscription(aCpStack, aDvStack); }

void OpenHome::TestFramework::Runner::Main(TInt /*aArgc*/, TChar* /*aArgv*/[], Net::InitialisationParams* aInitParams)
{
    Library* lib = new Library(aInitParams);
    std::vector<NetworkAdapter*>* subnetList = lib->CreateSubnetList();
    TIpAddress subnet = (*subnetList)[0]->Subnet();
    TIpAddress addr = (*subnetList)[0]->Address();
    Endpoint endpt(0, addr);
    Endpoint::AddressBuf buf;
    endpt.AppendAddress(buf);
    Print("Connect to %s:%u then 'help' for options\n\n", buf.Ptr(), Shell::kServerPort);
    Library::DestroySubnetList(subnetList);
    CpStack* cpStack = NULL;
    DvStack* dvStack = NULL;
    lib->StartCombined(subnet, cpStack, dvStack);

    Shell* shell = new Shell(cpStack->Env());
    Semaphore* blocker = new Semaphore("BLCK", 0);

    std::vector<ShellTest> shellTests;
    shellTests.push_back(ShellTest("TestBuffer", RunTestBuffer));
    shellTests.push_back(ShellTest("TestBuffer", RunTestBuffer));
    shellTests.push_back(ShellTest("TestThread", RunTestThread));
    shellTests.push_back(ShellTest("TestFifo", RunTestFifo));
    shellTests.push_back(ShellTest("TestQueue", RunTestQueue));
    shellTests.push_back(ShellTest("TestTextUtils", RunTestTextUtils));
    shellTests.push_back(ShellTest("TestNetwork", RunTestNetwork, true));
    shellTests.push_back(ShellTest("TestTimer", RunTestTimer));
    shellTests.push_back(ShellTest("TestSsdpMListen", RunTestSsdpMListen, true));
    shellTests.push_back(ShellTest("TestSsdpUListen", RunTestSsdpUListen, true));
    shellTests.push_back(ShellTest("TestDeviceList", RunTestDeviceList, true));
    shellTests.push_back(ShellTest("TestInvocation", RunTestInvocation));
    shellTests.push_back(ShellTest("TestSubscription", RunTestSubscription));
    shellTests.push_back(ShellTest("TestCpDeviceDv", RunTestCpDeviceDv));
    shellTests.push_back(ShellTest("TestDviDiscovery", RunTestDviDiscovery));
    shellTests.push_back(ShellTest("TestDviDeviceList", RunTestDviDeviceList));
    shellTests.push_back(ShellTest("TestDvInvocation", RunTestDvInvocation));
    shellTests.push_back(ShellTest("TestDvSubscription", RunTestDvSubscription));

    ShellCommandRun* cmdRun = new ShellCommandRun(*cpStack, *dvStack, *shell, shellTests);
    ShellCommandDebug* cmdDebug = new ShellCommandDebug(*shell);
    ShellCommandQuit* cmdQuit = new ShellCommandQuit(*shell, *blocker);
    ShellCommandWatchDog* cmdWatchDog = new ShellCommandWatchDog(*shell, 60); // default to 60s watchdog. This can be changed at runtime by client.
    blocker->Wait();
    // control never reaches here
    delete blocker;
    delete cmdWatchDog;
    delete cmdQuit;
    delete cmdDebug;
    delete cmdRun;
    delete shell;
    delete lib;
}
