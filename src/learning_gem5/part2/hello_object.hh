#ifndef __LEARNING_GEM5_HELLO_OBJECT_HH__
#define __LEARNING_GEM5_HELLO_OBJECT_HH__

#include "params/HelloObject.hh"
#include "sim/sim_object.hh"

class HelloObject : public SimObject
{
  private:
        void processEvent();

        EventFunctionWrapper event;

        const Tick latency;

        int timesLeft;

  public:
    HelloObject(const HelloObjectParams &p);

        void startup();
};

#endif // __LEARNING_GEM5_HELLO_OBJECT_HH__
