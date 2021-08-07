#include "learning_gem5/part2/hello_object.hh"

#include "base/trace.hh"
#include "debug/HelloExample.hh"

HelloObject::HelloObject(const HelloObjectParams &params) :
    SimObject(params),
        event([this]{processEvent();}, name()),
        latency(100),
        timesLeft(10)
{
    DPRINTF(HelloExample, "Created the hello object");
}

void
HelloObject::processEvent()
{
        timesLeft--;
        DPRINTF(HelloExample, "Hello world! Processing the event!\n");

        if (timesLeft <= 0) {
                DPRINTF(HelloExample, "Done firing!\n");
        } else {
                schedule(event, curTick() + latency);
        }
}

void
HelloObject::startup()
{
        schedule(event, 100);
}
