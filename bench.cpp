#include <benchmark/benchmark.h>


extern "C" {
    #include "NoFoot.h"
}

static void test1(benchmark::State& state){
    MemoryContext* mc=NULL;
    int n=state.range(0);
    for(auto _ : state){
        user(mc,n);
    }
}

BENCHMARK(test1)->RangeMultiplier(10)->Range(1000,10000000);

BENCHMARK_MAIN();