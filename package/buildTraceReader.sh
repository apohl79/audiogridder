#!/bin/bash
clang++ -g -std=c++14 -O3 -lboost_program_options -o package/build/tracereader Common/Source/TraceReader.cpp
