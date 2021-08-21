#include "learning_gem5/simple_cache/SimpleCache.hh"

#include "base/compiler.hh"
#include "base/random.hh"
#include "debug/SimpleCache.hh"
#include "sim/system.hh"

SimpleCache::SimpleCache(const SimpleCacheParams &params):
    ClockedObject(params),
    latency(params.latency),
    blockSize(params.system->cacheLineSize()),
    capacity(params.size / blockSize),
    memPort(params.name + ".mem_side", this),
    blocked(false),
    originalPacket(nullptr),
    waitingPortId(-1),
    stats(this)
{
    for (int i = 0; i < params.port_cpu_side_connection_count; ++i){
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }
}

Port &
SimpleCache::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID,
                "Mem side of siimple not a vector port");
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        return cpuPorts[idx];
    } else {
        return ClockedObject::getPort(if_name, idx);
    }
}

void
SimpleCache::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    DPRINTF(SimpleCache, "Sending %s to CPU\n", pkt->print());

    if (!sendTimingResp(pkt)) {
        DPRINTF(SimpleCache, "Failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
SimpleCache::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
SimpleCache::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        needRetry = false;
        DPRINTF(SimpleCache, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
SimpleCache::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    return owner->handleFunctional(pkt);
}

bool
SimpleCache::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(SimpleCache, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        DPRINTF(SimpleCache, "Request blocked\n");
        needRetry = true;
        return false;
    }

    if (!owner ->handleRequest(pkt, id)) {
        DPRINTF(SimpleCache, "Request failed\n");

        needRetry = true;
        return false;
    } else {
        DPRINTF(SimpleCache, "Request succeeded\n");
        return true;
    }
}


void
SimpleCache::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(SimpleCache, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
SimpleCache::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
SimpleCache::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleResponse(pkt);
}

void
SimpleCache::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
SimpleCache::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}



bool
SimpleCache::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        return false;
    }

    DPRINTF(SimpleCache, "Got request for add %#x\n", pkt->getAddr());

    blocked = true;
    waitingPortId = port_id;

    // schedule(new AccessEvent(this, pkt), clockEdge(latency));
    schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                    name() + ".accessEvent", true),
                                    clockEdge(latency));

    return true;
}

void
SimpleCache::sendResponse(PacketPtr pkt)
{
    int port = waitingPortId;

    blocked = false;
    waitingPortId = -1;

    cpuPorts[port].sendPacket(pkt);
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
}

bool
SimpleCache::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(SimpleCache, "Got response for addr %#x\n", pkt->getAddr());
    insert(pkt);


    stats.missLatency.sample(curTick() - missTime);

    if (originalPacket != nullptr){
        accessFunctional(originalPacket);
        originalPacket->makeResponse();
        delete pkt;
        pkt = originalPacket;
        originalPacket = nullptr;
    } // else, pkt contains the data it needs

    sendResponse(pkt);

    return true;
}

void
SimpleCache::handleFunctional(PacketPtr pkt)
{
    if (accessFunctional(pkt)) {
        pkt->makeResponse();
    } else {
        memPort.sendFunctional(pkt);
    }
}

void
SimpleCache::accessTiming(PacketPtr pkt)
{
    bool hit = accessFunctional(pkt);
    if (hit) {
        stats.hits++; // update stats
        pkt->makeResponse();
        sendResponse(pkt);
    } else {
        stats.misses++; // update stats
        missTime = curTick();
        Addr addr = pkt->getAddr();
        Addr block_addr = pkt->getBlockAddr(blockSize);
        unsigned size = pkt->getSize();
        if (addr == block_addr && size == blockSize) {
            DPRINTF(SimpleCache, "forwarding packet\n");
            memPort.sendPacket(pkt);
        } else {
            DPRINTF(SimpleCache, "Upgrading packet to block size\n");
            panic_if(addr - block_addr + size > blockSize,
                    "Cannot handle accesses that span multiple cache lines");
            assert(pkt->needsResponse());
            MemCmd cmd;
            if (pkt->isWrite() || pkt->isRead()) {
                cmd = MemCmd::ReadReq;
            } else {
                panic("Unknown packet type in upgrade size");
            }

            PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
            new_pkt->allocate();

            originalPacket = pkt;

            memPort.sendPacket(new_pkt);
        }
    }
}

