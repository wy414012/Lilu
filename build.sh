#!/bin/bash

git submodule update --init --recursive

xcodebuild -jobs 1 -configuration Debug
xcodebuild -jobs 1 -configuration Release
