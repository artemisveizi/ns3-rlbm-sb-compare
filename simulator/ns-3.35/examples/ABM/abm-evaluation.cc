/*
	Artemis Veizi
	Created: 24 March 2023
	10-Sender Star Toplogy with Bottleneck Link; Using RLBM
*/

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <fstream>
#include <iomanip>
#include <map>
#include <ctime>
#include <set>
#include <unordered_map>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gen-queue-disc.h"
#include "ns3/red-queue-disc.h"
#include "ns3/fq-pie-queue-disc.h"
#include "ns3/fq-codel-queue-disc.h"
#include "ns3/shared-memory.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "../src/traffic-control/model/gen-queue-disc.h"

# define PACKET_SIZE 1000
# define GIGA 1000000000

/*Buffer Management Algorithms*/
# define DT 101
# define FAB 102
# define CS 103
# define IB 104
# define ABM 110
# define RLB 111


/*Congestion Control Algorithms*/
# define RENO 0
# define CUBIC 1
# define DCTCP 2
# define HPCC 3
# define POWERTCP 4
# define HOMA 5 // HOMA is not supported at the moment.
# define TIMELY 6
# define THETAPOWERTCP 7

#define PORT_END 65530

extern "C"
{
#include "cdf.h"
}


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ABM_EVALUATION");

uint32_t PORT_START[512] = { 4444 };

double alpha_values[8] = { 1 };

Ptr<OutputStreamWrapper> fctOutput;
AsciiTraceHelper asciiTraceHelper;

Ptr<OutputStreamWrapper> torStats;
AsciiTraceHelper torTraceHelper;

Ptr<OutputStreamWrapper> thruputOutput;

const int NUM_NODES = 10;
Ptr<SharedMemoryBuffer> sharedMemoryNode[NUM_NODES];
Ptr<SharedMemoryBuffer> sharedMemoryLink, sharedMemorySink;
QueueDiscContainer bottleneckQueue;
QueueDiscContainer ToRQueueDiscs[1];

double poission_gen_interval(double avg_rate)
{
	if (avg_rate > 0)
		return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
	else
		return 0;
}

template<typename T>
T rand_range(T min, T max)
{
	return min + ((double)max - min) * rand() / RAND_MAX;
}

double baseRTTNano;
double nicBw;

struct RL_input_struct* RL_input;
int numFinished = 0; 
//invoked when each flow finishes, get stats for each flow, calculate avg flow slowdown
void TraceMsgFinish(Ptr<OutputStreamWrapper> stream, struct RL_input_struct* RL_input_inmsg, double size, double start, bool incast, uint32_t prior)
{
	numFinished++;
	//std::cout << numFinished << " SZ = " << size << std::endl;
	double fct, standalone_fct, slowdown;
	fct = Simulator::Now().GetNanoSeconds() - start;
	// std::cout << "FCT reported: " << fct << std::endl;
	standalone_fct = baseRTTNano + size / nicBw;
	slowdown = fct / standalone_fct;
	RL_input_inmsg->time = Simulator::Now().GetSeconds();
	RL_input_inmsg->size = size;
	RL_input_inmsg->fct = fct;
	// RL_input_inmsg->standalone_fct = standalone_fct;
	RL_input_inmsg->slowdown = slowdown;
	//reset cumulative variables when a new interval begins 
	if (RL_input_inmsg->reset) {
		RL_input_inmsg->worst_slowdown = 0;
		RL_input_inmsg->num_finished_flows = 0;
		RL_input_inmsg->reset = false;
	}
	//only remember biggest slowdown
	RL_input_inmsg->worst_slowdown = (slowdown > RL_input_inmsg->worst_slowdown) ? slowdown : RL_input_inmsg->worst_slowdown;
	RL_input_inmsg->num_finished_flows += 1;
	// RL_input_inmsg->basertt = baseRTTNano / 1e3;
	RL_input_inmsg->flowstart = (start / 1e3 - Seconds(10).GetMicroSeconds());
	RL_input_inmsg->priority = prior;
	RL_input_inmsg->incast = incast;
	// printf("fct outside: %lf\n", RL_input_inmsg->fct);

	//dump fct slowdown to file
	*stream->GetStream()
		<< Simulator::Now().GetSeconds()
		<< " " << size
		<< " " << fct
		<< " " << standalone_fct
		<< " " << slowdown
		<< " " << baseRTTNano / 1e3
		<< " " << (start / 1e3 - Seconds(10).GetMicroSeconds())
		<< " " << prior
		<< " " << incast
		<< std::endl;
}

