// Marks a temporary stand-in for a routine whose unit isn't ported yet.
// Stubs are weak symbols, so the real implementation replaces them
// automatically when its unit lands (reunion_core is an object library, so
// every object is always linked). GCC/Clang only; stubs go before release.
#pragma once

#define STUB [[gnu::weak]]
