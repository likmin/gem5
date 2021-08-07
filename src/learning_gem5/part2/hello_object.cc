#include "learning_gem5/part2/hello_object.hh"

#include "base/trace.hh"
#include "debug/HelloExample.hh"

HelloObject::HelloObject(const HelloObjectParams &params) :
    SimObject(params)
{
    DPRINTF(HelloExample, "Created the hello object");
}
