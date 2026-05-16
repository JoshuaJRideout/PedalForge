#include "source/pedals/PedalRegistry.h"
#include <iostream>

int main() {
    auto proc = GraphPedalFactory::createOverdrive();
    std::cout << proc->saveGraph().toStdString() << std::endl;
    return 0;
}
