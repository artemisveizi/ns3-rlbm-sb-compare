/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:  Vamsi Addanki (vamsi@inet.tu-berlin.de)
 */

#ifndef GEN_QUEUE_DISC_H
#define GEN_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include <array>

#include "unordered_map"
#include "ns3/simulator.h"
#include "shared-memory.h"

#include "/repo/ns3-datacenter/simulator/ns-3.35/src/traffic-control/model/RLAgent.h"

#include <stdio.h>
#include <string>

//test for getting trace msg as pointer 
struct RL_input_struct {
	double time = 0;
	double size = 0;
	double fct = 0;
	// double standalone_fct = 0;
	double slowdown = 0;
  double worst_slowdown = 0; //worst flow slowdown in one interval
  int num_finished_flows = 0;
  bool reset = false; //used to reset worst slowdown for each interval. RL agent will change it to true when it reads the states/rewards
	// double basertt = 0;
	double flowstart = 0;
	uint32_t priority = 0;
	bool incast = 0;
};

namespace ns3 {

class GenQueueDisc : public QueueDisc {
public:
  RLagent rlagent;
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief GenQueueDisc constructor
   */
  GenQueueDisc ();

  virtual ~GenQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded

  void setOutputFileQueue(std::string filename)
  {
    outputFileName = filename;
    isOutput = true;
    fp = fopen(outputFileName.c_str(), "w");
  }

  void setStrictPriority() {
    strict_priority = 1;
    round_robin=0;//Just to avoid clash
  }

  void setRoundRobin() {
    round_robin = 1;
    strict_priority = 0;//Just to avoid clash
  }

  double GetThroughputQueue(uint32_t p,double nanodelay);
  double GetThroughputPort(double nanodelay);
  double GetThroughputEnQueue(uint32_t p,double nanodelay);

  uint64_t GetBuffersize(uint32_t p);

  void setNPrior(uint32_t np) {
     nPrior = np;
     alphas = new double[nPrior];
     betas = new double[nPrior + 1];
     for (int i = 0; i < nPrior + 1; i++)
     {
       betas[i] = 1;
     }
  }
  double *alphas;
  double *betas;
  struct RL_input_struct * RL_input;
  int num_packet_dropped = 0;

  uint32_t getNPrior() {
    return nPrior;
  }

  void setPortBw(double bw){portBW = bw;}

  void SetSharedMemory(Ptr<SharedMemoryBuffer> sm){sharedMemory=sm;}

  void SetBufferAlgorithm(uint32_t alg){
    bufferalg=alg;
  }

  void SetPortId(uint32_t port){portId=port;}
  uint32_t getPortId(){return portId;}

  void SetFabWindow(Time t){FabWindow=t;}
  void SetFabThreshold(uint32_t n){FabThreshold=n;}

  void SetName(std::string name){switchname=name;}

  bool DynamicThresholds(uint32_t priority, Ptr<Packet> packet);

  void UpdateDequeueRate(double nanodelay);
  void UpdateNofP();
  void InvokeUpdates(double nanodelay);
  bool ActiveBufferManagement(uint32_t priority, Ptr<Packet> packet);

  bool FlowAwareBuffer(uint32_t priority, Ptr<Packet> packet);
  void RL_agent(double nanodelay);
  void UpdateAlphas(double* new_alphas);
  void InvokeUpdates_RLB(double nanodelay);
  bool RLBuffer(uint32_t priority, Ptr<Packet> packet);
  bool CompleteSharing(uint32_t priority, Ptr<Packet> packet);

  int DropAfd(double prob,uint32_t priority);
  void SetAfdWindow(Time t){AfdWindow=t;}
  void SetQrefAfd(uint32_t p, uint32_t ref);//{QRefAfd[p]=ref;}
  uint32_t GetQrefAfd(uint32_t p);//{return QRefAfd[p];}
  void SetDppWindow(Time t){DppWindow=t;}
  void SetDppThreshold(uint32_t n){DppThreshold=n;}
  bool IntelligentBuffer(uint32_t priority, Ptr<Packet> packet);

  bool AcceptPacket(uint32_t priority, Ptr<Packet> packet);

  void TrimPacket(Ptr<Packet> packetCopy);

  void writeQueueLength();

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  uint64_t droppedBytes[101];

  /*at enqueue*/
  Time firstSeen[101];
  Time lastAccepted[101];
  double numBytesSent[101];

  /*at dequeue*/
  Time firstSeenQueue[11];
  Time lastAcceptedQueue[11];
  double numBytesSentQueue[11];

  double DeqRate[11];
  double Deq[11];

  uint32_t nPrior;
  std::unordered_map<uint32_t,uint32_t> flowPrior;
  Time window;
  Time timeSinceLastChange;
  uint32_t new_index = 0;

  uint32_t dequeueIndex=0;
  uint32_t strict_priority;
  uint32_t round_robin;

  Ptr<SharedMemoryBuffer> sharedMemory;
  uint32_t bufferalg;
  uint32_t portId;
  uint32_t sat;
  std::string switchname; //optional

  std::unordered_map<uint32_t,std::pair<uint32_t,Time>> FlowCount; // FlowId --> <packetcounter, LastSeen>

  uint64_t bufferMax[11]={0};
  
  uint64_t updateInterval;
  bool firstTimeUpdate = true;

  uint64_t staticBuffer;
  uint64_t staticOccupancy=0;

  double alphaUnsched;

  Time FabWindow; // Needs to be set in experiment code.
  uint32_t FabThreshold; // Needs to be set in experiment code.
  Time AfdWindow; // Needs to be set in experiment code.
  uint32_t QRefAfd[11]; // Needs to be set in experiment code.
  Time DppWindow; // Needs to be set in experiment code.
  uint32_t DppThreshold; // Needs to be set in experiment code.

  Time timeSinceLastChangeAdf=Seconds(0);
  std::unordered_map<uint32_t,std::pair<uint32_t,uint32_t>> M; // FlowId --> <counterNow, Total count in last window>
  double MFair[11]={0};
  uint32_t Qold[11]={0};
  uint32_t DPPQueue=1;

  double a1=1.8;
  double a2=1.7;

  double portBW;// Needs to be set in experiment code via attribute.

  double nofP[11];

  bool is_homa;

  uint64_t txBytesInt=0;
  bool enableDPPQueue;

  FILE *fp;
  bool isOutput = false;
  std::string outputFileName;
  long outputFlag = 0;


  uint64_t maxSize;
  uint64_t maxSize0;
  uint64_t maxSize1;
};

} // namespace ns3

#endif /* GEN_QUEUE_DISC_H */
