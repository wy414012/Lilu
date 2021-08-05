#!/bin/bash

git submodule update --init --recursive

git submodule foreach git checkout master

git submodule foreach git pull

xcodebuild -jobs 1 -arch x86_64 -arch ACID32 -configuration Debug
xcodebuild -jobs 1 -arch x86_64 -arch ACID32 -configuration Release
