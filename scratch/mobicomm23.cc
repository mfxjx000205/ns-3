#include "ns3/core-module.h"
#include "ns3/olsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main (int argc, char* argv[]){
    bool enableFlowMonitor = false;
    CommandLine cmd;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);
    uint32_t simNode = 100; //Number of nodes
    double simTime = 40; //Time of simulation
    std::string phyMode("DsssRate1Mbps");

    Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("512")); // packet size
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("1000kb/s")); // data rate

    //device setting
    //nodes
    NodeContainer node;
    //NodeContainer node_manet;
    node.Create(simNode);

    //wifi
    WifiHelper wifi;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChan = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChan.Create());
    //wifi-mac
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",StringValue(phyMode), "ControlMode",StringValue(phyMode));
    //Install wifi&mac to node
    NetDeviceContainer device;
    device = wifi.Install(wifiPhy,mac,node);

    //try to set initial olsr protocol
    OlsrHelper olsr;
    //OlsrHelper malicious_olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    //internet.Install(node_manet);
    internet.InstallAll();

    // set IP to all devices
    Ipv4AddressHelper ipadd;
    ipadd.SetBase("10.1.0.0", "255.255.252.0");
    ipadd.Assign(device);

    //set mobility
    MobilityHelper mobilitas;
    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    Ptr<PositionAllocator> posAlloc = pos.Create()->GetObject<PositionAllocator>();
    mobilitas.SetMobilityModel("ns3::RandomWaypointMobilityModel", 
                            "Speed", StringValue ("ns3::UniformRandomVariable[Min=0|Max=60]"),
                            "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                            "PositionAllocator", PointerValue(posAlloc));
    mobilitas.SetPositionAllocator (posAlloc);
    mobilitas.Install(node);

    //set UDP connnection
    uint32_t port = 5000;
    OnOffHelper cbrgenhelper("ns3::UdpSocketFactory", Address());
  
    cbrgenhelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=42.0]"));
    cbrgenhelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    ApplicationContainer cbrgen;
    AddressValue remoteaddr;

    for (uint32_t j = 0; j < 4; j++)
    {
    AddressValue remoteadd(InetSocketAddress(ipadd.Assign(device).GetAddress(j+1),port));
    cbrgenhelper.SetAttribute("Remote", remoteadd);
    cbrgen.Add(cbrgenhelper.Install(node.Get(j)));
    }

    cbrgen.Start(Seconds(0.0));
    cbrgen.Stop(Seconds(simTime-10.0));

    
     // set packet sink
    for (uint32_t k = 0; k < 4; k++)
    {
    PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress(ipadd.Assign(device).GetAddress(k+1),port));
    cbrgen.Add(sink.Install(node.Get(k+1)));
    }

    cbrgen.Start(Seconds(0.0));
    cbrgen.Stop(Seconds(simTime-10.0));
    
  
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    wifiPhy.EnablePcap("test", device.Get(0),true);
    wifiPhy.EnablePcap("test", device.Get(1),true);
    AnimationInterface anim ("manet.xml");
    Simulator::Run();
  
    monitor->CheckForLostPackets ();
    
    // calculate tx, rx, throughput, end to end delay, dand packet delivery ratio
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.0.2" && t.destinationAddress == "10.1.3.255"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
          std::cout << "  End to End Delay: " << i->second.delaySum.GetSeconds() / i->second.txPackets <<"\n";
          std::cout << "  Packet Delivery Ratio: " << ((i->second.rxPackets * 100) / i->second.txPackets) << "%" << "\n";
      }
     }
    
    Simulator::Destroy();
}
