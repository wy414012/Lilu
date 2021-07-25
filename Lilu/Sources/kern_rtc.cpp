//
//  kern_rtc.cpp
//  Lilu
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#include <Headers/kern_rtc.hpp>
#include <Headers/kern_mach.hpp>

bool RTCStorage::init(bool wait) {
	auto matching = IOService::serviceMatching("AppleRTC");
	if (matching) {
		if (wait) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
			rtcSrv = IOService::waitForService(matching);
			if (rtcSrv)
				rtcSrv->retain();
#else
			rtcSrv = IOService::waitForMatchingService(matching);
			matching->release();
#endif
		} else {
			auto rtcIterator = IOService::getMatchingServices(matching);
			if (rtcIterator) {
				rtcSrv = OSDynamicCast(IOService, rtcIterator->getNextObject());
				rtcIterator->release();
			}
			matching->release();
		}
	} else {
		SYSLOG("rtc", "failed to allocate rtc service matching");
	}
	return rtcSrv != nullptr;
}

void RTCStorage::deinit() {
	if (rtcSrv) {
		rtcSrv->release();
		rtcSrv = nullptr;
	}
}

bool RTCStorage::checkExtendedMemory() {
	uint8_t off = 0xB4; /* APPLERTC_POWER_BYTE_PM_ADDR */
	uint8_t dst = 0;
	return read(off, 1, &dst);
}

bool RTCStorage::read(uint64_t off, uint32_t size, uint8_t *buffer) {
	// IOUserClient::externalMethod does not exist on 10.4
	if (!rtcSrv || getKernelVersion() < KernelVersion::Leopard)
		return false;

	IOUserClient *rtcHandler = nullptr;
	auto ret = rtcSrv->newUserClient(current_task(), current_task(), 0x101beef, &rtcHandler);
	if (ret == kIOReturnSuccess) {
		DBGLOG("rtc", "successful rtc read client obtain");
		IOExternalMethodArguments args {};

		args.version = kIOExternalMethodArgumentsCurrentVersion;
		args.selector = 0;
		args.asyncWakePort = MACH_PORT_NULL;
		args.scalarInput = &off;
		args.scalarInputCount = 1;
		args.structureOutput = buffer;
		args.structureOutputSize = size;

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
		auto userExternalMethod = reinterpret_cast<t_UserClientExternalMethod **>(rtcHandler)[0][UserClientExternalMethodIndex];
		ret = userExternalMethod(rtcHandler, 0, &args, 0, 0, 0);
#else
		ret = rtcHandler->externalMethod(0, &args);
#endif
		rtcHandler->release();
		if (ret == kIOReturnSuccess)
			return true;
		SYSLOG("rtc", "rtc read failure %u bytes from %u %X", size, static_cast<uint32_t>(off), ret);
	} else {
		SYSLOG("rtc", "rtc read client obtain failure %X", ret);
	}

	return false;
}

bool RTCStorage::write(uint64_t off, uint32_t size, uint8_t *buffer) {
	// IOUserClient::externalMethod does not exist on 10.4
	if (!rtcSrv || getKernelVersion() < KernelVersion::Leopard)
		return false;

	IOUserClient *rtcHandler = nullptr;
	auto ret = rtcSrv->newUserClient(current_task(), current_task(), 0x101beef, &rtcHandler);
	if (ret == kIOReturnSuccess) {
		DBGLOG("rtc", "successful rtc write client obtain");
		IOExternalMethodArguments args {};

		args.version = kIOExternalMethodArgumentsCurrentVersion;
		args.selector = 1;
		args.asyncWakePort = MACH_PORT_NULL;
		args.scalarInput = &off;
		args.scalarInputCount = 1;
		args.structureInput = buffer;
		args.structureInputSize = size;

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
		auto userExternalMethod = reinterpret_cast<t_UserClientExternalMethod **>(rtcHandler)[0][UserClientExternalMethodIndex];
		ret = userExternalMethod(rtcHandler, 1, &args, 0, 0, 0);
#else
		ret = rtcHandler->externalMethod(1, &args);
#endif
		rtcHandler->release();
		if (ret == kIOReturnSuccess)
			return true;
		SYSLOG("rtc", "rtc write failure %u bytes from %u %X", size, static_cast<uint32_t>(off), ret);
	} else {
		SYSLOG("rtc", "rtc write client obtain failure %X", ret);
	}

	return false;
}

uint8_t RTCStorage::readByte(IOACPIPlatformDevice *dev, uint8_t offset) {
	uint8_t indexPort;
	uint8_t dataPort;
	if (offset < RTC_BANK_SIZE) {
		indexPort  = R_PCH_RTC_INDEX;
		dataPort   = R_PCH_RTC_TARGET;
	} else {
		indexPort  = R_PCH_RTC_EXT_INDEX;
		dataPort   = R_PCH_RTC_EXT_TARGET;
	}
	dev->ioWrite8(indexPort, offset & RTC_DATA_MASK);
	return dev->ioRead8(dataPort);
}

void RTCStorage::writeByte(IOACPIPlatformDevice *dev, uint8_t offset, uint8_t value) {
	uint8_t indexPort;
	uint8_t dataPort;
	if (offset < RTC_BANK_SIZE) {
		indexPort  = R_PCH_RTC_INDEX;
		dataPort   = R_PCH_RTC_TARGET;
	} else {
		indexPort  = R_PCH_RTC_EXT_INDEX;
		dataPort   = R_PCH_RTC_EXT_TARGET;
	}
	dev->ioWrite8(indexPort, offset & RTC_DATA_MASK);
	dev->ioWrite8(dataPort, value);
}

void RTCStorage::readDirect(IOACPIPlatformDevice *rtc, uint8_t off, uint16_t size, uint8_t *buffer, bool introff) {
	bool intrsOn = introff ? MachInfo::setInterrupts(false) : false;
	for (uint16_t i = 0; i < size; i++)
		buffer[i] = readByte(rtc, off + i);
	if (intrsOn)
		MachInfo::setInterrupts(true);
}

void RTCStorage::writeDirect(IOACPIPlatformDevice *rtc, uint8_t off, uint16_t size, uint8_t *buffer, bool updatecrc, bool introff) {
	bool intrsOn = introff ? MachInfo::setInterrupts(false) : false;
	for (uint16_t i = 0; i < size; i++)
		writeByte(rtc, off + i, buffer[i]);

	if (updatecrc) {
		uint16_t checksum = 0;
		for (uint16_t i = APPLERTC_HASHED_ADDR; i < RTC_BANK_SIZE*2; i++) {
			checksum ^= (i == APPLERTC_CHECKSUM_ADDR1 || i == APPLERTC_CHECKSUM_ADDR2) ? 0 : readByte(rtc, i);
			for (size_t j = 0; j < 7; j++) {
				bool even = checksum & 1;
				checksum = (checksum & 0xFFFE) >> 1;
				if (even) checksum ^= 0x2001;
			}
		}
		writeByte(rtc, APPLERTC_CHECKSUM_ADDR1, checksum >> 8);
		writeByte(rtc, APPLERTC_CHECKSUM_ADDR2, checksum & 0xFF);
	}

	if (intrsOn)
		MachInfo::setInterrupts(true);
}
