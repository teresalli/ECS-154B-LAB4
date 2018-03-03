
#include <iostream>

#include "direct_mapped.hh"
#include "set_assoc.hh"
#include "memory.hh"
#include "processor.hh"

int main(int argc, char *argv[])
{
    Processor p;
	Memory m(8);
    //DirectMappedCache c(1 << 10, m, p);
	SetAssociativeCache s(1 << 10, m, p, 1);
    std::cout << "Running simulation" << std::endl;
    TickedObject::runSimulation();
    std::cout << "Simulation done" << std::endl;
}
