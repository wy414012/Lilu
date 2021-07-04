#!/bin/bash

git clone https://github.com/acidanthera/MacKernelSDK.git

xcodebuild -jobs 1 -configuration Debug
xcodebuild -jobs 1 -configuration Release
