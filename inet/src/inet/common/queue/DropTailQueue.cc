//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/common/INETDefs.h"

#include "inet/common/queue/DropTailQueue.h"
#include "inet/linklayer/ethernet/EtherMACBase.h"
#include "inet/linklayer/ethernet/Ethernet.h"
#include "transport/HomaPkt.h"
#include <fstream>
#include <cqueue.h>

extern std::ofstream logFile;
extern std::ofstream logFile2;
extern bool logPacketEvents;

namespace inet {

Define_Module(DropTailQueue);

void DropTailQueue::initialize()
{
    PassiveQueueBase::initialize();

    queue.setName(par("queueName"));

    // Configure the HomaPkt priority sort function
    if (par("transportType").stdstringValue() == "VectioTransport") {
        queue.setup(&HomaPkt::comparePrios);
    }
    else if (par("transportType").stdstringValue() == "VectioSenderTransport") {
        queue.setup(&HomaPkt::comparePrios);
    } else if (par("transportType").stdstringValue() == "PseudoIdealTransport") {
        queue.setup(&HomaPkt::compareSizeAndPrios);
    } else {
        assert(false);
    }

    //statistics
    emit(queueLengthSignal, queue.length());
    emit(queueByteLengthSignal, queue.getByteLength());

    outGate = gate("out");

    // configuration
    frameCapacity = par("frameCapacity");
    logFile2 << "Setting frame capacity: " << frameCapacity << std::endl;

    mac = getNextMacLayer();
    if (!mac) {
        EV_WARN << "Warning. No mac connected to queue module.\n";
    }

}

cMessage *DropTailQueue::enqueue(cMessage *msg)
{
    double txRate = 0.0; // transmit speed of the next mac layer 
    if (mac) {
        txRate = dynamic_cast<EtherMACBase*>(mac)->getTxRate();
    }

    logFile2 << simTime() << " queue: " << queue.length() << " frameCapacity: " << frameCapacity << std::endl;
    bool drop = false;

    if (frameCapacity && queue.length() >= frameCapacity) {
        EV << "Queue full, dropping packet.\n";
        logFile2 <<  simTime() << "Queue full, dropping packet.\n";
        cPacket* pktTemp = dynamic_cast<cPacket*>(msg);
        pktTemp = HomaPkt::searchEncapHomaPkt(pktTemp);
        if (pktTemp) {
            HomaPkt* homaPkt = check_and_cast<HomaPkt*>(pktTemp);
            cModule* parentHost = this->getParentModule();
            cModule* grandParentHost = parentHost->getParentModule();
            cModule* grandGrandParentHost = grandParentHost->getParentModule();
            // if(homaPkt->getMsgId() == 288){
                switch (homaPkt->getPktType()) {
                    case PktType::REQUEST:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " req packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::GRANT:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " grant packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::SCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " sched data packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getSchedDataFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::UNSCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " unsched data packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getUnschedFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    default:
                        logFile2 << simTime() << "HomaPkt arrived at the queue has unknown type. " << homaPkt->getPktType() << " " << std::endl;
                        logFile2.flush();
                }
                switch (homaPkt->getPktType()) {
                    case PktType::REQUEST:
                        drop = false;
                        break;
                    case PktType::GRANT:
                        drop = false;
                        break;
                    case PktType::SCHED_DATA:
                        drop = true;
                        break;
                    case PktType::UNSCHED_DATA:
                        drop = false;
                        break;
                    default:
                        drop = false;
                        break;
                }
            // }
        }
        if(drop == true){
            return msg;
        }
    }

    // at the queueing time, we check how much of the previous transmitting
    // packet is remained to be serialized and trigger queueLength signals
    // with current queue length and the remainder of that packet.
    int pktOnWire = 0;
    int txPktBitsRemained = 0; 

    if (lastTxPktDuration.second > simTime()) {
        txPktBitsRemained =  (int)((lastTxPktDuration.second - simTime()).dbl() * txRate);
        pktOnWire = 1;
    }

    if (queue.length() == 0 && pktOnWire == 0) {
            queueEmpty++;
    } else if ((queue.length() == 0 && pktOnWire == 1) 
            || (queue.length() == 1 && pktOnWire == 0)) {
        queueLenOne++;
    }

    // The timeavg of queueLength queueByteLength will not be accurate if
    // the queue(Byte)LengthSignal is emitted before a packet is stored at
    // the queue (similar to the following line) however the average of
    // queueLength at the packet arrival time at the queue will be accurate.
    // If you want the timeavg of queueLength instead, comment out the
    // following line and decomment the other emit invokations of
    // queueLengthSignal in this file.
    emit(queueLengthSignal, queue.length() + pktOnWire);
    emit(queueByteLengthSignal, queue.getByteLength() + (txPktBitsRemained >> 3));
    cPacket* pkt = check_and_cast<cPacket*>(msg);
    queue.insert(pkt);

    if(logPacketEvents){
        cPacket* pktTemp = dynamic_cast<cPacket*>(msg);
        pktTemp = HomaPkt::searchEncapHomaPkt(pktTemp);
        if (pktTemp) {
            HomaPkt* homaPkt = check_and_cast<HomaPkt*>(pktTemp);
            cModule* parentHost = this->getParentModule();
            cModule* grandParentHost = parentHost->getParentModule();
            cModule* grandGrandParentHost = grandParentHost->getParentModule();
            if(homaPkt->getMsgId() == 288){
                switch (homaPkt->getPktType()) {
                    case PktType::REQUEST:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " req packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::GRANT:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " grant packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::SCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " sched data packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getSchedDataFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::UNSCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " unsched data packet at droptail enqueue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getUnschedFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    default:
                        logFile2 << simTime() << "HomaPkt arrived at the queue has unknown type. " << homaPkt->getPktType() << " " << std::endl;
                        logFile2.flush();
                }
            }
        }
    }

    //emit(queueLengthSignal, queue.length());
    //emit(queueByteLengthSignal, queue.getByteLength());
    return NULL;
}

cMessage *DropTailQueue::dequeue()
{
    if (queue.empty())
        return NULL;

    cPacket* pkt = queue.pop();

    if(logPacketEvents){
        cPacket* pktTemp = HomaPkt::searchEncapHomaPkt(pkt);
        if (pktTemp) {
            HomaPkt* homaPkt = check_and_cast<HomaPkt*>(pktTemp);
            cModule* parentHost = this->getParentModule();
            cModule* grandParentHost = parentHost->getParentModule();
            cModule* grandGrandParentHost = grandParentHost->getParentModule();
            if(homaPkt->getMsgId() == 288){
                switch (homaPkt->getPktType()) {
                    case PktType::REQUEST:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " req packet at droptail dequeue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::GRANT:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " grant packet at droptail dequeue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::SCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " sched data packet at droptail dequeue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getSchedDataFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    case PktType::UNSCHED_DATA:
                        logFile2 << simTime() << "len: " << queue.length() << " bytelen: " << queue.getByteLength() << " msg id: " << homaPkt->getMsgId() << " unsched data packet at droptail dequeue: " << parentHost->getName() << " " << grandParentHost->getName() << " " << grandGrandParentHost->getName() << " " << grandGrandParentHost->getIndex() << " " << grandGrandParentHost->getIndex() << " firstbyte: " << homaPkt->getUnschedFields().firstByte << std::endl;
                        logFile2.flush();
                        break;
                    default:
                        logFile2 << simTime() << "HomaPkt leaving the queue has unknown type. " << homaPkt->getPktType() << std::endl;
                        logFile2.flush();
                }
            }
        }
    }

    // statistics
    //emit(queueLengthSignal, queue.length());
    //emit(queueByteLengthSignal, queue.getByteLength());

    return (cMessage *)pkt;
}

void DropTailQueue::sendOut(cMessage *msg)
{
    send(msg, outGate);
}

bool DropTailQueue::isEmpty()
{
    return queue.empty();
}

void
DropTailQueue::setTxPktDuration(int txPktBytes)
{
    double txRate = 0.0; // transmit speed of the next mac layer

    if (txPktBytes == 0) {
        lastTxPktDuration.first = 0;
        lastTxPktDuration.second = simTime();
        return;
    }
    
    lastTxPktDuration.first = txPktBytes + (INTERFRAME_GAP_BITS >> 3) +
            PREAMBLE_BYTES + SFD_BYTES;
    double lastTxBits = 0.0;
    lastTxBits = lastTxPktDuration.first * 8.0;

    if (mac) {
        txRate = dynamic_cast<EtherMACBase*>(mac)->getTxRate();
    }

    if (txRate <= 0.0) {
        lastTxPktDuration.second = simTime();
        return;
    }

    lastTxPktDuration.second = simTime() + lastTxBits / txRate;
    return;
}

cModule*
DropTailQueue::getNextMacLayer()
{
    cModule* parentModule = getParentModule(); 
    if (!parentModule->hasGate("out")) {
        return NULL;
    }

    cGate* parentOutGate = parentModule->gate("out");
    cGate* destGate = parentOutGate->getNextGate();
    cModule* nextModule = destGate->getOwnerModule();

    if (strcmp(nextModule->getName(), "mac") != 0) {
        return NULL;
    }

    return nextModule;

}

} // namespace inet