void
InvokeToRStats(Ptr<OutputStreamWrapper> stream, uint32_t BufferSize, uint32_t node, double nanodelay) {
	Ptr<SharedMemoryBuffer> sm = sharedMemoryLink; // sharedMemoryNode[node];
	QueueDiscContainer queues = ToRQueueDiscs[0];
	double totalThroughput = 0;
	for (uint32_t i = 0; i < queues.GetN(); i++) {
		Ptr<GenQueueDisc> genDisc = DynamicCast<GenQueueDisc>(queues.Get(i));
		totalThroughput += genDisc->GetThroughputPort(nanodelay);
	}
	totalThroughput = totalThroughput / queues.GetN();

	*stream->GetStream()
		<< " " << Simulator::Now().GetSeconds()
		<< " " << node
		<< " " << double(BufferSize) / 1e6
		<< " " << 100 * double(sm->GetOccupiedBuffer()) / BufferSize
		<< " " << 100 * totalThroughput
		<< " " << 100 * double(sm->GetPerPriorityOccupied(0)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(1)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(2)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(3)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(4)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(5)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(6)) / BufferSize
		<< " " << 100 * double(sm->GetPerPriorityOccupied(7)) / BufferSize
		<< std::endl;

	Simulator::Schedule(NanoSeconds(nanodelay), InvokeToRStats, stream, BufferSize, node, nanodelay);
}

void
InvokePerPortToRStats(Ptr<OutputStreamWrapper> stream, uint32_t BufferSize, uint32_t node, double nanodelay) {
	Ptr<SharedMemoryBuffer> sm = sharedMemoryNode[node];
	QueueDiscContainer queues = bottleneckQueue;
	for (uint32_t i = 0; i < queues.GetN(); i++) {
		Ptr<GenQueueDisc> genDisc = DynamicCast<GenQueueDisc>(queues.Get(i));
		double totalThroughput = genDisc->GetThroughputPort(nanodelay);
		*stream->GetStream()
			<< " " << Simulator::Now().GetSeconds()
			<< " " << node
			<< " " << genDisc->getPortId()
			<< " " << double(BufferSize) / 1e6
			<< " " << 100 * double(genDisc->GetCurrentSize().GetValue()) / BufferSize
			<< " " << 100 * totalThroughput
			<< std::endl;
	}
	Simulator::Schedule(NanoSeconds(nanodelay), InvokePerPortToRStats, stream, BufferSize, node, nanodelay);
}

uint64_t prevBytes = 0;
Time prevTime = Seconds(0);

void
GetThroughput(Ptr<OutputStreamWrapper> stream, Ptr<PacketSink> sinkApp) {
	Time t = Simulator::Now();

	double bytesTotal = sinkApp->GetTotalRx();
	float thruput = (bytesTotal - prevBytes) * 8.0 / (1024 * 1024) / (t.GetSeconds() - prevTime.GetSeconds());
	prevTime = t;
	*stream->GetStream() << "Time: \t" << t.GetSeconds() << "\tThroughput:\t" << thruput << "Mbps" << std::endl;
	prevBytes = bytesTotal;

	Simulator::Schedule(Seconds(0.01), &GetThroughput, stream, sinkApp);
}

void
LimitsTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
	*stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
PacketsInQueueTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
	*stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
CheckQueueSize(Ptr<OutputStreamWrapper> stream,  Ptr<QueueDisc> queue) {
	Time t = Simulator::Now();
	uint32_t qSize = queue->GetCurrentSize().GetValue();
	*stream->GetStream() << "Time: \t" << t.GetSeconds() << "Bytes in Queue: " << qSize << std::endl;
	Simulator::Schedule(Seconds(0.01), &CheckQueueSize, stream, queue);
}

void connectTx(Ptr<Packet const> packet) {
	std::cout << "connected" << std::endl;
}

