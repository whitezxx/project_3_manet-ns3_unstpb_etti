/*
 * manet-compare.cc  --  Project 3: AODV vs OLSR vs DSDV in ns-3 (ns-3.48)
 *
 * Place in:  <ns-3-dir>/scratch/manet-compare.cc
 * Run:       ./ns3 run "manet-compare --protocol=aodv --nodes=50 --speed=0 --run=1 --outDir=results"
 *
 * Outputs (in --outDir, which must exist):
 *   <tag>_tx.csv        one line per data packet sent      (flow, seq, t_tx, bytes)
 *   <tag>_rx.csv        one line per data packet received  (flow, seq, t_tx, t_rx, delay_ms, bytes)
 *   <tag>_ctrl.csv      one line per routing control packet transmitted (t, node, port, bytes)
 *   <tag>_flows.csv     the random source/sink pairs of this run
 *   <tag>_flowmon.xml   raw FlowMonitor output
 *   <tag>_flowmon.csv   FlowMonitor per-flow stats (data flows only)
 *   <tag>_summary.csv   one line with totals and the wall-clock time of the run
 *
 * tag = <protocol>_n<nodes>_s<speed>_r<run>
 */

#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetCompare");

namespace
{
// UDP ports used by the ns-3 routing protocol implementations
const uint16_t AODV_PORT = 654;
const uint16_t OLSR_PORT = 698;
const uint16_t DSDV_PORT = 269;
// data flow i uses port DATA_PORT_BASE + i
const uint16_t DATA_PORT_BASE = 9000;

std::ofstream g_txLog;
std::ofstream g_rxLog;
std::ofstream g_ctrlLog;

uint64_t g_appTx = 0;
uint64_t g_appRx = 0;
uint64_t g_ctrlPkts = 0;
uint64_t g_ctrlBytes = 0;
uint64_t g_srcDrops = 0; // packets the source could not send (no route at the source)

// Constant-bit-rate sender. One packet every `interval`, whatever the routing state.
// Why not OnOffApplication: when Send() fails (no route yet, typical for OLSR/DSDV at the
// source) OnOff caches the packet and stops generating new ones, so the offered load would
// differ between protocols. Here every scheduled packet is counted as offered; if the
// source has no route it is simply lost (and counted in g_srcDrops).
void
CbrSend(uint32_t flowId, Ptr<Socket> sock, uint32_t seq, uint32_t size, Time interval, Time stop)
{
    SeqTsSizeHeader h; // timestamp = Simulator::Now(), set by the constructor
    h.SetSeq(seq);
    h.SetSize(size);
    Ptr<Packet> p = Create<Packet>(size - h.GetSerializedSize());
    p->AddHeader(h); // total size on the UDP layer = size bytes
    g_appTx++;
    g_txLog << flowId << ',' << seq << ',' << Simulator::Now().GetSeconds() << ',' << size
            << '\n';
    if (sock->Send(p) < 0)
    {
        g_srcDrops++;
    }
    if (Simulator::Now() + interval < stop)
    {
        Simulator::Schedule(interval, &CbrSend, flowId, sock, seq + 1, size, interval, stop);
    }
}

// PacketSink received a data packet; the header carries seq and send time
void
AppRx(uint32_t flowId,
      Ptr<const Packet> p,
      const Address& /*from*/,
      const Address& /*to*/,
      const SeqTsSizeHeader& h)
{
    g_appRx++;
    double delayMs = (Simulator::Now() - h.GetTs()).GetMicroSeconds() / 1000.0;
    g_rxLog << flowId << ',' << h.GetSeq() << ',' << h.GetTs().GetSeconds() << ','
            << Simulator::Now().GetSeconds() << ',' << delayMs << ',' << p->GetSize() << '\n';
}

// Every IPv4 packet transmitted by any node (originated or forwarded).
// A packet whose UDP destination port is a routing port is a control packet.
void
IpTx(Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t /*iface*/)
{
    Ptr<Packet> copy = p->Copy();
    Ipv4Header ip;
    copy->RemoveHeader(ip);
    if (ip.GetProtocol() != UdpL4Protocol::PROT_NUMBER)
    {
        return;
    }
    UdpHeader udp;
    copy->RemoveHeader(udp);
    uint16_t port = udp.GetDestinationPort();
    if (port == AODV_PORT || port == OLSR_PORT || port == DSDV_PORT)
    {
        g_ctrlPkts++;
        g_ctrlBytes += p->GetSize();
        g_ctrlLog << Simulator::Now().GetSeconds() << ',' << ipv4->GetObject<Node>()->GetId()
                  << ',' << port << ',' << p->GetSize() << '\n';
    }
}
} // namespace

