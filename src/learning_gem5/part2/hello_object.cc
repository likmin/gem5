#include "learning_gem5/part2/hello_object.hh"

#include "base/trace.hh"
#include "debug/HelloExample.hh"
#include "learning_gem5/part2/GoodbyeObject.hh"

HelloObject::HelloObject(const HelloObjectParams &params) :
    SimObject(params),
        event([this]{processEvent();}, name()),
        goodbye(params.goodbye_object),
        myName(params.name),
        latency(params.time_to_wait),
        timesLeft(params.number_of_fires)
{
    DPRINTF(HelloExample, "Created the hello object");
    panic_if(!goodbye, "HelloObject must have a non-null GoodbyeObject");
}

void
HelloObject::processEvent()
{
        timesLeft--;
        DPRINTF(HelloExample, "Hello world! Processing the event! %d left\n",
                timesLeft);

        if (timesLeft <= 0) {
                DPRINTF(HelloExample, "Done firing!\n");
                goodbye->sayGoodbye(myName);
        } else {
                schedule(event, curTick() + latency);
        }
}

void
HelloObject::startup()
{
        schedule(event, 100);
}

