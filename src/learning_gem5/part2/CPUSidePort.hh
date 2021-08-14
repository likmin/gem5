class CPUSidePort : public SlavePort
{
    private:
        SimpleMemobj *owner;

    public:
        CPUSidePort(const std::string &name, SimpleMemobj *owner) :
            SlavePort(name, owner), owner(owner)
        { }

        AddrRangeList getAddrRanges() const override;

    protected:
        Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic unimpl.");}
        void recvFunctional(PacketPtr pkt) override;
        void recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
}