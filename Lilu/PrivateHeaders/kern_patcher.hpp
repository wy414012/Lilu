//
//  kern_patcher_private.hpp
//  Lilu
//
//  Copyright © 2016-2017 vit9696. All rights reserved.
//

#ifndef kern_patcher_private_h
#define kern_patcher_private_h

#include <Headers/kern_config.hpp>
#include <Headers/kern_util.hpp>

#include <stdint.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <mach/mach_types.h>

// Where are my type_traits :(
template<bool B, class T, class F>
struct conditional { typedef T type; };
template<class T, class F>
struct conditional<false, T, F> { typedef F type; };

namespace Patch {
	enum class Variant {
		U8,
		U16,
		U32,
		U64,
#if defined(__x86_64__)
		U128
#endif
	};

	template <Variant P>
	using VV =
		typename conditional<P == Variant::U8, uint8_t,
		typename conditional<P == Variant::U16, uint16_t,
		typename conditional<P == Variant::U32, uint32_t,
		typename conditional<P == Variant::U64, uint64_t,
#if defined(__x86_64__)
		typename conditional<P == Variant::U128, unsigned __int128,
#endif
		void

#if defined(__x86_64__)
	>::type>::type>::type>::type>::type;
#else
	>::type>::type>::type>::type;
#endif

	template <typename T>
	static void writeType(mach_vm_address_t addr, T value) {
		// It is completely forbidden to IOLog with disabled interrupts as of High Sierra, yet DBGLOG may bypass it if needed.
		*reinterpret_cast<T *>(addr) = value;
	}

	template <Variant T>
	struct P {
		const Variant type {T};
		const mach_vm_address_t address;
		const VV<T> original;
		const VV<T> replaced;
		P(mach_vm_address_t addr, VV<T> rep) :
			address(addr), original(*reinterpret_cast<VV<T> *>(addr)), replaced(rep) {}
		P(mach_vm_address_t addr, VV<T> org, VV<T> rep) :
			address(addr), original(org), replaced(rep) {}
		void patch() {
			writeType(address, replaced);
		}
		void restore() {
			writeType(address, original);
		}
	};

	union All {
		explicit All(P<Variant::U8> &&v) : u8(v) {}
		explicit All(P<Variant::U16> &&v) : u16(v) {}
		explicit All(P<Variant::U32> &&v) : u32(v) {}
		explicit All(P<Variant::U64> &&v) : u64(v) {}
#if defined(__x86_64__)
		explicit All(P<Variant::U128> &&v) : u128(v) {}
#endif

		P<Variant::U8> u8;
		P<Variant::U16> u16;
		P<Variant::U32> u32;
		P<Variant::U64> u64;
#if defined(__x86_64__)
		P<Variant::U128> u128;
#endif

		void patch() {
			switch (u8.type) {
				case Variant::U8: return u8.patch();
				case Variant::U16: return u16.patch();
				case Variant::U32: return u32.patch();
				case Variant::U64: return u64.patch();
#if defined(__x86_64__)
				case Variant::U128: return u128.patch();
#endif
				default: PANIC("patcher", "unsupported patch type %d, cannot patch", static_cast<int>(u8.type));
			}
		}

		void restore() {
			switch (u8.type) {
				case Variant::U8: return u8.restore();
				case Variant::U16: return u16.restore();
				case Variant::U32: return u32.restore();
				case Variant::U64: return u64.restore();
#if defined(__x86_64__)
				case Variant::U128: return u128.restore();
#endif
				default: PANIC("patcher", "unsupported patch type %d, cannot restore", static_cast<int>(u8.type));
			}
		}
	};

	template <Variant T>
	static All *create(mach_vm_address_t addr, VV<T> rep) {
		return new All(P<T>(addr, rep));
	}

	template <Variant T>
	static All *create(mach_vm_address_t addr, VV<T> org, VV<T> rep) {
		return new All(P<T>(addr, org, rep));
	}

	static void deleter(All *i NONNULL) {
		delete i;
	}
}

#ifdef LILU_KEXTPATCH_SUPPORT

/**
 *  Taken from libkern/libkern/OSKextLibPrivate.h
 */
struct OSKextLoadedKextSummaryBase {
	char        name[KMOD_MAX_NAME];
	uuid_t      uuid;
	uint64_t    address;
	uint64_t    size;
	uint64_t    version;
	uint32_t    loadTag;
	uint32_t    flags;
	uint64_t    reference_list;
};

struct OSKextLoadedKextSummaryLegacy {
	OSKextLoadedKextSummaryBase base;
};

struct OSKextLoadedKextSummaryBigSur {
	OSKextLoadedKextSummaryBase base;
	uint64_t                    text_address;
	uint64_t                    text_size;
};

struct OSKextLoadedKextSummaryHeaderBase {
	uint32_t version;
	uint32_t entry_size;
	_Atomic(uint32_t) numSummaries; /* marking as atomic just in case to avoid compiler optimisations */
	uint32_t reserved; /* explicit alignment for gdb  */
};

struct OSKextLoadedKextSummaryHeaderLegacy {
	OSKextLoadedKextSummaryHeaderBase base;
	OSKextLoadedKextSummaryLegacy     summaries[];
};

struct OSKextLoadedKextSummaryHeaderBigSur {
	OSKextLoadedKextSummaryHeaderBase base;
	OSKextLoadedKextSummaryBigSur     summaries[];
};

union OSKextLoadedKextSummaryHeaderAny {
	OSKextLoadedKextSummaryHeaderBase    base;
	OSKextLoadedKextSummaryHeaderLegacy  legacy;
	OSKextLoadedKextSummaryHeaderBigSur  bigSur;
};

#endif /* LILU_KEXTPATCH_SUPPORT */

#endif /* kern_patcher_private_h */
