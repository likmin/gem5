class MemSidePort : public MasterPort
{
  private:
    SimpleMemobj *owner;

  public:
    MemSidePort(const std::string& name, SimpleMemobj *owner) :
        MasterPort(name, owner), owner(owner)
    { }

  protected:
    bool recvTimingResp(PacketPtr pkt) override;
    void recvReqRetry() override;
    void recvRangeChange() override;
};