bool
SimpleCache::accessFunctional(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    auto it = cacheStore.find(block_addr);
    if (it != cacheStore.end()) {
        if (pkt->isWrite()) {
            // Write the data into the block in the cache
            pkt->writeDataToBlock(it->second, blockSize);
        } else if (pkt->isRead()) {
            // Read the data out of the cache block into the packet
            pkt->setDataFromBlock(it->second, blockSize);
        } else {
            panic("Unknown packet type!");
        }
        return true;
    }
    return false;
}

void
SimpleCache::insert(PacketPtr pkt)
{
    // The packet should be aligned.
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The address should not be in the cache
    assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
    // The pkt should be a response
    assert(pkt->isResponse());

    if (cacheStore.size() >= capacity) {
        // Select random thing to evict. This is a little convoluted since we
        // are using a std::unordered_map. See http://bit.ly/2hrnLP2
        int bucket, bucket_size;
        do {
            bucket = random_mt.random(0, (int)cacheStore.bucket_count() - 1);
        } while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0 );
        auto block = std::next(cacheStore.begin(bucket),
                               random_mt.random(0, bucket_size - 1));

        DPRINTF(SimpleCache, "Removing addr %#x\n", block->first);

        // Write back the data.
        // Create a new request-packet pair
        RequestPtr req = std::make_shared<Request>(
            block->first, blockSize, 0, 0);

        PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
        new_pkt->dataDynamic(block->second); // This will be deleted later

        DPRINTF(SimpleCache, "Writing packet back %s\n", pkt->print());
        // Send the write to memory
        memPort.sendPacket(new_pkt);

        // Delete this entry
        cacheStore.erase(block->first);
    }

    DPRINTF(SimpleCache, "Inserting %s\n", pkt->print());
    DDUMP(SimpleCache, pkt->getConstPtr<uint8_t>(), blockSize);

    // Allocate space for the cache block data
    uint8_t *data = new uint8_t[blockSize];

    // Insert the data and address into the cache store
    cacheStore[pkt->getAddr()] = data;

    // Write the data into the cache
    pkt->writeDataToBlock(data, blockSize);
}

AddrRangeList
SimpleCache::getAddrRanges() const
{
    DPRINTF(SimpleCache, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
SimpleCache::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

SimpleCache::SimpleCacheStats::SimpleCacheStats(Stats::Group *parent)
      : Stats::Group(parent),
      ADD_STAT(hits, UNIT_COUNT, "Number of hits"),
      ADD_STAT(misses, UNIT_COUNT, "Number of misses"),
      ADD_STAT(missLatency, UNIT_TICK, "Ticks for misses to the cache"),
      ADD_STAT(hitRatio, UNIT_RATIO,
               "The ratio of hits to the total accesses to the cache",
               hits / (hits + misses))
{
    missLatency.init(16); // number of buckets
}

// BaseSlavePort&
// SimpleCache::getSlavePort(const std::string& if_name, PortID idx)
// {
//     if (if_name == "cpu_side" && idx < cpuPorts.size()) {
//         return cpuPorts[idx];
//     } else {
//         return MemObject::getSlavePort(if_name, idx);
//     }
// }

// void
// SimpleCache::regStats()
// {
//     ClockedObject::regStats();
//     stats.hits.name(name() + ".hits")
//         .desc("Num of hits");

//     stats.misses.name(name() + ".misses")
//         .desc("Number of misses");

//     stats.missLatency.name(name() + ".missLatency")
//         .desc("Ticks for misses to the cache")
//         .init(16); // number of buckets
//     stats.hitRatio.name(name() + ".hitRatio")
//         .desc("The ratio of hits to the total accesses to the cache");

//     stats.hitRatio = stats.hits / (stats.hits + stats.misses);
// }