void maxBytesSent(uint32_t maxBytes) {
	std::cout << "all sent " << std::to_string(maxBytes) << std::endl;
}
void
CheckDTQSize(Ptr<OutputStreamWrapper> stream, Ptr<Queue<Packet>> queue) {
	Time t = Simulator::Now();
	uint32_t qSize = queue->GetNPackets();
	*stream->GetStream() << "Time: \t" << t.GetSeconds() << "Packets in Queue: " << qSize << std::endl;
	Simulator::Schedule(Seconds(0.01), &CheckDTQSize, stream, queue);
}

static void
GoodputSampling(ApplicationContainer app, Ptr<OutputStreamWrapper> stream, float period)
{
	Simulator::Schedule(Seconds(period), &GoodputSampling, app, stream, period);
	double goodput;
	uint64_t totalPackets = DynamicCast<PacketSink>(app.Get(0))->GetTotalRx();
	goodput = totalPackets * 8 / (Simulator::Now().GetSeconds() * 1024 * 1024); // Kbit/s
	*stream->GetStream() << "Time: \t" << Simulator::Now().GetSeconds() << "\tGoodput:\t" << goodput << "Mbps" << std::endl;
}

static void PingRtt(std::string context, Time rtt)
{
	std::cout << context << "=" << rtt.GetMilliSeconds() << " ms" << std::endl;
}