int
main(int argc, char* argv[])
{
    // ---------------- parameters (defaults = scenario defaults of the guidelines)
    std::string protocol = "aodv";
    uint32_t nNodes = 50;
    double speed = 0.0;       // m/s, 0 = static
    uint32_t run = 1;         // RngRun = seed index of the matrix
    double totalTime = 200.0; // simulated seconds
    uint32_t nFlows = 10;
    uint32_t pktSize = 128;   // bytes
    double pktRate = 2.0;     // packets/s per flow
    double area = 500.0;      // m
    double range = 100.0;     // m
    double pause = 2.0;       // s
    std::string outDir = "results";

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "aodv | olsr | dsdv", protocol);
    cmd.AddValue("nodes", "number of nodes", nNodes);
    cmd.AddValue("speed", "RandomWaypoint speed in m/s (0 = static)", speed);
    cmd.AddValue("run", "RngRun number (seed index)", run);
    cmd.AddValue("time", "simulated time in s", totalTime);
    cmd.AddValue("pause", "RandomWaypoint pause time in s", pause);
    cmd.AddValue("flows", "number of CBR flows", nFlows);
    cmd.AddValue("pktSize", "packet size in bytes", pktSize);
    cmd.AddValue("pktRate", "packets per second per flow", pktRate);
    cmd.AddValue("range", "radio range in m", range);
    cmd.AddValue("outDir", "output directory (must exist)", outDir);
    cmd.Parse(argc, argv);

    if(pause < 0.0)
    {
        NS_FATAL_ERROR("pause time must be >= 0");
    }

    auto wallStart = std::chrono::steady_clock::now();

    RngSeedManager::SetSeed(12345);
    RngSeedManager::SetRun(run);

    std::ostringstream tag;
    tag << protocol << "_n" << nNodes << "_s" << speed << "_r" << run;
    std::string base = outDir + "/" + tag.str();

    g_txLog.open(base + "_tx.csv");
    g_rxLog.open(base + "_rx.csv");
    g_ctrlLog.open(base + "_ctrl.csv");
    if (!g_txLog || !g_rxLog || !g_ctrlLog)
    {
        NS_FATAL_ERROR("cannot write to " << outDir << " (does the directory exist?)");
    }
    g_txLog << "flow,seq,t_tx,bytes\n";
    g_rxLog << "flow,seq,t_tx,t_rx,delay_ms,bytes\n";
    g_ctrlLog << "t,node,port,bytes\n";


    // ---------------- nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // ---------------- Wi-Fi: 802.11g ad-hoc, fixed 6 Mbps, hard range
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("ErpOfdmRate6Mbps"),
                                 "ControlMode", StringValue("ErpOfdmRate6Mbps"));

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    wifi.AssignStreams(devices, 100000);

    // ---------------- mobility
    std::ostringstream xy;
    xy << "ns3::UniformRandomVariable[Min=0.0|Max=" << area << "]";
    ObjectFactory posFactory;
    posFactory.SetTypeId("ns3::RandomRectanglePositionAllocator");
    posFactory.Set("X", StringValue(xy.str()));
    posFactory.Set("Y", StringValue(xy.str()));
    Ptr<PositionAllocator> posAlloc = posFactory.Create()->GetObject<PositionAllocator>();
    posAlloc->AssignStreams(200000);

    MobilityHelper mobility;
    mobility.SetPositionAllocator(posAlloc);
    if (speed > 0.0)
    {
        std::ostringstream sp;
        sp << "ns3::ConstantRandomVariable[Constant=" << speed << "]";
        std::ostringstream pa;
        pa << "ns3::ConstantRandomVariable[Constant=" << pause << "]";
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue(sp.str()),
                                  "Pause", StringValue(pa.str()),
                                  "PositionAllocator", PointerValue(posAlloc));
    }
    else
    {
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    mobility.Install(nodes);
    mobility.AssignStreams(nodes, 300000);

    // ---------------- routing + IP stack
    InternetStackHelper internet;
    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    if (protocol == "aodv")
    {
        internet.SetRoutingHelper(aodv);
    }
    else if (protocol == "olsr")
    {
        internet.SetRoutingHelper(olsr);
    }
    else if (protocol == "dsdv")
    {
        internet.SetRoutingHelper(dsdv);
    }
    else
    {
        NS_FATAL_ERROR("unknown protocol: " << protocol << " (use aodv, olsr or dsdv)");
    }
    internet.Install(nodes);

    Ipv4AddressHelper addr;
    addr.SetBase("10.0.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifaces = addr.Assign(devices);

    // ---------------- traffic: nFlows CBR/UDP flows between random pairs
    Ptr<UniformRandomVariable> pick = CreateObject<UniformRandomVariable>();
    pick->SetStream(400000);
    Ptr<UniformRandomVariable> startRv = CreateObject<UniformRandomVariable>();
    startRv->SetStream(400001);

    Time interval = Seconds(1.0 / pktRate); // 0.5 s for 2 packets/s
    Time stop = Seconds(totalTime - 1.0);   // last second left for packets in flight

    std::ofstream flowsLog(base + "_flows.csv");
    flowsLog << "flow,src,dst,start\n";

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        uint32_t src = pick->GetInteger(0, nNodes - 1);
        uint32_t dst = pick->GetInteger(0, nNodes - 1);
        while (dst == src)
        {
            dst = pick->GetInteger(0, nNodes - 1);
        }
        uint16_t port = DATA_PORT_BASE + i;

        PacketSinkHelper sinkH("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkH.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
        ApplicationContainer s = sinkH.Install(nodes.Get(dst));
        s.Get(0)->TraceConnectWithoutContext("RxWithSeqTsSize", MakeBoundCallback(&AppRx, i));
        s.Start(Seconds(0.0));
        s.Stop(Seconds(totalTime));

        Ptr<Socket> sock = Socket::CreateSocket(nodes.Get(src), UdpSocketFactory::GetTypeId());
        sock->Bind();
        sock->Connect(InetSocketAddress(ifaces.GetAddress(dst), port));
        double start = startRv->GetValue(1.0, 2.0); // random start: flows not synchronized
        Simulator::Schedule(Seconds(start), &CbrSend, i, sock, 0u, pktSize, interval, stop);

        flowsLog << i << ',' << src << ',' << dst << ',' << start << '\n';
    }
    flowsLog.close();

    // ---------------- control overhead: IPv4 Tx on all nodes
    Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", MakeCallback(&IpTx));

    // ---------------- FlowMonitor
    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

    // ---------------- run
    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    // ---------------- FlowMonitor: XML (raw) + CSV (data flows only)
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile(base + "_flowmon.xml", false, false);
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());
    std::ofstream fm(base + "_flowmon.csv");
    fm << "flowId,src,dst,dstPort,txPackets,rxPackets,lostPackets,delaySum_ms,jitterSum_ms,"
          "timesForwarded,avgHops\n";
    for (const auto& [id, st] : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);
        if (t.destinationPort < DATA_PORT_BASE)
        {
            continue; // routing control "flows"
        }
        double hops =
            st.rxPackets ? 1.0 + static_cast<double>(st.timesForwarded) / st.rxPackets : 0.0;
        fm << id << ',' << t.sourceAddress << ',' << t.destinationAddress << ','
           << t.destinationPort << ',' << st.txPackets << ',' << st.rxPackets << ','
           << st.lostPackets << ',' << st.delaySum.GetSeconds() * 1000.0 << ','
           << st.jitterSum.GetSeconds() * 1000.0 << ',' << st.timesForwarded << ',' << hops
           << '\n';
    }
    fm.close();

    // ---------------- summary
    double wall =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
    std::ofstream sum(base + "_summary.csv");
    sum << "protocol,nodes,speed,run,sim_time,flows,app_tx,app_rx,src_drops,ctrl_pkts,ctrl_bytes,"
           "wall_s\n";
    sum << protocol << ',' << nNodes << ',' << speed << ',' << run << ',' << totalTime << ','
        << nFlows << ',' << g_appTx << ',' << g_appRx << ',' << g_srcDrops << ',' << g_ctrlPkts
        << ',' << g_ctrlBytes << ',' << wall << '\n';
    sum.close();

    std::cout << tag.str() << "  tx=" << g_appTx << " rx=" << g_appRx
              << " PDR=" << (g_appTx ? 100.0 * g_appRx / g_appTx : 0.0) << "%"
              << "  srcDrops=" << g_srcDrops << "  ctrl=" << g_ctrlPkts << "  wall=" << wall << " s\n";

    Simulator::Destroy();
    g_txLog.close();
    g_rxLog.close();
    g_ctrlLog.close();
    return 0;
}
