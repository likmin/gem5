#ifndef __LEARNING_GEM5_PART2_SIMPLE_CACHE_HH__
#define __LEARNING_GEM5_PART2_SIMPLE_CACHE_HH__

#include <unordered_map>

#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/SimpleCache.hh"
#include "sim/clocked_object.hh"

class SimpleCache : public ClockedObject
{
    private:

        class CPUSidePort : public ResponsePort
        {
            private:
                int id;

                SimpleCache *owner;

                bool needRetry;

                PacketPtr blockedPacket;

            public:
                CPUSidePort(const std::string& name, int id,
                    SimpleCache *owner) :
                    ResponsePort(name, owner),
                    id(id),
                    owner(owner),
                    needRetry(false),
                    blockedPacket(nullptr)
                { }

                void sendPacket(PacketPtr pkt);

                AddrRangeList getAddrRanges() const override;

                void trySendRetry();

            protected:
                Tick recvAtomic(PacketPtr pkt) override
                { panic("recvAtomic unimpl."); }

                void recvFunctional(PacketPtr pkt) override;

                bool recvTimingReq(PacketPtr pkt) override;

                void recvRespRetry() override;

        };

        class MemSidePort : public RequestPort
        {
            private:
                SimpleCache *owner;
                PacketPtr blockedPacket;
            public:
                MemSidePort(const std::string& name, SimpleCache *owner) :
                    RequestPort(name, owner), owner(owner),
                    blockedPacket(nullptr)
                { }
                void sendPacket(PacketPtr pkt);
            protected:
                bool recvTimingResp(PacketPtr pkt) override;
                void recvReqRetry() override;
                void recvRangeChange() override;
        };

        bool handleRequest(PacketPtr pkt, int port_id);

        bool handleResponse(PacketPtr pkt);

        void sendResponse(PacketPtr pkt);

        void handleFunctional(PacketPtr pkt);

        void accessTiming(PacketPtr pkt);

        bool accessFunctional(PacketPtr pkt);

        void insert(PacketPtr pkt);

        AddrRangeList getAddrRanges() const;

        void sendRangeChange() const;

        const Cycles latency;

        const unsigned blockSize;

        const unsigned capacity;

        std::vector<CPUSidePort> cpuPorts;

        MemSidePort memPort;

        bool blocked;

        PacketPtr originalPacket;

        int waitingPortId;

        Tick missTime;

        std::unordered_map<Addr, uint8_t*> cacheStore;

        // Cache statistics
    protected:
            struct  SimpleCacheStats : public Stats::Group
            {
                SimpleCacheStats(Stats::Group *parent);
                Stats::Scalar hits;
                Stats::Scalar misses;
                Stats::Histogram missLatency;
                Stats::Formula hitRatio;
            } stats;

    public:
        SimpleCache(const SimpleCacheParams &params);

        Port &getPort(const std::string &if_name,
                    PortID idx=InvalidPortID) override;

        // void regStats() override;
};
// class AccessEvent : public Event
// {
//     private:
//         SimpleCache *cache;
//         PacketPtr pkt;
//     public:
//         AccessEvent(SimpleCache *cache, PacketPtr pkt) :
//             Event(Default_Pri, AutoDelete),
//             cache(cache),
//             pkt(pkt)
//         { }
//         void process() override {
//             cache->accessTiming(pkt);
//         }
// };
#endif // __LEARNING_GEM5_PART2_SIMPLE_CACHE_HH__