int
main(int argc, char* argv[])
{
	CommandLine cmd;

	std::string bandwidth = "10Mbps";
	std::string delay = "10ms"; // "5ms";
	std::string queueDiscType = "RLB";
	std::string tcpProtocol = "TcpBasic";
	uint32_t queueDiscSize = 500;

	cmd.AddValue("tcpProtocol", "Select TCP Version in {TcpBasic, TcpBic, TcpHighSpeed, TcpHtcp, TcpHybla, TcpNewReno, TcpScalable, TcpWestwood, TcpYeah}", tcpProtocol);
	cmd.AddValue("queueDiscSize", "Bottleneck queue disc size in packets", queueDiscSize);

	uint32_t netdevicesQueueSize = 1000;
	bool bql = false;
	bool dropTail = (queueDiscType == "droptail");

	uint32_t flowsPacketsSize = 1000;

	float startTime = 0.1f; // in s
	float simDuration = 60;
	float samplingPeriod = 1;
	float stopTime = startTime + simDuration;

	unsigned randomSeed = 5;
	cmd.AddValue("randomSeed", "Random seed, 0 for random generated", randomSeed);
	std::cout << randomSeed << std::endl;

	double load = 0.6;
	cmd.AddValue("load", "Load of the network, 0.0 - 1.0", load);

	double accessLinkCapacity = 0.1; //Gbps --->accessLink
	double bottleneckLinkCapacity = 0.01; //Gbps 

	double linkLatency = 100;
	cmd.AddValue("accessLinkCapacity", "Spine <-> Leaf capacity in Gbps", accessLinkCapacity);
	cmd.AddValue("bottleneckLinkCapacity", "Leaf <-> Server capacity in Gbps", bottleneckLinkCapacity);
	cmd.AddValue("linkLatency", "linkLatency in microseconds", linkLatency);

	uint32_t TcpProt = CUBIC;
	cmd.AddValue("TcpProt", "Tcp protocol", TcpProt);

	uint32_t nPrior = 2; // number queues in switch ports
	cmd.AddValue("nPrior", "number of priorities", nPrior);

	uint32_t algorithm = CS; // RLB;
	cmd.AddValue("algorithm", "Buffer Management algorithm", algorithm);

	std::string sched = "roundRobin";
	cmd.AddValue("sched", "scheduling", sched);

	std::string alphasFile = "/home/vamsi/src/phd/ns3-datacenter/simulator/ns-3.35/examples/ABM/alphas"; // On lakewood
	std::string cdfFileName = "/home/vamsi/src/phd/ns3-datacenter/simulator/ns-3.35/examples/ABM/websearch.txt";
	std::string cdfName = "WS";
	cmd.AddValue("alphasFile", "alpha values file (should be exactly nPrior lines)", alphasFile);
	cmd.AddValue("cdfFileName", "File name for flow distribution", cdfFileName);
	cmd.AddValue("cdfName", "Name for flow distribution", cdfName);

	uint32_t printDelay = 30 * 1e3;
	cmd.AddValue("printDelay", "printDelay in NanoSeconds", printDelay);

	double alphaUpdateInterval = 1;
	cmd.AddValue("alphaUpdateInterval", "(Number of Rtts) update interval for alpha values in ABM", alphaUpdateInterval);


	std::string fctOutFile = "./fcts.txt";
	cmd.AddValue("fctOutFile", "File path for FCTs", fctOutFile);

	std::string maxSizeOutfile = "./maxsz.txt";
	cmd.AddValue("maxSizeOutFile", "File path for buffer max size", maxSizeOutfile);

	std::string flowMonOutfile = "./flowmon.xml";
	cmd.AddValue("flowMonOutfile", "File path for flow monitor", flowMonOutfile);

	std::string thruputOutFile = "./thru.txt";
	cmd.AddValue("thruputOutFile", "File path for throughput", thruputOutFile);

	std::string torOutFile = "./tor.txt";
	cmd.AddValue("torOutFile", "File path for ToR statistic", torOutFile);

	uint32_t rto = 10 * 1000; // in MicroSeconds, 5 milliseconds.
	cmd.AddValue("rto", "min Retransmission timeout value in MicroSeconds", rto);

	uint32_t torPrintall = 0;
	cmd.AddValue("torPrintall", "torPrintall", torPrintall);

	uint32_t updateRL = 100000000;
	cmd.AddValue("updateRL", "Update interval for RL state", updateRL);

	/*Parse CMD*/
	cmd.Parse(argc, argv);


	fctOutput = asciiTraceHelper.CreateFileStream(fctOutFile);

	*fctOutput->GetStream()
		<< "time "
		<< "flowsize "
		<< "fct "
		<< "basefct "
		<< "slowdown "
		<< "basertt "
		<< "flowstart "
		<< "priority "
		<< "incast "
		<< std::endl;

	thruputOutput = asciiTraceHelper.CreateFileStream(thruputOutFile);

	RL_input = new RL_input_struct{};

	torStats = torTraceHelper.CreateFileStream(torOutFile);

	if (!torPrintall) {
		*torStats->GetStream()
			<< "time "
			<< "tor "
			<< "bufferSizeMB "
			<< "occupiedBufferPct "
			<< "uplinkThroughput "
			<< "priority0 "
			<< "priority1 "
			<< "priority2 "
			<< "priority3 "
			<< "priority4 "
			<< "priority5 "
			<< "priority6 "
			<< "priority7 "
			<< std::endl;
	}
	else {
		*torStats->GetStream()
			<< "time "
			<< "tor "
			<< "portId "
			<< "bufferSizeMB "
			<< "PortOccBuffer "
			<< "PortThroughput "
			<< std::endl;
	}

	/*Reading alpha values from file*/
	std::string line;
	std::fstream aFile;
	aFile.open(alphasFile);
	uint32_t p = 0;
	while (getline(aFile, line) && p < 8) { // hard coded to read only 8 alpha values.
		std::istringstream iss(line);
		double a;
		iss >> a;
		alpha_values[p] = a;
		// std::cout << "Alpha-"<< p << " = "<< a << " " << alpha_values[p]<< std::endl;
		p++;
	}
	aFile.close();


	uint64_t allflows = 0;
	srand(randomSeed);
	struct cdf_table* cdfTable = new cdf_table();
	init_cdf(cdfTable);
	load_cdf(cdfTable, cdfFileName.c_str());

	uint32_t BufferSize = 1000 * 2 * queueDiscSize;
	std::cout << "BUF SIZE = " << BufferSize;
	double ACCESS_LINK_CAPACITY = accessLinkCapacity * GIGA;
	double BOTTLENECK_LINK_CAPACITY = bottleneckLinkCapacity * GIGA;
	double requestRate = 500 * load; // load* ACCESS_LINK_CAPACITY * 100 * NUM_NODES / oversubRatio / (8 * avg_cdf(cdfTable));
	std::cout << "Average Request Rate: " << requestRate << " per second" << std::endl;

	baseRTTNano = linkLatency * 1e3;
	nicBw = bottleneckLinkCapacity;
	// std::cout << "bandwidth " << accessLinkCapacity << "gbps" <<  " rtt " << linkLatency*8 << "us" << " BDP " << bdp/1e6 << std::endl;

	if (load < 0.0)
	{
		NS_LOG_ERROR("Illegal Load value");
		return 0;
	}

	Config::SetDefault("ns3::GenQueueDisc::updateInterval", UintegerValue(updateRL));
	Config::SetDefault("ns3::GenQueueDisc::staticBuffer", UintegerValue(0));
	Config::SetDefault("ns3::GenQueueDisc::BufferAlgorithm", UintegerValue(algorithm));
	Config::SetDefault("ns3::SharedMemoryBuffer::BufferSize", UintegerValue(BufferSize));
	Config::SetDefault("ns3::FifoQueueDisc::MaxSize", StringValue(std::to_string(queueDiscSize) + "p")); //QueueSizeValue(QueueSize("0.1MB")));

	TrafficControlHelper tchBottleneck;
	uint16_t handle;
	TrafficControlHelper::ClassIdList cid;

	/*General TCP Socket settings. Mostly used by various congestion control algorithms in common*/
	Config::SetDefault("ns3::TcpSocket::ConnTimeout", TimeValue(MilliSeconds(10))); // syn retry interval
	Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MicroSeconds(rto)));  //(MilliSeconds (5))
	Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(NanoSeconds(10))); //(MicroSeconds (100))
	Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(200))); //TimeValue (MicroSeconds (80))
	Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1073725440)); //1073725440
	Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1073725440));
	Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(6));  // Syn retry count
	Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(true));
	Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
	Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
	Config::SetDefault("ns3::TcpSocket::PersistTimeout", TimeValue(Seconds(20)));


	/*CC Configuration
	switch (TcpProt) {
	case RENO:
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpNewReno::GetTypeId()));
		break;
	case CUBIC:
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpCubic::GetTypeId()));
		break;
	case BIC:
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpBic::GetTypeId()));
		break;
	default:
		std::cout << "Error in CC configuration" << std::endl;
		return 0;
	}*/
	if (tcpProtocol == "TcpBic") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpBic::GetTypeId())); }
	else if (tcpProtocol == "TcpHtcp") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpHtcp::GetTypeId())); }
	else if (tcpProtocol == "TcpHighSpeed") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpHighSpeed::GetTypeId())); }
	else if (tcpProtocol == "TcpHybla") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpHybla::GetTypeId())); }
	else if (tcpProtocol == "TcpNewReno") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpNewReno::GetTypeId())); }
	else if (tcpProtocol == "TcpScalable") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpScalable::GetTypeId())); }
	else if (tcpProtocol == "TcpWestwood") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpWestwood::GetTypeId())); }
	else if (tcpProtocol == "TcpYeah") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpYeah::GetTypeId())); }
	else if (tcpProtocol == "TcpCubic") { Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ns3::TcpCubic::GetTypeId())); }

	Config::SetDefault("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(true));
	Config::SetDefault("ns3::GenQueueDisc::nPrior", UintegerValue(nPrior));
	Config::SetDefault("ns3::GenQueueDisc::RoundRobin", UintegerValue(1));
	Config::SetDefault("ns3::GenQueueDisc::StrictPriority", UintegerValue(0));
	handle = tchBottleneck.SetRootQueueDisc("ns3::GenQueueDisc");
	cid = tchBottleneck.AddQueueDiscClasses(handle, nPrior, "ns3::QueueDiscClass");
	for (uint32_t num = 0; num < nPrior; num++) {
		tchBottleneck.AddChildQueueDisc(handle, cid[num], "ns3::FifoQueueDisc");
	}

	NodeContainer nodes;
	nodes.Create(NUM_NODES);

	NodeContainer nd, ns;
	nd.Create(1);
	ns.Create(1);

	// Create and configure access link and bottleneck link
	PointToPointHelper accessLink;
	accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
	accessLink.SetChannelAttribute("Delay", StringValue("0.1ms"));
	if (dropTail) {
		accessLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("10000p"));
	}

	PointToPointHelper bottleneckLink;
	bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bandwidth));
	bottleneckLink.SetChannelAttribute("Delay", StringValue(delay));

	if (dropTail) {
		bottleneckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(std::to_string(queueDiscSize) + "p"));
	}

	InternetStackHelper stack;
	Ipv4GlobalRoutingHelper globalRoutingHelper;
	stack.SetRoutingHelper(globalRoutingHelper);

	stack.Install(nodes);
	stack.Install(nd);
	stack.Install(ns);

	sharedMemoryLink = CreateObject<SharedMemoryBuffer>();
	sharedMemoryLink->SetAttribute("BufferSize", UintegerValue(BufferSize));
	sharedMemoryLink->SetSharedBufferSize(BufferSize);

	sharedMemorySink = CreateObject<SharedMemoryBuffer>();
	sharedMemorySink->SetAttribute("BufferSize", UintegerValue(BufferSize * 1000));
	sharedMemorySink->SetSharedBufferSize(BufferSize * 1000);

	TrafficControlHelper tchPfifoFastAccess;
	uint16_t fasthandle;
	TrafficControlHelper::ClassIdList fastcid;
	tchPfifoFastAccess.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("10000p"));

	// Bottleneck link traffic control configuration
	Ipv4AddressHelper address;
	address.SetBase("192.168.0.0", "255.255.255.0");

	uint32_t nodePortId[NUM_NODES + 1] = { 0 }; //??????
	uint32_t sinkPortId[1] = { 0 }; //??????

	QueueDiscContainer queueDiscs;
	for (uint32_t node = 0; node < NUM_NODES; node++) {
		address.NewNetwork();
		NodeContainer nodeContainer = NodeContainer(nodes.Get(node), nd.Get(0));
		NetDeviceContainer devices = accessLink.Install(nodeContainer);
		tchPfifoFastAccess.Install(devices);
		address.Assign(devices);
	}

	NodeContainer bottleneckNodes = NodeContainer(nd.Get(0), ns.Get(0));
	NetDeviceContainer devicesBottleneckLink = bottleneckLink.Install(bottleneckNodes);
	queueDiscs = tchBottleneck.Install(devicesBottleneckLink);
	ToRQueueDiscs[0].Add(queueDiscs.Get(0));
	bottleneckQueue.Add(queueDiscs.Get(0));
	bottleneckQueue.Add(queueDiscs.Get(1));
	Ptr<GenQueueDisc> genDisc[2];
	genDisc[0] = DynamicCast<GenQueueDisc>(queueDiscs.Get(0));
	genDisc[0]->SetSharedMemory(sharedMemoryLink);
	genDisc[1] = DynamicCast<GenQueueDisc>(queueDiscs.Get(1));
	genDisc[1]->SetSharedMemory(sharedMemorySink);
	genDisc[0]->SetPortId(nodePortId[NUM_NODES]++);
	genDisc[1]->SetPortId(sinkPortId[0]++);
	genDisc[0]->setOutputFileQueue(fctOutFile + ".log");

	for (uint32_t i = 0; i < 2; i++) {
		genDisc[i]->setNPrior(nPrior); // IMPORTANT. This will also trigger "alphas = new ..."
		genDisc[i]->setPortBw(bottleneckLinkCapacity);
		std::cout << bottleneckLinkCapacity << std::endl;
		// genDisc[i]->SetSharedMemory(sharedMemoryLink);
		genDisc[i]->SetBufferAlgorithm(algorithm);
		for (uint32_t n = 0; n < nPrior; n++) {
			genDisc[i]->alphas[n] = alpha_values[n];
		}
		//add inputs for RL 
		genDisc[i]->RL_input = RL_input;
	}
	address.NewNetwork();
	Ipv4InterfaceContainer interfacesBottleneck = address.Assign(devicesBottleneckLink);
	Ipv4InterfaceContainer nsInterface;
	nsInterface.Add(interfacesBottleneck.Get(1));

	AsciiTraceHelper ascii;

	if (dropTail) {
		Ptr<Queue<Packet> > queue = StaticCast<PointToPointNetDevice>(devicesBottleneckLink.Get(0))->GetQueue();
		Ptr<OutputStreamWrapper> streamPacketsInQueue = ascii.CreateFileStream("/repo/ns3-datacenter/simulator/ns-3.35/examples/ABM/results/multi-seed/" + std::to_string(randomSeed) + tcpProtocol + queueDiscType + std::to_string(queueDiscSize) + "pktsInQueue.txt");
		queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&PacketsInQueueTrace, streamPacketsInQueue));
	}
	else {
		Ptr<QueueDisc> queue = queueDiscs.Get(0);
		Ptr<OutputStreamWrapper> streamPacketsInQueue = ascii.CreateFileStream("/repo/ns3-datacenter/simulator/ns-3.35/examples/ABM/results/multi-seed/" + std::to_string(randomSeed) + tcpProtocol + queueDiscType + std::to_string(queueDiscSize) + "pktsInQueue.txt");
		queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&PacketsInQueueTrace, streamPacketsInQueue));
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(flowsPacketsSize));
	// Set TCP Protocol for all instantiated TCP Sockets
	if (tcpProtocol != "TcpBasic") {
		Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::" + tcpProtocol)));
	}

	// Flows configuration
	uint16_t port = 9;
	ApplicationContainer sinkApps, sourceApps;
	// Configure and install upload flow

	InetSocketAddress socketAddressUp = InetSocketAddress(nsInterface.GetAddress(0), port);
	BulkSendHelper bulkSendHelperUp("ns3::TcpSocketFactory", Address());
	bulkSendHelperUp.SetAttribute("Remote", AddressValue(socketAddressUp));

	NodeContainer senders[NUM_NODES] = { NodeContainer(nodes.Get(0)), NodeContainer(nodes.Get(1)), NodeContainer(nodes.Get(2)), NodeContainer(nodes.Get(3)), NodeContainer(nodes.Get(4)), NodeContainer(nodes.Get(5)), NodeContainer(nodes.Get(6)), NodeContainer(nodes.Get(7)), NodeContainer(nodes.Get(8)), NodeContainer(nodes.Get(9)) };

	int numSends = 0;
	for (int i = 0; i < NUM_NODES; i++) {
		double sendStart = startTime + poission_gen_interval(requestRate);
		while (sendStart < stopTime * 0.75 && sendStart > startTime)
		{
			uint32_t prioritysend = 0;
			uint64_t flowSize = gen_random_cdf(cdfTable);
			while (flowSize == 0) { flowSize = gen_random_cdf(cdfTable); }
			allflows += flowSize;
			if (numSends > 0 && flowSize > 2000000) {
				prioritysend = 1;
			}

			Ptr<Node> rxNode = ns.Get(0);
			Ptr<Ipv4> ipv4 = rxNode->GetObject<Ipv4>();
			Ipv4InterfaceAddress rxInterface = ipv4->GetAddress(1, 0);
			Ipv4Address rxAddress = rxInterface.GetLocal();
			InetSocketAddress ad(rxAddress, port);
			Address sinkAddress(ad);

			PacketSinkHelper sinkHelperUp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
			sinkApps.Add(sinkHelperUp.Install(ns));
			sinkApps.Get(numSends)->SetStartTime(Seconds(sendStart));
			sinkApps.Get(numSends)->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
			sinkApps.Get(numSends)->SetAttribute("priority", UintegerValue(0)); // ack packets are prioritized
			sinkApps.Get(numSends)->SetAttribute("priorityCustom", UintegerValue(0)); // ack packets are prioritized
			sinkApps.Get(numSends)->SetAttribute("TotalQueryBytes", UintegerValue(flowSize));
			sinkApps.Get(numSends)->TraceConnectWithoutContext("FlowFinish", MakeBoundCallback(&TraceMsgFinish, fctOutput, RL_input));


			bulkSendHelperUp.SetAttribute("Remote", AddressValue(sinkAddress));
			bulkSendHelperUp.SetAttribute("MaxBytes", UintegerValue(flowSize));
			bulkSendHelperUp.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
			bulkSendHelperUp.SetAttribute("SendSize", UintegerValue(flowSize));
			bulkSendHelperUp.SetAttribute("FlowId", UintegerValue(numSends));
			bulkSendHelperUp.SetAttribute("priority", UintegerValue(prioritysend));
			bulkSendHelperUp.SetAttribute("priorityCustom", UintegerValue(prioritysend));
			sourceApps.Add(bulkSendHelperUp.Install(senders[i]));
			// std::cout << flowSize << "b @ Time = " << sendStart << std::endl;
			sourceApps.Get(numSends)->SetStartTime(Seconds(sendStart));
			numSends++;

			port++;
			if (port > PORT_END) {
				port = 4444;
			}
			sendStart += 100 * poission_gen_interval(requestRate);
		}
	}

	if (!torPrintall) {
		Simulator::Schedule(Seconds(0), InvokeToRStats, torStats, BufferSize, 0, printDelay);
	}
	else {
		Simulator::Schedule(Seconds(0), InvokePerPortToRStats, torStats, BufferSize, 0, printDelay);
	}

	V4PingHelper ping = V4PingHelper(nsInterface.GetAddress(0));
	ping.Install(nodes.Get(0));
	Config::Connect("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback(&PingRtt));

	sinkApps.Stop(Seconds(stopTime));
	sourceApps.Stop(Seconds(stopTime * 0.75));

	Ptr<FlowMonitor> flowMonitor;
	FlowMonitorHelper flowHelper;
	flowMonitor = flowHelper.InstallAll();

	Ptr<QueueDisc> queuePackets;
	if (dropTail) {
		Ptr<Queue<Packet>> queuePackets = StaticCast<PointToPointNetDevice>(devicesBottleneckLink.Get(0))->GetQueue();
		Ptr<OutputStreamWrapper> packetsStream = ascii.CreateFileStream("/repo/ns3-datacenter/simulator/ns-3.35/examples/ABM/results/multi-seed/" + std::to_string(randomSeed) + tcpProtocol + queueDiscType + std::to_string(queueDiscSize) + "bytesInQueue.txt");
		Simulator::Schedule(Seconds(0.01), &CheckDTQSize, packetsStream, queuePackets);
	}
	else {
		queuePackets = queueDiscs.Get(0);
		Ptr<OutputStreamWrapper> packetsStream = ascii.CreateFileStream("/repo/ns3-datacenter/simulator/ns-3.35/examples/ABM/results/multi-seed/" + std::to_string(randomSeed) + tcpProtocol + queueDiscType + std::to_string(queueDiscSize) + "bytesInQueue.txt");
		Simulator::Schedule(Seconds(0.01), &CheckQueueSize, packetsStream, queuePackets);
	}
	/*
	Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
	Ptr<OutputStreamWrapper> throughputStream = ascii.CreateFileStream("/repo/ns3-datacenter/simulator/ns-3.35/examples/ABM/results/multi-seed/" + std::to_string(randomSeed) + tcpProtocol + queueDiscType + std::to_string(queueDiscSize) + "throughput.txt");
	Simulator::Schedule(Seconds(0.01), &GetThroughput, throughputStream, sink);
	*/

	std::cout << "Running the Simulation...!" << std::endl;
	Simulator::Stop(Seconds(stopTime));
	Simulator::Run();
	
	flowMonitor->SerializeToXmlFile(flowMonOutfile, true, true);

	Simulator::Destroy();

	if (!dropTail) {
		QueueDisc::Stats st = queueDiscs.Get(0)->GetStats();
		std::cout << st << std::endl;
	}
	std::cout << "Total Bytes on All Flows: " << allflows << std::endl;
	uint32_t totalRx = 0;
	for (int i = 0; i < numSends; i++) {
		Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(i));
		totalRx += sink->GetTotalRx();
		// std::cout << "Total Rx on this app: " << std::to_string(sink->GetTotalRx()) << std::endl;
	}
	std::cout << "Total Bytes Received: " << totalRx << std::endl;
	std::cout << "Average thruput: " << totalRx * 8.0/ stopTime << std::endl;
	*thruputOutput->GetStream()
		<< totalRx * 8.0 / stopTime
		<< std::endl;
	if (!dropTail) { std::cout << queuePackets->GetNInternalQueues() << std::endl; }

	free_cdf(cdfTable);
	return 0;
}
